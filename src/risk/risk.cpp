#include "risk/risk.h"
#include "ind/calendar.h"
#include "util/config.h"
#include "util/format.h"

inline auto& risk_config = config.risk_config;

bool Risk::check_earnings_proximity(const Event& event) const {
  if (!event.is_earnings())
    return false;

  int days_until = event.days_until();
  return days_until >= 0 && days_until <= risk_config.earnings_buffer_days;
}

Risk::Risk(const Metrics& m,
           const Indicators& spy,
           LocalTimePoint tp,
           const CombinedSignal& signal,
           const OpenPositions& positions,
           const Event& next_event) noexcept {
  std::vector<fmt_string> checks;
  std::vector<fmt_string> decision;

  auto spy_idx = spy.idx_for_time(tp);
  regime = detect_market_regime(spy, spy_idx);
  checks.emplace_back("{} market", tagged(regime, color_of(regime), BOLD));

  is_near_earnings = check_earnings_proximity(next_event);
  if (is_near_earnings) {
    checks.emplace_back(
        "{} days until earnings",
        tagged(next_event.days_until(), color_of("semi-bad"), BOLD));
    should_take_trade = false;
  }

  stop_loss = StopLoss{m, tp, regime};
  sizing = PositionSizing{m, spy, tp, signal, stop_loss, positions, regime};
  target = ProfitTarget(m, tp, signal, stop_loss);
  scaling = ScalingRules(m, tp, stop_loss, sizing);

  should_take_trade = !is_near_earnings &&
                      sizing.rec != Recommendation::Avoid &&
                      target.risk_reward_ratio >= risk_config.min_rr_ratio &&
                      regime != MarketRegime::TRENDING_DOWN;

  if (should_take_trade) {
    decision.emplace_back(tagged("TAKE TRADE", color_of("wishful"), BOLD));
  } else {
    decision.emplace_back(tagged("SKIP", color_of("semi-bad"), BOLD));

    if (is_near_earnings)
      decision.emplace_back(tagged("Earnings too close", color_of("semi-bad")));
    if (sizing.rec == Recommendation::Avoid)
      decision.emplace_back(tagged("Position sizing rejected"),
                            color_of("caution"));
    if (target.risk_reward_ratio < risk_config.min_rr_ratio)
      decision.emplace_back("R:R too low ({:.1f})", target.risk_reward_ratio);
    if (regime == MarketRegime::TRENDING_DOWN)
      decision.emplace_back(
          tagged("Market trending down", color_of("bad"), IT));
  }

  checks.emplace_back("Stop: {:.2f} (-{:.1f}%)", stop_loss.get_stop_price(),
                      stop_loss.get_stop_percentage(m.last_price()) * 100);
  checks.emplace_back("Risk: {}%",
                      tagged(sizing.position_risk_pct * 100, color_of("risk")));
  checks.emplace_back("Target: {:.2f} ({:.1f}:1)", target.target_price,
                      target.risk_reward_ratio);

  constexpr auto rationale_templ = R"(
      <div class="risk-summary">{}</div>
      <div class="risk-checks rationale-details">{}</div>
      <div class="risk-components">
        <ul>
        <li>{}: {}</li>
        <li>{}: {}</li>
        <li>{}: {}</li>
        <li>{}: {}</li>
        </ul>
      </div>
  )";

  overall_rationale = fmt_string{
      rationale_templ,
      join(decision.begin(), decision.end(), " | "),  //
      join(checks.begin(), checks.end(), " | "),      //
      tagged("Stop", BOLD),                           //
      stop_loss.rationale,                            //
      tagged("Size", BOLD),                           //
      sizing.rationale,                               //
      tagged("Target", BOLD),                         //
      target.rationale,                               //
      tagged("Scale", BOLD),                          //
      scaling.rationale                               //
  };
}

MarketRegime Risk::detect_market_regime(const Indicators& spy, int idx) const {
  double ema21_slope = spy.ema21_trend(idx).slope();
  double price_trend_r2 = spy.price_trend(idx).r2;

  double price = spy.price(idx);
  double ema21 = spy.ema21(idx);
  double ema50 = spy.ema50(idx);
  double price_vs_ema21 = (price - ema21) / ema21;
  double price_vs_ema50 = (price - ema50) / ema50;

  double atr = spy.atr(idx);
  double atr_pct = atr / price;

  double avg_atr = 0;
  for (int i = idx - 20; i < idx; i++)
    avg_atr += spy.atr(i);
  avg_atr /= 20;
  double avg_atr_pct = avg_atr / spy.price(-10);  // Midpoint price

  bool high_volatility = atr_pct > avg_atr_pct * 1.3;

  // Strong uptrend: clear trend, low volatility, price above MAs
  if (ema21_slope > 0.001 && price_trend_r2 > 0.7 && price_vs_ema21 > 0.01 &&
      price_vs_ema50 > 0.02 && ema21 > ema50 && !high_volatility) {
    return MarketRegime::TRENDING_UP;
  }

  // Choppy bullish: upward bias but volatile, price mostly above MAs
  if (ema21_slope > 0 && price_vs_ema50 > 0 && high_volatility) {
    return MarketRegime::CHOPPY_BULLISH;
  }

  // Strong downtrend: clear down, price below both MAs
  if (ema21_slope < -0.001 && price_vs_ema21 < -0.01 &&
      price_vs_ema50 < -0.02) {
    return MarketRegime::TRENDING_DOWN;
  }

  // Choppy bearish: down bias, high volatility, price struggling with MAs
  if (ema21_slope < 0 && price_vs_ema21 < 0 && high_volatility) {
    return MarketRegime::CHOPPY_BEARISH;
  }

  // Default to range-bound if no clear pattern
  return MarketRegime::RANGE_BOUND;
}
