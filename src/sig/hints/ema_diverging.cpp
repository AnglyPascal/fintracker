#include "ind/indicators.h"
#include "sig/signals.h"

Hint ema_diverging_hint(const IndicatorsTrends& m, int idx) {
  // EMAs must be in diverging position
  if (m.ema9(idx) <= m.ema21(idx))
    return HintType::None;

  double score = 1.0;

  // Analyze divergence trend
  int diverging_periods = 0;
  double total_divergence_rate = 0.0;
  double max_separation = 0.0;

  for (int i = idx - 6; i <= idx && i > 0; i++) {
    double dist_curr = m.ema9(i) - m.ema21(i);
    double dist_prev = m.ema9(i - 1) - m.ema21(i - 1);

    constexpr double buffer_pct = 0.01;
    if (dist_curr > dist_prev * (1 - buffer_pct) && dist_curr > 0) {
      diverging_periods++;
      double rate = (dist_curr - dist_prev) / m.ema21(i);
      total_divergence_rate += rate;
    }

    if (dist_curr > 0)
      max_separation = std::max(max_separation, dist_curr / m.ema21(i));
  }

  if (diverging_periods == 0)
    return HintType::None;

  // Consistency scoring
  score += (diverging_periods - 3) * 0.15;  // 3-7 periods adds 0-0.6

  // Divergence strength
  double avg_rate = total_divergence_rate / diverging_periods;
  if (avg_rate > 0.003)
    score += 0.3;  // Rapid divergence
  else if (avg_rate < 0.001)
    score -= 0.25;  // Slow divergence

  // Current separation
  if (max_separation > 0.03)
    score += 0.25;  // Wide separation (overextended)
  else if (max_separation < 0.015)
    score -= 0.2;  // Still close

  // Check for acceleration
  double recent_sep = (m.ema9(idx) - m.ema21(idx)) / m.ema21(idx);
  double older_sep = (m.ema9(idx - 3) - m.ema21(idx - 3)) / m.ema21(idx - 3);
  if (recent_sep > older_sep * 1.5)
    score += 0.2;  // Accelerating divergence

  // Overbought context
  if (m.rsi(idx) > 70)
    score += 0.25;
  else if (m.rsi(idx) < 60)
    score -= 0.15;

  // Price momentum weakening
  if (m.price(idx) < m.price(idx - 1))
    score += 0.15;

  return {HintType::Ema9DivergeEma21, std::clamp(score, 0.4, 1.7)};
}
