#include "ind/indicators.h"
#include "sig/signals.h"

Hint macd_histogram_rising_hint(const IndicatorsTrends& ind, int idx) {
  // Must be below zero and rising
  if (ind.hist(idx) >= 0 || ind.hist(idx) <= ind.hist(idx - 1))
    return HintType::None;

  double score = 1.0;

  // Analyze histogram trend
  int rising_periods = 0;
  double total_rise = 0.0;
  double deepest_hist = ind.hist(idx);
  bool consistent_rise = true;

  for (int i = idx - 5; i <= idx && i > 0; i++) {
    if (ind.hist(i) > ind.hist(i - 1)) {
      rising_periods++;
      total_rise += (ind.hist(i) - ind.hist(i - 1));
    } else if (i > idx - 3) {
      consistent_rise = false;  // Recent inconsistency
    }
    deepest_hist = std::min(deepest_hist, ind.hist(i - 1));
  }

  if (rising_periods < 2)
    return HintType::None;

  // Consistency bonus
  if (consistent_rise && rising_periods >= 3)
    score += 0.35;  // Very consistent rise
  else if (rising_periods >= 4)
    score += 0.2;  // Generally rising

  // Strength of rise
  double avg_rise = total_rise / rising_periods;
  if (avg_rise > 0.0008)
    score += 0.3;  // Strong rise
  else if (avg_rise < 0.0003)
    score -= 0.25;  // Weak rise

  // Recovery from deep negative
  if (deepest_hist < -0.003)
    score += 0.25;  // Strong recovery
  else if (deepest_hist < -0.002)
    score += 0.15;  // Good recovery

  // Distance to zero line (crossing potential)
  double dist_to_zero = std::abs(ind.hist(idx));
  if (dist_to_zero < 0.0005)
    score += 0.2;  // About to cross
  else if (dist_to_zero > 0.002)
    score -= 0.2;  // Still far from zero

  // Check if MACD line is also improving
  if (ind.macd(idx) > ind.macd(idx - 2))
    score += 0.15;

  // Check for acceleration
  double recent_change = ind.hist(idx) - ind.hist(idx - 2);
  double older_change = ind.hist(idx - 2) - ind.hist(idx - 4);
  if (recent_change > older_change * 1.3)
    score += 0.15;  // Accelerating

  return {HintType::MacdRising, std::clamp(score, 0.4, 1.6)};
}
