#include "risk/sizing.h"
#include "core/positions.h"
#include "util/config.h"
#include "util/format.h"
#include "util/math.h"

#include <algorithm>

inline auto& risk_config = config.risk_config;

// Portfolio state calculation
struct PortfolioState {
  int active_positions = 0;
  double total_risk_deployed = 0.0;
  int positions_opened_today = 0;
  std::vector<std::string> current_symbols;

  PortfolioState(const OpenPositions& positions) {
    auto& pos_map = positions.get_positions();
    active_positions = pos_map.size();

    for (auto& [symbol, pos] : pos_map) {
      current_symbols.push_back(symbol);
      // Estimate risk per position (we don't have original stops, so estimate)
      // Assume average 3% stop for existing positions
      double estimated_risk =
          (pos.qty * pos.px * 0.03) / risk_config.capital_usd;
      total_risk_deployed += estimated_risk;
    }

    // TODO: Track positions opened today properly
    // For now, assume we can track this elsewhere
    positions_opened_today = 0;
  }
};

// Pure calculation functions
inline double calculate_correlation_to_spy(const Metrics& m,
                                           const Indicators& spy,
                                           LocalTimePoint tp) {
  const int CORRELATION_PERIOD = 30;
  auto& stock_ind = m.ind_1d;
  auto idx = stock_ind.idx_for_time(tp);

  std::vector<double> stock_returns, spy_returns;
  for (int i = idx - CORRELATION_PERIOD; i < idx; i++) {
    double stock_ret =
        (stock_ind.price(i) - stock_ind.price(i - 1)) / stock_ind.price(i - 1);
    double spy_ret = (spy.price(i) - spy.price(i - 1)) / spy.price(i - 1);
    stock_returns.push_back(stock_ret);
    spy_returns.push_back(spy_ret);
  }

  // Calculate correlation
  double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
  int n = stock_returns.size();

  for (int i = 0; i < n; i++) {
    sum_x += stock_returns[i];
    sum_y += spy_returns[i];
    sum_xy += stock_returns[i] * spy_returns[i];
    sum_x2 += stock_returns[i] * stock_returns[i];
    sum_y2 += spy_returns[i] * spy_returns[i];
  }

  double correlation =
      (n * sum_xy - sum_x * sum_y) / (std::sqrt(n * sum_x2 - sum_x * sum_x) *
                                      std::sqrt(n * sum_y2 - sum_y * sum_y));

  return std::isnan(correlation) ? 0.0 : correlation;
}

inline double calculate_target_risk(const CombinedSignal& signal,
                                    MarketRegime regime) {
  double base_risk = 0.005;

  // Adjust based on signal quality
  if (signal.type == Rating::Entry && signal.score > 0.8)
    base_risk = 0.007;
  else if (signal.type == Rating::Entry && signal.score > 0.6)
    base_risk = 0.006;
  else if (signal.type == Rating::Watchlist && signal.score > 0.6)
    base_risk = 0.004;
  else  // Skip mediocre trades
    return 0.0015;

  // Reduce in difficult market regimes
  if (regime == MarketRegime::CHOPPY_BEARISH)
    base_risk *= 0.7;
  else if (regime == MarketRegime::RANGE_BOUND)
    base_risk *= 0.85;
  else if (regime == MarketRegime::TRENDING_DOWN)
    base_risk *= 0.5;

  return base_risk;
}

inline int get_daily_position_limit(MarketRegime regime) {
  switch (regime) {
    case MarketRegime::TRENDING_UP:
      return 3;
    case MarketRegime::CHOPPY_BULLISH:
      return 2;
    case MarketRegime::RANGE_BOUND:
      return 1;
    case MarketRegime::CHOPPY_BEARISH:
      return 1;
    case MarketRegime::TRENDING_DOWN:
      return 0;
  }
  return 1;
}

inline Recommendation determine_recommendation(double position_risk_pct) {
  if (position_risk_pct >= 0.007)
    return Recommendation::StrongBuy;
  else if (position_risk_pct >= 0.005)
    return Recommendation::Buy;
  else if (position_risk_pct >= 0.003)
    return Recommendation::WeakBuy;
  else
    return Recommendation::Avoid;
}

