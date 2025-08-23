#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Hint rsi_bullish_divergence(const IndicatorsTrends& ind, int idx) {
  const int LOOKBACK = 10;  // Look back further for proper divergence
  std::vector<std::string> reasons;

  // Find recent low in price
  int price_low_idx = idx;
  double price_low = ind.low(idx);
  for (int i = idx - 1; i >= idx - LOOKBACK && i >= 0; i--) {
    if (ind.low(i) < price_low) {
      price_low = ind.low(i);
      price_low_idx = i;
    }
  }

  // Need at least 2 candles separation for meaningful divergence
  if (idx - price_low_idx < 2)
    return HintType::None;

  // Check for divergence: current low higher than previous, RSI higher than at
  // price low
  bool price_higher_low = ind.low(idx) > price_low;
  bool rsi_higher_low = ind.rsi(idx) > ind.rsi(price_low_idx);

  // Both RSI values should be oversold territory
  bool both_oversold = ind.rsi(idx) < 60 && ind.rsi(price_low_idx) < 50;

  if (!price_higher_low || !rsi_higher_low || !both_oversold)
    return HintType::None;

  double score = 1.0;

  // Age of divergence
  int separation = idx - price_low_idx;
  if (separation > 5) {
    score -= 0.2;
    reasons.push_back(std::format("{}c separation", separation));
  }

  // Check divergence strength
  double price_drop = (price_low - ind.low(idx)) / price_low;
  double rsi_rise = ind.rsi(idx) - ind.rsi(price_low_idx);
  if (price_drop > 0.02 && rsi_rise > 8) {
    score += 0.4;
    reasons.push_back("strong divergence");
  } else if (price_drop < 0.01 || rsi_rise < 4) {
    score -= 0.3;
    reasons.push_back("weak divergence");
  }

  // RSI oversold depth
  if (ind.rsi(price_low_idx) < 30) {
    score += 0.2;
    reasons.push_back("deep oversold");
  } else if (ind.rsi(price_low_idx) < 40) {
    reasons.push_back("oversold");
  }

  // Current RSI position
  if (ind.rsi(idx) > 45) {
    score += 0.15;
    reasons.push_back("rsi recovering");
  }

  // Check for confirmation
  if (ind.price(idx) > ind.price(idx - 1)) {
    score += 0.15;
    reasons.push_back("price bouncing");
  }

  // Look for multiple divergence points
  int divergence_points = 1;
  for (int i = price_low_idx + 1; i < idx; i++) {
    if (ind.low(i) > price_low && ind.rsi(i) > ind.rsi(price_low_idx))
      divergence_points++;
  }

  if (divergence_points >= 2) {
    score += 0.25;
    reasons.push_back("multiple points");
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {HintType::RsiBullishDiv, std::clamp(score, 0.5, 1.7), desc};
}

