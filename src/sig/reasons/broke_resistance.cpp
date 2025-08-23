#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Reason broke_resistance_entry(const IndicatorsTrends& ind, int idx) {
  double current_close = ind.price(idx);
  double prev_close = ind.price(idx - 1);
  std::vector<std::string> reasons;

  // Get the nearest resistance that we're potentially breaking
  auto resistance_below = ind.nearest_resistance_below(idx);
  if (!resistance_below)
    return ReasonType::None;

  const auto& zone = resistance_below->get();

  // Check if we were below/at resistance and now broke above
  if (prev_close > zone.hi * 1.002)  // Already above
    return ReasonType::None;

  // Dynamic breakout threshold based on ATR
  double atr = ind.atr(idx);
  double price_normalized_atr = atr / current_close;
  double min_breakout = zone.hi * (1 + price_normalized_atr / 2);
  if (current_close < min_breakout)
    return ReasonType::None;

  reasons.push_back(std::format("zone [{:.2f}, {:.2f}]", zone.lo, zone.hi));

  // Base score on zone strength
  double score = zone.conf;
  if (zone.conf >= 0.8)
    reasons.push_back("strong zone");
  else if (zone.conf >= 0.6)
    reasons.push_back("medium zone");
  else
    reasons.push_back("weak zone");

  // Breakout magnitude relative to ATR
  double breakout_magnitude = (current_close - zone.hi) / atr;
  if (breakout_magnitude > 1.0) {
    score += 0.3;
    reasons.push_back("strong breakout");
  } else if (breakout_magnitude < 0.3) {
    score -= 0.2;
    reasons.push_back("weak breakout");
  }

  // RSI momentum check
  if (ind.rsi(idx) > 50 && ind.rsi(idx) < 70) {
    score += 0.1;
    reasons.push_back("good rsi");
  } else if (ind.rsi(idx) >= 70) {
    score -= 0.2;
    reasons.push_back("overbought");
  }

  // Check how long we've been testing this resistance
  int touches = 0;
  for (int i = idx - 10; i < idx; i++)
    if (i >= 0 && std::abs(ind.high(i) - zone.hi) / zone.hi < 0.005)
      touches++;

  if (touches >= 3) {
    score += 0.2;
    reasons.push_back(std::format("{}+ tests", touches));
  } else if (touches == 0) {
    score -= 0.1;
    reasons.push_back("first touch");
  }

  // Check if there's room to run (next resistance)
  auto next_resistance = ind.nearest_resistance_above(idx);
  if (next_resistance) {
    auto next_zone = next_resistance->get();
    double room_to_run = (next_zone.lo - current_close) / current_close;
    if (room_to_run < 0.02) {
      score -= 0.2;
      reasons.push_back("limited upside");
    } else if (room_to_run > 0.05) {
      score += 0.1;
      reasons.push_back("good upside");
    }
    reasons.push_back(
        std::format("next ~{:.2f}", (next_zone.lo + next_zone.hi) / 2));
  } else {
    reasons.push_back("clear above");
  }

  // Timeframe-specific adjustments
  if (ind.interval == H_1)
    score *= 0.9;
  else if (ind.interval == D_1)
    score *= 1.1;

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {ReasonType::BrokeResistance, std::clamp(score, 0.3, 1.5), desc};
}
