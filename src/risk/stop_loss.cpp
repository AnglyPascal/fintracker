#include "risk/stop_loss.h"

#include "util/config.h"
#include "util/format.h"

#include <algorithm>

inline auto& risk_config = config.risk_config;

double StopLoss::calculate_atr_multiplier(double daily_atr_pct) const {
  // Much tighter multipliers
  if (daily_atr_pct > 0.04)
    return 2.3;
  if (daily_atr_pct > 0.03)
    return 2.05;
  if (daily_atr_pct > 0.02)
    return 1.8;
  return 1.6;
}

double StopLoss::calculate_support_buffer(double daily_atr_pct) const {
  if (daily_atr_pct > 0.03)
    return 0.015;
  if (daily_atr_pct > 0.02)
    return 0.012;
  return 0.01;
}

double StopLoss::calculate_regime_adjustment(MarketRegime regime) const {
  switch (regime) {
    case MarketRegime::TRENDING_UP:
      return 1.0;
    case MarketRegime::CHOPPY_BULLISH:
      return 1.15;
    case MarketRegime::RANGE_BOUND:
      return 1.2;
    case MarketRegime::CHOPPY_BEARISH:
      return 1.15;
    case MarketRegime::TRENDING_DOWN:
      return 1.05;
  }
  return 1.0;
}

double StopLoss::calculate_time_adjustment(int days_held) const {
  if (days_held == 0)
    return 1.1;
  if (days_held == 1)
    return 1.07;
  if (days_held == 2)
    return 1.05;
  return 1.0;
}

double StopLoss::calculate_trailing_adjustment(int days_to_1r) const {
  if (days_to_1r <= 3)
    return 0.7;
  else if (days_to_1r <= 7)
    return 0.85;
  else
    return 1.0;
}

StopLoss::StopLoss(const Metrics& m,
                   LocalTimePoint tp,
                   MarketRegime regime,
                   int days_held) noexcept
    : days_held(days_held),
      regime(regime)  //
{
  auto& ind_1h = m.ind_1h;
  auto& ind_1d = m.ind_1d;

  auto idx_1h = ind_1h.idx_for_time(tp);
  auto idx_1d = ind_1d.idx_for_time(tp);

  double price = ind_1h.price(idx_1h);
  double daily_atr = ind_1d.atr(idx_1d);
  double daily_atr_pct = daily_atr / price;

  std::deque<fmt_string> reasons;
  std::vector<fmt_string> details;

  // For existing positions
  auto pos = m.position;
  entry_price = m.has_position() ? pos->px : price;
  max_price_seen = m.has_position() ? pos->max_price_seen : price;

  // Calculate ATR-based stop
  atr_multiplier = calculate_atr_multiplier(daily_atr_pct);
  double regime_adj = calculate_regime_adjustment(regime);
  double effective_multiplier = atr_multiplier * regime_adj;
  atr_stop = price - (daily_atr * effective_multiplier);

  details.emplace_back("atr stop: {:.2f} ({} x {} atr)", atr_stop,
                       tagged(atr_multiplier, "amethyst"),
                       tagged(daily_atr, "amethyst"));

  // Check for support-based stop
  support_stop = 0.0;
  auto support_1d = ind_1d.nearest_support_below(idx_1d);
  if (support_1d) {
    double buffer = calculate_support_buffer(daily_atr_pct);
    support_stop = support_1d->get().lo * (1.0 - buffer);

    details.emplace_back("support: {:.2f} (-{}% buffer)",  //
                         support_stop, tagged(buffer * 100, "arctic-teal"));

    bool support_too_tight = support_stop > price * 0.97;
    bool support_too_far = support_stop < atr_stop * 0.85;

    if (support_too_tight) {
      support_stop = 0.0;  // Don't use it
      reasons.emplace_back("{}, tight support", tagged("using atr", "yellow"));
    } else if (support_too_far) {
      support_stop = 0.0;
      reasons.emplace_back("{}, support too far", tagged("using atr", "coral"));
    } else {
      reasons.push_back(tagged("using support", "arctic-teal", BOLD));
    }
  }

  standard_stop = std::max(atr_stop, support_stop);

  double time_adj = calculate_time_adjustment(days_held);
  stop_distance = entry_price - standard_stop;
  initial_stop = entry_price - stop_distance * time_adj;

  current_stop = initial_stop;

  // Check for trailing stop activation
  if (m.has_position()) {
    double profit_pct = (max_price_seen - entry_price) / entry_price;
    double r_achieved =
        profit_pct / (entry_price - standard_stop) * entry_price;

    if (r_achieved >= 1.0) {  // 1R profit achieved
      is_trailing = true;
      trailing_activation_price = entry_price + (entry_price - standard_stop);

      // Determine trailing tightness based on velocity
      double days_to_1r = days_held;  // Approximate
      double trailing_multiplier = calculate_trailing_adjustment(days_to_1r);

      if (days_to_1r <= 3)
        reasons.emplace_back(tagged("tight trailing", "green", BOLD));
      else if (days_to_1r <= 7)
        reasons.emplace_back(tagged("moderate trailing", "green"));
      else
        reasons.emplace_back(tagged("loose trailing", "sage"));

      // Calculate trailing stop
      double trailing_distance =
          (daily_atr * atr_multiplier) * trailing_multiplier;
      trailing_stop = max_price_seen - trailing_distance;
      current_stop = std::max(trailing_stop, entry_price);

      details.emplace_back("trailing from {:.2f} at {:.1f}x", max_price_seen,
                           trailing_multiplier);
    }

    auto risk = (entry_price - current_stop) * m.position->qty;
    auto risk_pct = risk / entry_price;
    if (risk_pct > risk_config.MAX_RISK_PER_POSITION) {
      details.emplace_back("capped at risk {} ({}%)", tagged(risk, "red", BOLD),
                           tagged("risk_pct", "yellow"));
      risk = risk_config.MAX_RISK_PER_POSITION * entry_price;
      current_stop = entry_price - risk;
      reasons.emplace_back("stop risk {}", tagged(current_stop, "red"));
    }
  }

  // Calculate final metrics
  stop_distance = price - current_stop;
  stop_pct = stop_distance / price;

  // Build rationale
  if (is_trailing)
    reasons.emplace_front(tagged("trailing", "sage", BOLD));
  else if (days_held >= 0 && days_held <= 2)
    reasons.emplace_front(tagged(std::format("initial (day {})", days_held + 1),
                                 "champagne", BOLD));
  else
    reasons.emplace_front(tagged("standard", "frost", BOLD));

  reasons.push_back(tagged(regime, "blue"));
  if (regime_adj > 1.0)
    details.emplace_back("regime adj {}%",
                         tagged((int)((regime_adj - 1.0) * 100), "coral"));

  details.emplace_back("{}% daily vol", tagged(daily_atr_pct * 100, "rose"));

  reasons.emplace_back("final: {} (-{}%)", tagged(current_stop, "blue", BOLD),
                       tagged(stop_pct * 100, "blue"));

  rationale =
      fmt_string{"{}<br><div class=\"rationale-details\">{}</div>",
                 join(reasons.begin(), reasons.end(), tagged(" | ", "gray")),
                 join(details.begin(), details.end(), tagged(" | ", "gray"))};
}