PositionSizing::PositionSizing(  //
    const Metrics& m,
    const Indicators& spy,
    LocalTimePoint tp,
    const CombinedSignal& signal,
    const StopLoss& stop_loss,
    const OpenPositions& positions,
    MarketRegime regime  //
) {
  std::vector<fmt_string> reasons;
  std::vector<fmt_string> adjustments;
  std::vector<fmt_string> details;
  auto sep = tagged(" | ", color_of("comment"));

  PortfolioState portfolio{positions};
  double current_price = m.last_price();

  // Get basic metrics
  auto idx_1d = m.ind_1d.idx_for_time(tp);
  daily_atr_pct = m.ind_1d.atr(idx_1d) / current_price;
  correlation_to_spy = calculate_correlation_to_spy(m, spy, tp);

  // Check daily limit first
  int daily_limit = get_daily_position_limit(regime);
  if (portfolio.positions_opened_today >= daily_limit) {
    hit_daily_limit = true;
    rec = Recommendation::Avoid;

    reasons.emplace_back(tagged("daily limit hit", color_of("bad"), BOLD));
    details.emplace_back(
        "{}/{} positions today",
        tagged(portfolio.positions_opened_today, color_of("bad")),
        tagged(daily_limit, color_of("comment")));

    rationale = std::format("{}<br><div class=\"rationale-details\">{}</div>",
                            join(reasons.begin(), reasons.end(), sep),
                            join(details.begin(), details.end(), sep));
    return;
  }

  // Calculate base target risk
  double target_risk = calculate_target_risk(signal, regime);
  if (target_risk < 0.002) {
    rec = Recommendation::Avoid;
    reasons.emplace_back(tagged("weak signal", color_of("comment")));
    details.emplace_back("target risk {}% too low",
                         tagged(target_risk * 100, color_of("comment")));
  }

  // Apply risk adjustments
  double risk_multiplier = 1.0;

  // Correlation adjustment
  if (correlation_to_spy > 0.8) {
    risk_multiplier *= 0.8;
    was_reduced_for_correlation = true;
    adjustments.emplace_back("high SPY corr {}",
                             tagged(correlation_to_spy, color_of("semi-bad")));
  } else if (correlation_to_spy < -0.3) {
    risk_multiplier *= 1.1;  // Defensive hedge bonus
    adjustments.emplace_back("defensive hedge {}",
                             tagged(correlation_to_spy, "green"));
  } else {
    adjustments.emplace_back("SPY corr {}",
                             tagged(correlation_to_spy, "slate"));
  }

  // Volatility adjustment
  if (daily_atr_pct > 0.05) {
    risk_multiplier *= 0.7;
    was_reduced_for_volatility = true;
    adjustments.emplace_back("high vol {}%",
                             tagged(daily_atr_pct * 100, "yellow"));
  } else if (daily_atr_pct > 0.04) {
    risk_multiplier *= 0.85;
    was_reduced_for_volatility = true;
    adjustments.emplace_back("elevated vol {}%",
                             tagged(daily_atr_pct * 100, "coral"));
  } else {
    adjustments.emplace_back("{}% vol", tagged(daily_atr_pct * 100, "sage"));
  }

  // Calculate adjusted risk
  double adjusted_risk = target_risk * risk_multiplier;
  double new_total_risk = portfolio.total_risk_deployed + adjusted_risk;

  // Portfolio limit check
  if (new_total_risk > risk_config.MAX_PORTFOLIO_RISK) {
    double available_risk =
        risk_config.MAX_PORTFOLIO_RISK - portfolio.total_risk_deployed;
    if (available_risk > 0.002) {  // At least 0.2% available
      adjusted_risk = available_risk;
      was_reduced_for_portfolio_limit = true;
      adjustments.emplace_back("Portfolio limit, using {}%",
                               tagged(available_risk * 100, "yellow"));
    } else {
      rec = Recommendation::Avoid;
      reasons.emplace_back(tagged("portfolio risk full", "red", BOLD));
      details.emplace_back("{}% deployed",
                           tagged(portfolio.total_risk_deployed * 100, "red"));
      rationale = std::format("{}<br><div class=\"rationale-details\">{}</div>",
                              join(reasons.begin(), reasons.end(), sep),
                              join(details.begin(), details.end(), sep));
      return;
    }
  }

  // Position concentration check
  if (portfolio.active_positions >= 10) {
    risk_multiplier *= 0.8;
    adjustments.emplace_back("{} positions open",
                             tagged(portfolio.active_positions, "amber"));
  }

  // Calculate final position
  position_risk_pct =
      std::min(adjusted_risk, risk_config.MAX_RISK_PER_POSITION);
  position_risk_amount = risk_config.capital_usd * position_risk_pct;

  double stop_distance = stop_loss.get_stop_distance(current_price);
  rec_shares = round(position_risk_amount / stop_distance, 2);
  rec_capital = rec_shares * current_price;

  portfolio_risk_after = portfolio.total_risk_deployed + position_risk_pct;

  // Determine recommendation level
  rec = determine_recommendation(position_risk_pct);

  // Build rationale
  std::string rec_str =  //
      rec == Recommendation::StrongBuy ? tagged(rec, "green", BOLD)
      : rec == Recommendation::Buy     ? tagged(rec, "green")
      : rec == Recommendation::WeakBuy ? tagged(rec, "sage")
                                       : tagged(rec, "gray");

  reasons.emplace_back(rec_str);
  reasons.emplace_back("{}% risk", tagged(position_risk_pct * 100, "coral"));
  reasons.emplace_back("{} w/ ${}", tagged(rec_shares, "teal"),
                       tagged(rec_capital, "teal"));

  // Stop context
  details.emplace_back("stop distance ${}", tagged(stop_distance, "blue"));
  details.emplace_back("risk per share ${}",
                       tagged(position_risk_amount / rec_shares, "coral"));

  details.emplace_back("signal: {} ({})",  //
                       tagged(signal.type, "blue"),
                       tagged(signal.score, "blue"));

  details.emplace_back("market: {}", tagged(regime, "slate"));

  // Portfolio context
  auto portfolio_str =
      std::format("portfolio: {}/12 positions, {}% -> {}% risk",
                  tagged(portfolio.active_positions, "frost"),
                  tagged(portfolio.total_risk_deployed * 100, "frost"),
                  tagged(portfolio_risk_after * 100, "frost"));

  // Build final rationale
  if (!adjustments.empty()) {
    constexpr auto templ = R"(
      {}
      <div class=\"rationale-details\">{} | {}</div>
      <div class=\"rationale-details\">{}</div>
    )";
    rationale = std::format(                                //
        templ,                                              //
        join(reasons.begin(), reasons.end(), sep),          //
        portfolio_str,                                      //
        join(adjustments.begin(), adjustments.end(), sep),  //
        join(details.begin(), details.end(), sep)           //
    );
  } else {
    constexpr auto templ = R"(
      {}
      <div class=\"rationale-details\">{} | {}</div>
    )";
    rationale = std::format(                                      //
        templ,                                                    //
        join(reasons.begin(), reasons.end(), sep),                //
        portfolio_str, join(details.begin(), details.end(), sep)  //
    );
  }
}
