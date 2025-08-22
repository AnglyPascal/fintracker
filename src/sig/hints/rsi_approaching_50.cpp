#include "ind/indicators.h"
#include "sig/signals.h"

Hint rsi_approaching_50_hint(const IndicatorsTrends& ind, int idx) {
  // Check if RSI is in the approach zone
  if (ind.rsi(idx) < 40 || ind.rsi(idx) >= 50)
    return HintType::None;

  double score = 1.0;

  // Analyze RSI trend over last 6 candles
  int rising_periods = 0;
  double total_momentum = 0.0;
  double lowest_rsi = ind.rsi(idx);

  for (int i = idx - 5; i <= idx && i > 0; i++) {
    if (ind.rsi(i) > ind.rsi(i - 1)) {
      rising_periods++;
      total_momentum += (ind.rsi(i) - ind.rsi(i - 1));
    }
    lowest_rsi = std::min(lowest_rsi, ind.rsi(i - 1));
  }

  // Need consistent upward movement
  if (rising_periods == 0)
    return HintType::None;

  // Score based on consistency
  score += (rising_periods - 3) * 0.2;  // 3-6 periods adds 0-0.6

  // Average momentum
  double avg_momentum = total_momentum / rising_periods;
  if (avg_momentum > 2.5)
    score += 0.3;  // Strong momentum
  else if (avg_momentum < 1.0)
    score -= 0.25;  // Weak momentum

  // Recovery from oversold is stronger signal
  if (lowest_rsi < 35)
    score += 0.25;  // Coming from deep oversold
  else if (lowest_rsi < 40)
    score += 0.15;  // Coming from oversold

  // Check if approach is smooth or choppy
  int direction_changes = 0;
  for (int i = idx - 4; i < idx && i > 0; i++)
    if ((ind.rsi(i) - ind.rsi(i - 1)) * (ind.rsi(i - 1) - ind.rsi(i - 2)) < 0)
      direction_changes++;

  if (direction_changes >= 2)
    score -= 0.3;  // Choppy approach

  // Distance to 50
  double distance = 50 - ind.rsi(idx);
  if (distance < 2)
    score += 0.15;  // Very close
  else if (distance > 5)
    score -= 0.2;  // Still far

  // Check for failed attempts
  for (int i = idx - 8; i < idx - 2 && i > 0; i++)
    if (ind.rsi(i) > 48 && ind.rsi(i + 1) < 45) {
      score -= 0.35;  // Recent failure
      break;
    }

  return {HintType::RsiConv50, std::clamp(score, 0.3, 1.5)};
}
