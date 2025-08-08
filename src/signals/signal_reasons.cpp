#include "config.h"
#include "indicators.h"
#include "signals.h"

inline auto& sig_config = config.sig_config;

// Entry reasons

inline Reason ema_crossover_entry(const Indicators& m, int idx) {
  if (m.ema9(idx - 1) <= m.ema21(idx - 1) && m.ema9(idx) > m.ema21(idx))
    return ReasonType::EmaCrossover;
  return ReasonType::None;
}

inline Reason rsi_cross_50_entry(const Indicators& m, int idx) {
  if (m.rsi(idx - 1) < 50 && m.rsi(idx) >= 50)
    return ReasonType::RsiCross50;
  return ReasonType::None;
}

inline Reason pullback_bounce_entry(const Indicators& m, int idx) {
  bool dipped_below_ema21 = m.price(idx - 1) < m.ema21(idx - 1);
  bool recovered_above_ema21 = m.price(idx) > m.ema21(idx);
  bool ema9_above_ema21 = m.ema9(idx) > m.ema21(idx);

  if (dipped_below_ema21 && recovered_above_ema21 && ema9_above_ema21)
    return ReasonType::PullbackBounce;

  return ReasonType::None;
}

inline Reason macd_histogram_cross_entry(const Indicators& m, int idx) {
  if (m.hist(idx - 1) < 0 && m.hist(idx) >= 0)
    return ReasonType::MacdHistogramCross;
  return ReasonType::None;
}

// Exit reasons

inline Reason ema_crossdown_exit(const Indicators& m, int idx) {
  if (m.ema9(idx - 1) >= m.ema21(idx - 1) && m.ema9(idx) < m.ema21(idx))
    return ReasonType::EmaCrossdown;
  return ReasonType::None;
}

inline Reason macd_bearish_cross_exit(const Indicators& ind, int idx) {
  if (ind.macd(idx - 1) >= ind.macd_signal(idx - 1) &&
      ind.macd(idx) < ind.macd_signal(idx))
    return ReasonType::MacdBearishCross;
  return ReasonType::None;
}

// SR breaks
//
// Support/Resistance Reasons (actual signals)
inline Reason broke_support_exit(const Indicators& ind, int idx) {
  double current_close = ind.price(idx);
  double prev_close = ind.price(idx - 1);

  for (const auto& zone : ind.support_zones()) {
    // Previous candle was above/in the zone
    if (prev_close >= zone.lo) {
      double break_threshold = zone.lo * (1 - 0.01);  // FIXME with config

      // Current candle closed below zone with minimum penetration
      if (current_close < break_threshold) {
        // Zone confidence filter - only signal on stronger zones
        // Volume confirmation (if available) - break should have higher
        // volume
        // if (ind.volume(idx) > ind.avg_volume(idx, 10)) {
        //   // 10-period avg volume
        //   return ReasonType::BrokeSupport;
        // }
        // If no volume data, still signal based on price action
        return ReasonType::BrokeSupport;
      }
    }
  }
  return ReasonType::None;
}

inline Reason broke_resistance_entry(const Indicators& ind, int idx) {
  double current_close = ind.price(idx);
  double prev_close = ind.price(idx - 1);

  for (const auto& zone : ind.resistance_zones()) {
    // Previous candle was below/in the zone
    if (prev_close <= zone.hi) {
      double break_threshold = zone.hi * (1 + 0.01);  // FIXME

      // Current candle closed above zone with minimum penetration
      if (current_close > break_threshold) {
        // Zone confidence filter
        // Volume confirmation - breakout should have higher volume
        // if (ind.volume(idx) > ind.avg_volume(idx, 10)) {
        //   return ReasonType::BrokeResistance;
        // }
        // If no volume data, still signal
        return ReasonType::BrokeResistance;
      }
    }
  }
  return ReasonType::None;
}

inline constexpr signal_f reason_funcs[] = {
    // Entry
    ema_crossover_entry,
    rsi_cross_50_entry,
    pullback_bounce_entry,
    macd_histogram_cross_entry,
    broke_resistance_entry,
    // Exit
    ema_crossdown_exit,
    macd_bearish_cross_exit,
    broke_support_exit,
};

std::vector<Reason> reasons(const Indicators& ind, int idx) {
  std::vector<Reason> res;
  for (auto f : reason_funcs) {
    res.push_back(f(ind, idx));
  }
  return res;
}

void Stats::get_reason_stats(const Backtest &bt) {
  for (auto& f : reason_funcs) {
    auto [r, s] = bt.get_stats<ReasonType>(f);
    reason.try_emplace(r, s);
  }
}
