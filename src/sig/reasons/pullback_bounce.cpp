#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Reason pullback_bounce_entry(const IndicatorsTrends& ind, int idx) {
  // Preconditions: trend and price alignment
  if (!(ind.ema9(idx) > ind.ema21(idx) && ind.price(idx) > ind.ema21(idx)))
    return ReasonType::None;

  const int LOOKBACK = sig_config.pullback_bounce_lookback;
  const int MAX_PULLBACK_SCAN = sig_config.pullback_scan_lookback;
  std::vector<std::string> reasons;

  // Find most recent pullback candle (price < ema21) within lookback
  bool found = false;
  int pb_end = 0;
  for (int i = idx - 1; i >= idx - LOOKBACK; --i) {
    if (ind.price(i) < ind.ema21(i)) {
      pb_end = i;
      found = true;
      break;
    }
  }

  if (!found)
    return ReasonType::None;

  // Expand backward to find contiguous pullback start
  int pb_start = pb_end - 1;
  for (; pb_start >= pb_end - MAX_PULLBACK_SCAN; --pb_start)
    if (ind.price(pb_start) > ind.ema21(pb_start))
      break;

  // Base score and recency decay
  int recency = idx - pb_end - 1;
  double score = 1.0 - recency * 0.15;
  if (recency > 0)
    reasons.push_back(std::format("-{}c", recency));
  else
    reasons.push_back("now");

  {
    // Recovery characteristics
    int recovery_candles = 0;
    for (int i = pb_end + 1; i <= idx; ++i) {
      if (ind.price(i) <= ind.ema21(i) * 1.005)
        recovery_candles++;
    }

    if (recovery_candles > 0) {
      score -= recovery_candles * 0.10;
      reasons.push_back(std::format("{}c slow recovery", recovery_candles));
    }
  }

  {
    // Compute pullback metrics
    double pb_depth = 0.0;
    for (int i = pb_start; i <= pb_end; ++i) {
      double denom = ind.ema21(i);
      if (denom <= 0.0)
        continue;
      pb_depth = std::max(pb_depth, (denom - ind.price(i)) / denom);
    }

    // Depth handling
    if (pb_depth > 0.001 && pb_depth < 0.03) {
      score += 0.30;
      reasons.push_back(
          std::format("shallow pullback ({:.1f})", pb_depth * 100));
    } else if (pb_depth >= 0.03 && pb_depth <= 0.05) {
      score += 0.05;
      reasons.push_back(
          std::format("moderate pullback ({:.1f})", pb_depth * 100));
    } else if (pb_depth > 0.05) {
      score -= 0.25;
      reasons.push_back(std::format("deep pullback ({:.1f})", pb_depth * 100));
    }
  }

  {
    // Duration
    int pb_candles = pb_end - pb_start + 1;
    if (pb_candles <= 2) {
      score += 0.20;
      reasons.push_back(std::format("quick pullback ({}c)", pb_candles));
    } else if (pb_candles > 5) {
      score -= 0.30;
      reasons.push_back(std::format("extended pullback ({}c)", pb_candles));
    }
  }

  {
    // Bounce strength
    double denom_now = ind.ema21(idx);
    if (denom_now > 0.0) {
      double bounce_strength = (ind.price(idx) - denom_now) / denom_now;
      if (bounce_strength > 0.01) {
        score += 0.20;
        reasons.push_back("strong bounce");
      } else if (bounce_strength < 0.002) {
        score -= 0.05;
        reasons.push_back("weak bounce");
      }
    }
  }

  {
    // RSI support
    if (ind.rsi(idx) > ind.rsi(idx - 1) && ind.rsi(idx) > 45) {
      score += 0.15;
      reasons.push_back("rsi rising");
    }
  }

  {
    // Trend confirmation
    if (ind.ema21(idx) > ind.ema50(idx)) {
      score += 0.10;
      reasons.push_back("uptrend");
    } else {
      score -= 0.05;
      reasons.push_back("weak trend");
    }
  }

  {
    // Freshness bonus
    bool immediate_pullback = (pb_end == idx - 1);
    if (immediate_pullback) {
      score += 0.05;
      reasons.push_back("immediate");
    }
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {ReasonType::PullbackBounce, std::clamp(score, 0.5, 1.8), desc};
}
