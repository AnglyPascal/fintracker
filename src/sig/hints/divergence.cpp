#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

Hint rsi_bullish_divergence(const IndicatorsTrends& ind, int idx) {
  // Price made a lower low, RSI made a higher low -> bullish divergence
  if (ind.low(idx - 1) > ind.low(idx) && ind.rsi(idx - 1) < ind.rsi(idx) &&
      ind.rsi(idx - 1) < 40 && ind.rsi(idx) < 60) {
    double score = 1.0;

    // Check divergence strength
    double price_drop = (ind.low(idx - 1) - ind.low(idx)) / ind.low(idx - 1);
    double rsi_rise = ind.rsi(idx) - ind.rsi(idx - 1);

    if (price_drop > 0.01 && rsi_rise > 5)
      score += 0.4;  // Strong divergence
    else if (price_drop < 0.005 || rsi_rise < 3)
      score -= 0.3;  // Weak divergence

    // RSI must be oversold or near
    if (ind.rsi(idx - 1) < 35)
      score += 0.2;  // Deep oversold

    // Check for confirmation candle
    if (ind.price(idx) > ind.price(idx - 1))
      score += 0.15;  // Already bouncing

    // Look for multiple divergence points (stronger signal)
    int divergence_points = 1;
    for (int i = idx - 6; i < idx - 1 && i > 0; i++)
      if (ind.low(i) < ind.low(idx) && ind.rsi(i) > ind.rsi(idx - 1))
        divergence_points++;

    if (divergence_points >= 2)
      score += 0.25;  // Multiple divergence points

    return {HintType::RsiBullishDiv, std::clamp(score, 0.5, 1.7)};
  }
  return HintType::None;
}

Hint macd_bullish_divergence(const IndicatorsTrends& ind, int idx) {
  // Price made a lower low, MACD made a higher low
  if (ind.low(idx) < ind.low(idx - 1) && ind.macd(idx) > ind.macd(idx - 1) &&
      ind.macd(idx - 1) < 0 && ind.macd(idx) < 0) {
    double score = 1.0;

    // Check divergence magnitude
    double price_drop = (ind.low(idx - 1) - ind.low(idx)) / ind.low(idx - 1);
    double macd_rise = ind.macd(idx) - ind.macd(idx - 1);

    if (price_drop > 0.01 && macd_rise > 0.001)
      score += 0.35;  // Strong divergence
    else if (price_drop < 0.005 || macd_rise < 0.0005)
      score -= 0.25;  // Weak divergence

    // MACD histogram improving
    if (ind.hist(idx) > ind.hist(idx - 1))
      score += 0.15;

    // Check for trend context
    if (ind.ema21(idx) < ind.ema21(idx - 3))
      score += 0.1;  // In downtrend, divergence more meaningful

    return {HintType::MacdBullishDiv, std::clamp(score, 0.5, 1.6)};
  }
  return HintType::None;
}

Hint rsi_bearish_divergence(const IndicatorsTrends& ind, int idx) {
  // Price up but RSI down – loss of momentum
  if (ind.price(idx) > ind.price(idx - 1) && ind.rsi(idx) < ind.rsi(idx - 1) &&
      ind.rsi(idx) > 50) {
    double score = 1.0;

    // Check divergence strength
    double price_rise =
        (ind.price(idx) - ind.price(idx - 1)) / ind.price(idx - 1);
    double rsi_drop = ind.rsi(idx - 1) - ind.rsi(idx);

    if (price_rise > 0.01 && rsi_drop > 5)
      score += 0.4;  // Strong divergence
    else if (price_rise < 0.005 || rsi_drop < 2)
      score -= 0.3;  // Weak divergence

    // RSI from overbought is stronger signal
    if (ind.rsi(idx - 1) > 70)
      score += 0.25;
    else if (ind.rsi(idx - 1) < 60)
      score -= 0.2;

    // Check for lower highs in RSI (classic divergence)
    double prev_high_rsi = 0;
    for (int i = idx - 10; i < idx - 2 && i > 0; i++)
      prev_high_rsi = std::max(prev_high_rsi, ind.rsi(i));

    if (prev_high_rsi > 0 && ind.rsi(idx - 1) < prev_high_rsi)
      score += 0.2;  // Lower high pattern

    return {HintType::RsiBearishDiv, std::clamp(score, 0.5, 1.7)};
  }

  return HintType::None;
}
