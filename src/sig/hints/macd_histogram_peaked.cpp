#include "ind/indicators.h"
#include "sig/signals.h"

Hint macd_histogram_peaking_hint(const IndicatorsTrends& m, int idx) {
  // Basic peak pattern check
  if (!(m.hist(idx - 2) < m.hist(idx - 1) && m.hist(idx - 1) > m.hist(idx)))
    return HintType::None;

  double score = 1.0;
  double peak_height = m.hist(idx - 1);

  // Find previous peaks for comparison
  std::vector<double> previous_peaks;
  for (int i = idx - 15; i < idx - 3 && i > 0; i++) {
    if (m.hist(i - 1) < m.hist(i) && m.hist(i) > m.hist(i + 1))
      previous_peaks.push_back(m.hist(i));
  }

  // Peak prominence
  double prominence = peak_height - std::min(m.hist(idx - 2), m.hist(idx));
  if (prominence > 0.001)
    score += 0.3;  // Prominent peak
  else if (prominence < 0.0003)
    score -= 0.25;  // Weak peak

  // Lower highs pattern (bearish divergence)
  if (!previous_peaks.empty()) {
    double max_prev =
        *std::max_element(previous_peaks.begin(), previous_peaks.end());
    if (peak_height < max_prev * 0.8)
      score += 0.35;  // Clear lower high
    else if (peak_height < max_prev)
      score += 0.15;  // Marginal lower high
  }

  // Peak sharpness (V-shaped vs rounded)
  double left_slope = m.hist(idx - 1) - m.hist(idx - 3);
  double right_slope = m.hist(idx - 1) - m.hist(idx);
  if (left_slope > 0.001 && right_slope > 0.0005)
    score += 0.2;  // Sharp peak

  // Context: MACD line position
  if (m.macd(idx) > 0 && m.macd(idx) < m.macd(idx - 1))
    score += 0.2;  // MACD positive but weakening
  else if (m.macd(idx) < 0)
    score += 0.1;  // MACD already negative

  // Duration of rise before peak
  int rising_periods = 0;
  for (int i = idx - 6; i < idx - 1 && i > 0; i++)
    if (m.hist(i) > m.hist(i - 1))
      rising_periods++;

  if (rising_periods >= 3)
    score += 0.15;  // Sustained rise before peak

  // Check continuation after peak
  if (idx < static_cast<int>(m.size()) - 1 && m.hist(idx + 1) < m.hist(idx))
    score += 0.1;  // Continued weakness

  return {HintType::MacdPeaked, std::clamp(score, 0.4, 1.7)};
}