Hint macd_bullish_divergence(const IndicatorsTrends& ind, int idx) {
  const int LOOKBACK = 15;  // Longer lookback for MACD divergence
  std::vector<std::string> reasons;

  // Find recent low in price
  int price_low_idx = idx;
  double price_low = ind.low(idx);
  for (int i = idx - 1; i >= idx - LOOKBACK && i >= 0; i--) {
    if (ind.low(i) < price_low) {
      price_low = ind.low(i);
      price_low_idx = i;
    }
  }

  // Need meaningful separation
  if (idx - price_low_idx < 3)
    return HintType::None;

  // Check for divergence: price lower low, MACD higher low
  bool price_lower_low = ind.low(idx) < price_low;
  bool macd_higher_low = ind.macd(idx) > ind.macd(price_low_idx);

  // Both MACD values should be negative (bear territory)
  bool both_negative = ind.macd(idx) < 0 && ind.macd(price_low_idx) < 0;

  if (!price_lower_low || !macd_higher_low || !both_negative)
    return HintType::None;

  double score = 1.0;

  // Age of divergence
  int separation = idx - price_low_idx;
  if (separation > 8) {
    score -= 0.2;
    reasons.push_back(std::format("{}c separation", separation));
  }

  // Check divergence magnitude
  double price_drop = (price_low - ind.low(idx)) / price_low;
  double macd_rise = ind.macd(idx) - ind.macd(price_low_idx);
  if (price_drop > 0.02 && macd_rise > 0.002) {
    score += 0.35;
    reasons.push_back("strong divergence");
  } else if (price_drop < 0.01 || macd_rise < 0.001) {
    score -= 0.25;
    reasons.push_back("weak divergence");
  }

  // MACD histogram improving
  if (ind.hist(idx) > ind.hist(idx - 1)) {
    score += 0.15;
    reasons.push_back("hist improving");
  }

  // Check if MACD was deeply negative
  if (ind.macd(price_low_idx) < -0.005) {
    score += 0.2;
    reasons.push_back("deep negative");
  }

  // Trend context - divergence more meaningful in downtrend
  if (ind.ema21(idx) < ind.ema21(idx - 5)) {
    score += 0.1;
    reasons.push_back("in downtrend");
  }

  // Current momentum
  if (ind.macd(idx) > ind.macd(idx - 1)) {
    score += 0.1;
    reasons.push_back("macd rising");
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {HintType::MacdBullishDiv, std::clamp(score, 0.5, 1.6), desc};
}

Hint rsi_bearish_divergence(const IndicatorsTrends& ind, int idx) {
  const int LOOKBACK = 10;
  std::vector<std::string> reasons;

  // Find recent high in price
  int price_high_idx = idx;
  double price_high = ind.high(idx);
  for (int i = idx - 1; i >= idx - LOOKBACK && i >= 0; i--) {
    if (ind.high(i) > price_high) {
      price_high = ind.high(i);
      price_high_idx = i;
    }
  }

  // Need meaningful separation
  if (idx - price_high_idx < 2)
    return HintType::None;

  // Check for divergence: price higher high, RSI lower high
  bool price_higher_high = ind.high(idx) > price_high;
  bool rsi_lower_high = ind.rsi(idx) < ind.rsi(price_high_idx);

  // Current RSI should be in meaningful territory
  bool rsi_meaningful = ind.rsi(idx) > 50 && ind.rsi(price_high_idx) > 60;

  if (!price_higher_high || !rsi_lower_high || !rsi_meaningful)
    return HintType::None;

  double score = 1.0;

  // Age of divergence
  int separation = idx - price_high_idx;
  if (separation > 6) {
    score -= 0.2;
    reasons.push_back(std::format("{}c separation", separation));
  }

  // Check divergence strength
  double price_rise = (ind.high(idx) - price_high) / price_high;
  double rsi_drop = ind.rsi(price_high_idx) - ind.rsi(idx);
  if (price_rise > 0.01 && rsi_drop > 8) {
    score += 0.4;
    reasons.push_back("strong divergence");
  } else if (price_rise < 0.005 || rsi_drop < 3) {
    score -= 0.3;
    reasons.push_back("weak divergence");
  }

  // RSI from overbought is stronger
  if (ind.rsi(price_high_idx) > 75) {
    score += 0.25;
    reasons.push_back("from overbought");
  } else if (ind.rsi(price_high_idx) > 65) {
    score += 0.1;
    reasons.push_back("from elevated");
  } else {
    score -= 0.2;
    reasons.push_back("not overbought");
  }

  // Current RSI level
  if (ind.rsi(idx) < 65) {
    reasons.push_back("rsi declining");
  } else {
    score -= 0.1;
    reasons.push_back("still high rsi");
  }

  // Check for lower highs pattern in RSI
  bool lower_high_pattern = true;
  for (int i = price_high_idx + 1; i <= idx; i++) {
    if (ind.rsi(i) > ind.rsi(price_high_idx)) {
      lower_high_pattern = false;
      break;
    }
  }

  if (lower_high_pattern) {
    score += 0.2;
    reasons.push_back("lower high pattern");
  }

  // Trend context - more meaningful in uptrend
  if (ind.ema21(idx) > ind.ema21(idx - 5)) {
    score += 0.1;
    reasons.push_back("in uptrend");
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {HintType::RsiBearishDiv, std::clamp(score, 0.5, 1.7), desc};
}
