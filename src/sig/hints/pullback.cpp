#include "ind/indicators.h"
#include "sig/signals.h"

Hint price_pullback_hint(const IndicatorsTrends& m, int idx) {
  // Current candle must be below EMA21
  if (m.price(idx) >= m.ema21(idx))
    return HintType::None;

  double score = 1.0;

  // Analyze the pullback pattern
  int above_ema_count = 0;
  int below_ema_count = 0;
  double max_above_distance = 0.0;
  double current_below_distance = (m.ema21(idx) - m.price(idx)) / m.ema21(idx);

  // Look back to find the pattern
  for (int i = idx - 8; i < idx && i > 0; i++) {
    if (m.price(i) > m.ema21(i)) {
      above_ema_count++;
      double dist = (m.price(i) - m.ema21(i)) / m.ema21(i);
      max_above_distance = std::max(max_above_distance, dist);
    } else {
      below_ema_count++;
    }
  }

  // Need prior strength above EMA21
  if (above_ema_count < 3)
    return HintType::None;

  // Ideal pullback: was above, now briefly below
  if (below_ema_count <= 2)
    score += 0.3;  // Fresh pullback
  else if (below_ema_count > 4)
    score -= 0.3;  // Extended weakness

  // Pullback depth
  if (current_below_distance < 0.008)
    score += 0.25;  // Shallow (healthy)
  else if (current_below_distance > 0.02)
    score -= 0.35;  // Too deep

  // Prior strength matters
  if (max_above_distance > 0.015)
    score += 0.2;  // Was strongly above

  // Check if EMA21 is rising (pullback in uptrend)
  double ema_slope = (m.ema21(idx) - m.ema21(idx - 5)) / m.ema21(idx - 5);
  if (ema_slope > 0.005)
    score += 0.25;  // Strong uptrend
  else if (ema_slope < 0)
    score -= 0.3;  // Downtrend pullback (bad)

  // Volume analysis
  double avg_volume = 0;
  for (int i = idx - 5; i < idx; i++)
    avg_volume += m.volume(i);
  avg_volume /= 5;

  if (m.volume(idx) < avg_volume * 0.7)
    score += 0.15;  // Low volume pullback (healthy)
  else if (m.volume(idx) > avg_volume * 1.3)
    score -= 0.2;  // High volume pullback (concerning)

  // Check for support nearby
  auto support = m.nearest_support_below(idx);
  if (support && support->get().is_near(m.price(idx)))
    score += 0.2;  // Pullback to support

  return {HintType::Pullback, std::clamp(score, 0.3, 1.6)};
}
