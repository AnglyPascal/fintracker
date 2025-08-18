#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

// Entry Hints
inline Hint ema_converging_hint(const Indicators& m, int idx) {
  double prev_dist = std::abs(m.ema9(idx - 1) - m.ema21(idx - 1));
  double curr_dist = std::abs(m.ema9(idx) - m.ema21(idx));

  if (m.ema9(idx) < m.ema21(idx) && curr_dist < prev_dist &&
      curr_dist / m.ema21(idx) < 0.01) {
    return HintType::Ema9ConvEma21;
  }

  return HintType::None;
}

inline Hint rsi_approaching_50_hint(const Indicators& ind, int idx) {
  if (ind.rsi(idx - 1) < ind.rsi(idx) && ind.rsi(idx) > 45 && ind.rsi(idx) < 50)
    return HintType::RsiConv50;
  return HintType::None;
}

inline Hint macd_histogram_rising_hint(const Indicators& ind, int idx) {
  if (ind.hist(idx - 1) < ind.hist(idx) && ind.hist(idx) < 0)
    return HintType::MacdRising;
  return HintType::None;
}

inline Hint price_pullback_hint(const Indicators& m, int idx) {
  if (m.price(idx - 1) > m.ema21(idx - 1) && m.price(idx) < m.ema21(idx))
    return HintType::Pullback;
  return HintType::None;
}

inline Hint rsi_bullish_divergence(const Indicators& ind, int idx) {
  // Price made a lower low, RSI made a higher low -> bullish divergence
  if (ind.low(idx - 1) > ind.low(idx) && ind.rsi(idx - 1) < ind.rsi(idx) &&
      ind.rsi(idx - 1) < 40 && ind.rsi(idx) < 60)
    return HintType::RsiBullishDiv;

  return HintType::None;
}

inline Hint macd_bullish_divergence(const Indicators& ind, int idx) {
  // Price made a lower low, MACD made a higher low
  if (ind.low(idx) < ind.low(idx - 1) && ind.macd(idx) > ind.macd(idx - 1) &&
      ind.macd(idx - 1) < 0 && ind.macd(idx) < 0)
    return HintType::MacdBullishDiv;

  return HintType::None;
}

// Exit Hints

inline Hint ema_diverging_hint(const Indicators& m, int idx) {
  double prev_dist = std::abs(m.ema9(idx - 1) - m.ema21(idx - 1));
  double curr_dist = std::abs(m.ema9(idx) - m.ema21(idx));

  if (m.ema9(idx) > m.ema21(idx) && curr_dist > prev_dist &&
      curr_dist / m.ema21(idx) > 0.02) {
    return HintType::Ema9DivergeEma21;
  }

  return HintType::None;
}

inline Hint rsi_falling_from_overbought_hint(const Indicators& m, int idx) {
  double prev = m.rsi(idx - 1), now = m.rsi(idx);
  if (prev > 70 && now < prev && prev - now > 3.0)
    return HintType::RsiDropFromOverbought;
  return HintType::None;
}

inline Hint macd_histogram_peaking_hint(const Indicators& m, int idx) {
  if (m.hist(idx - 2) < m.hist(idx - 1) && m.hist(idx - 1) > m.hist(idx))
    return HintType::MacdPeaked;
  return HintType::None;
}

inline Hint ema_flattens_hint(const Indicators& m, int idx) {
  double slope = m.ema9(idx) - m.ema9(idx - 1);
  bool is_flat = std::abs(slope / m.ema9(idx)) < 0.001;

  double dist = std::abs(m.ema9(idx) - m.ema21(idx));
  bool is_close = dist / m.ema21(idx) < 0.01;

  if (is_flat && is_close)
    return HintType::Ema9Flattening;

  return HintType::None;
}

inline Hint rsi_bearish_divergence(const Indicators& ind, int idx) {
  // Price up but RSI down — loss of momentum
  if (ind.price(idx) > ind.price(idx - 1) && ind.rsi(idx) < ind.rsi(idx - 1) &&
      ind.rsi(idx) > 50)
    return HintType::RsiBearishDiv;

  return HintType::None;
}

// Trends

inline Hint price_trending(const Indicators& m, int idx) {
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

inline Hint ema21_trending(const Indicators& m, int idx) {
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

inline Hint rsi_trending(const Indicators& m, int idx) {
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

// SR Proximity

// Support/Resistance Hints
inline Hint near_support_resistance_hint(const Indicators& ind, int idx) {
  auto price = ind.price(idx);

  auto support_opt = ind.nearest_support_below(idx);
  auto resistance_opt = ind.nearest_resistance_above(idx);

  if (!support_opt && !resistance_opt)
    return HintType::None;

  auto near_support = support_opt && support_opt->get().is_near(price);
  auto near_resistance = resistance_opt && resistance_opt->get().is_near(price);

  if (near_support && near_resistance)
    return HintType::WithinTightRange;

  if (near_support)
    return support_opt->get().is_strong() ? HintType::NearStrongSupport
                                          : HintType::NearWeakSupport;

  if (near_resistance)
    return resistance_opt->get().is_strong() ? HintType::NearStrongResistance
                                             : HintType::NearWeakResistance;

  return HintType::None;
}

inline constexpr hint_f hint_funcs[] = {
    // Entry
    ema_converging_hint,
    rsi_approaching_50_hint,
    macd_histogram_rising_hint,
    price_pullback_hint,

    rsi_bullish_divergence,
    macd_bullish_divergence,

    // Exit
    ema_diverging_hint,
    rsi_falling_from_overbought_hint,
    macd_histogram_peaking_hint,
    ema_flattens_hint,

    rsi_bearish_divergence,

    // Trends
    price_trending,
    ema21_trending,
    rsi_trending,

    // SR
    near_support_resistance_hint,
};

std::vector<Hint> hints(const Indicators& ind, int idx) {
  std::vector<Hint> res;
  for (auto f : hint_funcs) {
    res.push_back(f(ind, idx));
  }
  return res;
}

std::map<HintType, SignalStats> Stats::get_hint_stats(const Backtest& bt) {
  std::map<HintType, SignalStats> hint;
  for (auto& f : hint_funcs) {
    auto [h, s] = bt.get_stats<HintType>(f);
    hint.try_emplace(h, s);
  }
  return hint;
}
