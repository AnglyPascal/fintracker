#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Reason broke_support_exit(const IndicatorsTrends& ind, int idx) {
  double current_close = ind.price(idx);
  double prev_close = ind.price(idx - 1);
  std::vector<std::string> reasons;

  // Get the nearest support above current price (the one we just broke)
  auto support_above = ind.nearest_support_above(idx);
  if (!support_above)
    return ReasonType::None;

  const auto& zone = support_above->get();

  // Check if we just broke below
  if (prev_close < zone.lo * 0.998)  // Already below
    return ReasonType::None;

  // Dynamic break threshold using ATR
  double atr = ind.atr(idx);
  double price_normalized_atr = atr / current_close;
  double min_break = zone.lo * (1 - price_normalized_atr * 0.3);
  if (current_close > min_break)
    return ReasonType::None;

  reasons.push_back(std::format("zone [{:.2f}, {:.2f}]", zone.lo, zone.hi));

  // START WITH URGENCY - exits should be urgent
  double score = 1.2;

  // Zone strength makes break MORE serious
  if (zone.is_strong()) {
    score += 0.4;
    reasons.push_back("strong support broken");
  } else {
    score -= 0.2;
    reasons.push_back("weak support broken");
  }

  // Break magnitude relative to ATR
  double break_magnitude = (zone.lo - current_close) / atr;
  if (break_magnitude > 0.5) {
    score += 0.3;
    reasons.push_back("decisive break");
  } else if (break_magnitude < 0.2) {
    score -= 0.4;
    reasons.push_back("marginal break");
  }

  // Acceleration check
  if (idx >= 2) {
    double prev_change = std::abs(ind.price(idx - 2) - ind.price(idx - 1));
    double curr_change = std::abs(prev_close - current_close);
    if (curr_change > prev_change * 1.5) {
      score += 0.3;
      reasons.push_back("accelerating");
    }
  }

  // RSI oversold check
  if (ind.rsi(idx) < 30) {
    score -= 0.2;
    reasons.push_back("oversold");
  } else if (ind.rsi(idx) > 40 && ind.rsi(idx) < 50) {
    score += 0.2;
    reasons.push_back("room to fall");
  }

  // Check if this is a retest of previously broken support
  bool is_retest = false;
  for (int i = idx - 20; i < idx - 5; i++)
    if (i >= 0 && ind.low(i) < zone.lo * 0.99) {
      is_retest = true;
      break;
    }

  if (is_retest) {
    score += 0.3;
    reasons.push_back("failed retest");
  }

  // Next support check
  auto next_support = ind.nearest_support_below(idx);
  if (!next_support) {
    score += 0.2;
    reasons.push_back("no support below");
  } else {
    auto next_zone = next_support->get();
    double fall_potential = (current_close - next_zone.hi) / current_close;
    if (fall_potential > 0.05) {
      score += 0.2;
      reasons.push_back("significant downside");
    } else if (fall_potential < 0.02) {
      score -= 0.3;
      reasons.push_back("limited downside");
    }
    reasons.push_back(
        std::format("next ~{:.2f}", (next_zone.lo + next_zone.hi) / 2));
  }

  // Failed bounce attempts
  int bounce_attempts = 0;
  for (int i = idx - 5; i < idx; i++)
    if (i > 0 && ind.low(i) < zone.lo && ind.close(i) > zone.lo)
      bounce_attempts++;

  if (bounce_attempts >= 2) {
    score += 0.3;
    reasons.push_back(std::format("{}+ failed bounces", bounce_attempts));
  }

  // Timeframe adjustments
  if (ind.interval == H_1)
    score *= 0.8;
  else if (ind.interval == D_1)
    score *= 1.3;

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {ReasonType::BrokeSupport, std::clamp(score, 0.4, 2.0), desc};
}
