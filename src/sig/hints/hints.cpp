#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

// Entry Hints
Hint ema_converging_hint(const IndicatorsTrends& m, int idx);
Hint rsi_approaching_50_hint(const IndicatorsTrends& ind, int idx);
Hint macd_histogram_rising_hint(const IndicatorsTrends& ind, int idx);
Hint price_pullback_hint(const IndicatorsTrends& m, int idx);

// Exit Hints
Hint ema_diverging_hint(const IndicatorsTrends& m, int idx);
Hint rsi_falling_from_overbought_hint(const IndicatorsTrends& m, int idx);
Hint macd_histogram_peaking_hint(const IndicatorsTrends& m, int idx);
Hint ema_flattens_hint(const IndicatorsTrends& m, int idx);

// Support/Resistance Hints
Hint near_support_resistance_hint(const IndicatorsTrends& ind, int idx);

Hint rsi_bullish_divergence(const IndicatorsTrends& ind, int idx);
Hint macd_bullish_divergence(const IndicatorsTrends& ind, int idx);
Hint rsi_bearish_divergence(const IndicatorsTrends& ind, int idx);

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

    // SR
    near_support_resistance_hint,
};

// Check for conflicting hints and apply penalties
inline void check_conflicts(std::vector<Hint>& hints) {
  // Find conflicting pairs
  bool has_ema_conv = false, has_ema_div = false;
  bool has_rsi_bull = false, has_rsi_bear = false;
  bool has_macd_rise = false, has_macd_peak = false;

  for (auto& h : hints) {
    if (h.type == HintType::Ema9ConvEma21)
      has_ema_conv = true;
    if (h.type == HintType::Ema9DivergeEma21)
      has_ema_div = true;
    if (h.type == HintType::RsiBullishDiv || h.type == HintType::RsiConv50)
      has_rsi_bull = true;
    if (h.type == HintType::RsiBearishDiv ||
        h.type == HintType::RsiDropFromOverbought)
      has_rsi_bear = true;
    if (h.type == HintType::MacdRising || h.type == HintType::MacdBullishDiv)
      has_macd_rise = true;
    if (h.type == HintType::MacdPeaked)
      has_macd_peak = true;
  }

  // Apply penalties for conflicts
  for (auto& h : hints) {
    if ((h.type == HintType::Ema9ConvEma21 && has_ema_div) ||
        (h.type == HintType::Ema9DivergeEma21 && has_ema_conv))
      h.score *= 0.7;  // EMA conflict

    if (((h.type == HintType::RsiBullishDiv || h.type == HintType::RsiConv50) &&
         has_rsi_bear) ||
        ((h.type == HintType::RsiBearishDiv ||
          h.type == HintType::RsiDropFromOverbought) &&
         has_rsi_bull))
      h.score *= 0.7;  // RSI conflict

    if (((h.type == HintType::MacdRising ||
          h.type == HintType::MacdBullishDiv) &&
         has_macd_peak) ||
        (h.type == HintType::MacdPeaked && has_macd_rise))
      h.score *= 0.7;  // MACD conflict
  }
}

std::vector<Hint> hints(const IndicatorsTrends& ind, int idx) {
  std::vector<Hint> res;
  for (auto f : hint_funcs) {
    auto hint = f(ind, idx);
    if (hint.type != HintType::None)
      res.push_back(hint);
  }

  // Check for conflicts and apply penalties
  check_conflicts(res);

  return res;
}

// Keep the Stats::get_hint_stats function as is
std::map<HintType, SignalStats> Stats::get_hint_stats(const Backtest& bt) {
  std::map<HintType, SignalStats> hint;
  for (auto& f : hint_funcs) {
    auto [h, s] = bt.get_stats<HintType>(f);
    hint.try_emplace(h, s);
  }
  return hint;
}
