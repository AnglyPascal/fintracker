#include "ind/indicators.h"
#include "sig/signals.h"

Hint ema_converging_hint(const IndicatorsTrends& m, int idx) {
  // Check if EMAs are in converging configuration
  if (m.ema9(idx) >= m.ema21(idx))
    return HintType::None;  // Not in position for convergence

  double score = 1.0;

  // Look at convergence trend over last 5-8 candles
  int convergence_periods = 0;
  double total_convergence_rate = 0.0;
  double max_convergence_rate = 0.0;

  for (int i = idx - 7; i <= idx && i > 0; i++) {
    double dist_curr = std::abs(m.ema9(i) - m.ema21(i));
    double dist_prev = std::abs(m.ema9(i - 1) - m.ema21(i - 1));

    constexpr double buffer_pct = 0.01;
    if (dist_curr < dist_prev * (1 + buffer_pct) && m.ema9(i) < m.ema21(i)) {
      convergence_periods++;
      double rate = (dist_prev - dist_curr) / dist_prev;
      total_convergence_rate += rate;
      max_convergence_rate = std::max(max_convergence_rate, rate);
    }
  }

  if (convergence_periods == 0)
    return HintType::None;

  // Score based on consistency and strength
  score += (convergence_periods - 3) * 0.15;  // 3-8 periods adds 0-0.75

  // Average convergence rate
  double avg_rate = total_convergence_rate / convergence_periods;
  if (avg_rate > 0.08)
    score += 0.25;  // Strong consistent convergence
  else if (avg_rate < 0.03)
    score -= 0.2;  // Weak convergence

  // Check if convergence is accelerating
  double recent_dist_change =
      (m.ema21(idx - 2) - m.ema9(idx - 2)) - (m.ema21(idx) - m.ema9(idx));
  double older_dist_change = (m.ema21(idx - 5) - m.ema9(idx - 5)) -
                             (m.ema21(idx - 3) - m.ema9(idx - 3));
  if (recent_dist_change > older_dist_change * 1.2)
    score += 0.2;  // Accelerating convergence

  // Check how close they are now
  double current_dist = std::abs(m.ema9(idx) - m.ema21(idx)) / m.ema21(idx);
  if (current_dist < 0.005)
    score += 0.2;  // Very close
  else if (current_dist > 0.015)
    score -= 0.15;  // Still far apart

  // Supporting momentum
  if (m.rsi(idx) > m.rsi(idx - 3) && m.rsi(idx) > 45)
    score += 0.15;

  // Price action confirmation
  if (m.price(idx) > m.price(idx - 2))
    score += 0.1;

  return {HintType::Ema9ConvEma21, std::clamp(score, 0.4, 1.6)};
}
