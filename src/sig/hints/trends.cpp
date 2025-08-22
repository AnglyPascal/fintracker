#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

Hint price_trending(const IndicatorsTrends& m, int idx) {
  auto best = m.price_trend(idx);

  if (best.slope() > 0.3 && best.r2 > 0.8)
    return HintType::PriceUpStrongly;
  if (best.slope() > 0.15)
    return HintType::PriceUp;
  if (best.slope() < -0.3 && best.r2 > 0.8)
    return HintType::PriceDownStrongly;
  if (best.slope() < -0.15)
    return HintType::PriceDown;

  return HintType::None;
}

Hint ema21_trending(const IndicatorsTrends& m, int idx) {
  auto best = m.ema21_trend(idx);

  if (best.slope() > 0.3 && best.r2 > 0.8)
    return HintType::Ema21UpStrongly;
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return HintType::Ema21Up;
  if (best.slope() < -0.3 && best.r2 > 0.8)
    return HintType::Ema21DownStrongly;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return HintType::Ema21Down;

  return HintType::None;
}

Hint rsi_trending(const IndicatorsTrends& m, int idx) {
  auto best = m.rsi_trend(idx);

  if (best.slope() > 0.3 && best.r2 > 0.85)
    return HintType::RsiUpStrongly;
  if (best.slope() > 0.15)
    return HintType::RsiUp;
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return HintType::RsiDownStrongly;
  if (best.slope() < -0.15)
    return HintType::RsiDown;

  return HintType::None;
}
