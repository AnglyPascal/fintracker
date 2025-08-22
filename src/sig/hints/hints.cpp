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
inline Hint near_support_resistance_hint(const IndicatorsTrends& ind, int idx) {
  auto price = ind.price(idx);

  auto support_opt = ind.nearest_support_below(idx);
  auto resistance_opt = ind.nearest_resistance_above(idx);

  if (!support_opt && !resistance_opt)
    return HintType::None;

  auto near_support = support_opt && support_opt->get().is_near(price);
  auto near_resistance = resistance_opt && resistance_opt->get().is_near(price);

  // Handle tight range
  if (near_support && near_resistance) {
    double score = 1.0;

    // Check zone strengths
    double support_conf = support_opt->get().conf;
    double resistance_conf = resistance_opt->get().conf;
    double avg_conf = (support_conf + resistance_conf) / 2;

    score += (avg_conf - 0.5) * 0.6;  // Conf from 0.5-1.0 adds 0-0.3

    // Tighter range is more significant
    double range = resistance_opt->get().lo - support_opt->get().hi;
    double range_pct = range / price;
    if (range_pct < 0.02)
      score += 0.2;  // Very tight
    else if (range_pct > 0.04)
      score -= 0.2;  // Wide range

    return {HintType::WithinTightRange, std::clamp(score, 0.6, 1.5)};
  }

  // Handle near support
  if (near_support) {
    double score = 1.0;
    const auto& zone = support_opt->get();

    // Zone confidence directly affects score
    score += (zone.conf - 0.5) * 0.8;  // Conf from 0.5-1.0 adds 0-0.4

    // Distance to support
    double distance = zone.distance(price);
    double distance_pct = distance / price;
    if (distance_pct < 0.003)
      score += 0.2;  // Very close
    else if (distance_pct > 0.005)
      score -= 0.15;  // Getting far

    // Check number of previous hits
    if (zone.hits.size() >= 3)
      score += 0.15;  // Well-tested support

    // Check for recent tests
    bool recent_test = false;
    for (const auto& hit : zone.hits)
      if (idx - hit.r < 10) {
        recent_test = true;
        break;
      }
    if (recent_test)
      score -= 0.2;  // Recently tested, might be weakening

    return zone.is_strong()
               ? Hint{HintType::NearStrongSupport, std::clamp(score, 0.5, 1.6)}
               : Hint{HintType::NearWeakSupport, std::clamp(score, 0.4, 1.3)};
  }

  // Handle near resistance
  if (near_resistance) {
    double score = 1.0;
    const auto& zone = resistance_opt->get();

    // Zone confidence
    score += (zone.conf - 0.5) * 0.8;

    // Distance to resistance
    double distance = zone.distance(price);
    double distance_pct = distance / price;
    if (distance_pct < 0.003)
      score += 0.25;  // Very close - strong exit signal
    else if (distance_pct > 0.005)
      score -= 0.15;

    // Number of rejections
    if (zone.hits.size() >= 3)
      score += 0.2;  // Strong resistance history

    // Recent rejection check
    bool recent_rejection = false;
    for (const auto& hit : zone.hits)
      if (idx - hit.r < 10) {
        recent_rejection = true;
        break;
      }
    if (recent_rejection)
      score += 0.15;  // Recently rejected, likely to reject again

    return zone.is_strong() ? Hint{HintType::NearStrongResistance,
                                   std::clamp(score, 0.5, 1.7)}
                            : Hint{HintType::NearWeakResistance,
                                   std::clamp(score, 0.4, 1.4)};
  }

  return HintType::None;
}

Hint rsi_bullish_divergence(const IndicatorsTrends& ind, int idx);
Hint macd_bullish_divergence(const IndicatorsTrends& ind, int idx);
Hint rsi_bearish_divergence(const IndicatorsTrends& ind, int idx);

Hint price_trending(const IndicatorsTrends& m, int idx);
Hint ema21_trending(const IndicatorsTrends& m, int idx);
Hint rsi_trending(const IndicatorsTrends& m, int idx);

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

    // Trends (unchanged for now)
    price_trending,
    ema21_trending,
    rsi_trending,

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
