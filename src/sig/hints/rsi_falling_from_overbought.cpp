#include "ind/indicators.h"
#include "sig/signals.h"

Hint rsi_falling_from_overbought_hint(const IndicatorsTrends& m, int idx) {
  // Current RSI must be falling from overbought
  if (m.rsi(idx) >= m.rsi(idx - 1) || m.rsi(idx) >= 70)
    return HintType::None;

  double score = 1.0;

  // Find the overbought peak
  double peak_rsi = 0;
  int peak_idx = -1;
  int overbought_duration = 0;

  for (int i = idx - 1; i >= idx - 8 && i > 0; i--) {
    if (m.rsi(i) > 70) {
      overbought_duration++;
      if (m.rsi(i) > peak_rsi) {
        peak_rsi = m.rsi(i);
        peak_idx = i;
      }
    }
  }

  if (peak_rsi < 70)
    return HintType::None;

  // Score based on peak extremity
  if (peak_rsi > 80)
    score += 0.35;  // Extreme overbought
  else if (peak_rsi > 75)
    score += 0.2;  // Strong overbought

  // Duration in overbought
  if (overbought_duration >= 4)
    score += 0.25;  // Extended overbought
  else if (overbought_duration == 1)
    score -= 0.2;  // Brief spike

  // Fall characteristics
  double total_drop = peak_rsi - m.rsi(idx);
  double periods_falling = idx - peak_idx;

  if (total_drop > 15)
    score += 0.3;  // Sharp drop
  else if (total_drop < 5)
    score -= 0.25;  // Mild drop

  // Fall velocity
  double drop_rate = total_drop / periods_falling;
  if (drop_rate > 5)
    score += 0.2;  // Rapid fall
  else if (drop_rate < 2)
    score -= 0.15;  // Slow drift

  // Price confirmation
  if (m.price(idx) < m.price(peak_idx))
    score += 0.15;

  // Check if it's breaking below 70 for first time
  if (m.rsi(idx - 1) > 70 && m.rsi(idx) < 70)
    score += 0.2;  // Key level break

  return {HintType::RsiDropFromOverbought, std::clamp(score, 0.4, 1.8)};
}
