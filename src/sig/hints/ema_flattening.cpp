#include "ind/indicators.h"
#include "sig/signals.h"

Hint ema_flattens_hint(const IndicatorsTrends& m, int idx) {
  double score = 1.0;

  // Analyze EMA9 slope over multiple periods
  std::vector<double> slopes;
  for (int i = idx - 5; i <= idx && i > 0; i++)
    slopes.push_back((m.ema9(i) - m.ema9(i - 1)) / m.ema9(i));
  if (slopes.empty())
    return HintType::None;

  // Calculate average and max slope
  double avg_slope = 0;
  double max_slope = 0;
  for (double s : slopes) {
    avg_slope += std::abs(s);
    max_slope = std::max(max_slope, std::abs(s));
  }
  avg_slope /= slopes.size();

  // More lenient slope thresholds
  if (avg_slope > 0.004 || max_slope > 0.007)
    return HintType::None;

  // Check how long it's been flat (adjusted for wider range)
  int flat_periods = 0;
  for (double s : slopes)
    if (std::abs(s) < 0.003)
      flat_periods++;

  score += (flat_periods - 3) * 0.2;  // 3-6 periods adds 0-0.6

  // Adjust scoring for different flatness levels
  if (avg_slope < 0.001)
    score += 0.3;  // Very flat
  else if (avg_slope < 0.002)
    score += 0.15;  // Moderately flat
  else if (avg_slope > 0.003)
    score -= 0.2;  // Borderline flat

  // Slope trend (is it getting flatter?)
  double recent_avg =
      (std::abs(slopes.back()) + std::abs(slopes[slopes.size() - 2])) / 2;
  double older_avg = (std::abs(slopes[0]) + std::abs(slopes[1])) / 2;
  if (recent_avg < older_avg * 0.5)
    score += 0.25;  // Increasingly flat
  else if (recent_avg < older_avg * 0.75)
    score += 0.1;  // Somewhat flattening

  // Distance between EMAs (slightly more lenient)
  double ema_dist = std::abs(m.ema9(idx) - m.ema21(idx)) / m.ema21(idx);
  if (ema_dist < 0.007)
    score += 0.2;  // Very close
  else if (ema_dist < 0.012)
    score += 0.05;  // Reasonably close
  else if (ema_dist > 0.02)
    return HintType::None;  // Too far apart to matter

  // Price position
  if (m.price(idx) < m.ema9(idx))
    score += 0.2;  // Price below flat EMAs (bearish)

  // Momentum context
  if (m.rsi(idx) < 50)
    score += 0.15;
  else if (m.rsi(idx) > 60)
    score -= 0.1;  // High RSI reduces flattening concern

  // Volume declining (consolidation)
  double recent_vol = (m.volume(idx) + m.volume(idx - 1)) / 2.0;
  double older_vol = (m.volume(idx - 3) + m.volume(idx - 4)) / 2.0;
  if (recent_vol < older_vol * 0.8)
    score += 0.1;  // Declining volume

  return {HintType::Ema9Flattening, std::clamp(score, 0.4, 1.5)};
}
