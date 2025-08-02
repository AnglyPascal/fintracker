#include "backtest.h"
#include "indicators.h"
#include "portfolio.h"
#include "signal.h"

#include <iostream>

inline const std::unordered_map<ReasonType, Meta> reason_meta = {
    {ReasonType::None,  //
     {Severity::Low, Source::EMA, SignalClass::Entry, ""}},
    // Entry:
    {ReasonType::EmaCrossover,  //
     {Severity::High, Source::EMA, SignalClass::Entry, "ema⤯"}},
    {ReasonType::RsiCross50,  //
     {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi⤯50"}},
    {ReasonType::PullbackBounce,  //
     {Severity::Urgent, Source::Price, SignalClass::Entry, "bounce"}},
    {ReasonType::MacdHistogramCross,  //
     {Severity::Medium, Source::MACD, SignalClass::Entry, "macd⤯"}},
    // Exit:
    {ReasonType::EmaCrossdown,  //
     {Severity::High, Source::EMA, SignalClass::Exit, "ema⤰"}},
    {ReasonType::RsiOverbought,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit, "rsi↱70"}},
    {ReasonType::MacdBearishCross,  //
     {Severity::High, Source::MACD, SignalClass::Exit, "macd⤰"}},
    {ReasonType::StopLossHit,  //
     {Severity::Urgent, Source::Stop, SignalClass::Exit, "stop⤰"}},
};

inline const std::unordered_map<HintType, Meta> hint_meta = {
    {HintType::None,  //
     {Severity::Low, Source::None, SignalClass::None, ""}},
    // Entry
    {HintType::Ema9ConvEma21,  //
     {Severity::Low, Source::EMA, SignalClass::Entry, "ema9↗21"}},
    {HintType::RsiConv50,  //
     {Severity::Low, Source::RSI, SignalClass::Entry, "rsi↗50"}},
    {HintType::MacdRising,  //
     {Severity::Medium, Source::MACD, SignalClass::Entry, "macd↗"}},
    {HintType::Pullback,  //
     {Severity::Medium, Source::Price, SignalClass::Entry, "pullback"}},
    // Exit
    {HintType::Ema9DivergeEma21,  //
     {Severity::Low, Source::EMA, SignalClass::Exit, "ema9↘21"}},
    {HintType::RsiDropFromOverbought,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit, "rsi⭛"}},
    {HintType::MacdPeaked,  //
     {Severity::Medium, Source::MACD, SignalClass::Exit, "macd▲"}},
    {HintType::Ema9Flattening,  //
     {Severity::Low, Source::EMA, SignalClass::Exit, "ema9↝21"}},
    {HintType::StopProximity,  //
     {Severity::High, Source::Stop, SignalClass::Exit, "stop⨯"}},
    {HintType::StopInATR,  //
     {Severity::High, Source::Stop, SignalClass::Exit, "stop!"}},

    // Trends:

    // Entry
    {HintType::PriceUp,  //
     {Severity::Medium, Source::Price, SignalClass::Entry, "price↗"}},
    {HintType::Ema21Up,  //
     {Severity::Medium, Source::EMA, SignalClass::Entry, "ema21↗"}},
    {HintType::RsiUp,  //
     {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi↗"}},
    {HintType::RsiUpStrongly,  //
     {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi⇗"}},
    // Exit
    {HintType::PriceDown,  //
     {Severity::Medium, Source::Price, SignalClass::Exit, "price↘"}},
    {HintType::Ema21Down,  //
     {Severity::Medium, Source::EMA, SignalClass::Exit, "ema21↘"}},
    {HintType::RsiDown,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit, "price↘"}},
    {HintType::RsiDownStrongly,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit, "rsi⇘"}},
};

template <>
SignalType<ReasonType, ReasonType::None>::SignalType(ReasonType type)
    : type{type} {
  auto it = reason_meta.find(type);
  meta = it == reason_meta.end() ? nullptr : &it->second;
}

template <>
SignalType<HintType, HintType::None>::SignalType(HintType type) : type{type} {
  auto it = hint_meta.find(type);
  meta = it == hint_meta.end() ? nullptr : &it->second;
}

// Filters

Filter evaluate_daily_trend(const Indicators& ind_1d) {
  auto& ind = ind_1d;

  auto& ema = ind.trends.ema21.top_trends;
  auto& rsi = ind._rsi.values;

  if (ema.empty() || rsi.empty())
    return Confidence::NeutralOrSideways;

  double slope = ema[0].slope();
  double r2 = ema[0].r2;
  double rsi_val = rsi.back();

  if (slope > 0.08 && r2 > 0.8 && rsi_val > 55)
    return {Confidence::StrongUptrend, "1d⇗"};

  if (slope > 0.02 && r2 > 0.6 && rsi_val > 50)
    return {Confidence::ModerateUptrend, "1d↗"};

  if (rsi_val > 45)
    return {Confidence::NeutralOrSideways, "1d🡒"};

  return {Confidence::Bearish, "1d↘"};
}

Filter evaluate_four_hour_trend(const Indicators& ind_4h) {
  auto& ind = ind_4h;

  auto& ema_trend = ind.trends.ema21.top_trends;
  auto& rsi_vals = ind._rsi.values;
  auto& rsi_trend = ind.trends.rsi.top_trends;

  if (ema_trend.empty() || rsi_vals.empty())
    return {Confidence::NeutralOrSideways, "4h🡒"};

  double ema_slope = ema_trend[0].slope();
  double ema_r2 = ema_trend[0].r2;

  double rsi_val = rsi_vals.back();
  double rsi_slope = 0.0;
  double rsi_r2 = 0.0;
  if (!rsi_trend.empty()) {
    rsi_slope = rsi_trend[0].slope();
    rsi_r2 = rsi_trend[0].r2;
  }

  // Strong Uptrend: clear EMA rise + solid RSI
  if (ema_slope > 0.10 && ema_r2 > 0.8 && rsi_val > 55 && rsi_slope > 0.1 &&
      rsi_r2 > 0.7)
    return {Confidence::StrongUptrend, "4h⇗"};

  // Moderate Uptrend: some slope or RSI support
  if ((ema_slope > 0.03 && ema_r2 > 0.6 && rsi_val > 50) ||
      (rsi_slope > 0.05 && rsi_r2 > 0.6 && rsi_val > 50))
    return {Confidence::ModerateUptrend, "4h↗"};

  // Weak Neutral Zone
  if (rsi_val > 45)
    return {Confidence::NeutralOrSideways, "4h🡒"};

  return {Confidence::Bearish, "4h↘"};
}

Filters evaluate_filters(const Metrics& m) {
  Filters res{
      .trend_4h = evaluate_four_hour_trend(m.ind_4h),
      .trend_1d = evaluate_daily_trend(m.ind_1d),
  };
  return res;
}

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

inline Reason stop_loss_exit(const Indicators& ind, int) {
  auto price = ind.price(-1);
  if (price < ind.stop_loss.final_stop)
    return ReasonType::StopLossHit;
  return ReasonType::None;
}

inline constexpr signal_f reason_funcs[] = {
    // Entry
    ema_crossover_entry,
    rsi_cross_50_entry,
    pullback_bounce_entry,
    macd_histogram_cross_entry,
    // Exit
    ema_crossdown_exit,
    macd_bearish_cross_exit,
    stop_loss_exit,
};

std::vector<Reason> reasons(const Indicators& ind, int idx) {
  std::vector<Reason> res;
  for (auto f : reason_funcs) {
    res.push_back(f(ind, idx));
  }
  return res;
}

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

inline Hint stop_proximity_hint(const Indicators& ind, int idx) {
  auto price = ind.price(-1);
  auto dist = price - ind.stop_loss.final_stop;

  if (dist < 0)
    return HintType::StopInATR;

  if (dist < ind.atr(idx) * 0.75)
    return HintType::StopProximity;

  if (dist < 1.0 * ind.stop_loss.atr_stop - ind.stop_loss.final_stop)
    return HintType::StopInATR;

  return HintType::None;
}

// Trends

inline Hint price_trending(const Indicators& m, int idx) {
  auto best = m.price_trend(idx);

  if (best.slope() > 0.2 && best.r2 > 0.8)
    return HintType::PriceUp;
  if (best.slope() < -0.2 && best.r2 > 0.8)
    return HintType::PriceDown;
  return HintType::None;
}

inline Hint ema21_trending(const Indicators& m, int idx) {
  auto best = m.ema21_trend(idx);
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return HintType::Ema21Up;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return HintType::Ema21Down;
  return HintType::None;
}

inline Hint rsi_trending(const Indicators& m, int idx) {
  auto best = m.rsi_trend(idx);
  if (best.slope() > 0.3 && best.r2 > 0.85)
    return HintType::RsiUpStrongly;
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return HintType::RsiUp;
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return HintType::RsiDownStrongly;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return HintType::RsiDown;
  return HintType::None;
}

inline constexpr hint_f hint_funcs[] = {
    // Entry
    ema_converging_hint,
    rsi_approaching_50_hint,
    macd_histogram_rising_hint,
    price_pullback_hint,

    // Exit
    ema_diverging_hint,
    rsi_falling_from_overbought_hint,
    macd_histogram_peaking_hint,
    ema_flattens_hint,
    stop_proximity_hint,

    // Trends
    price_trending,
    ema21_trending,
    rsi_trending,
};

std::vector<Hint> hints(const Indicators& ind, int idx) {
  std::vector<Hint> res;
  for (auto f : hint_funcs) {
    res.push_back(f(ind, idx));
  }
  return res;
}

// Entry confirmations

inline bool volume_above_ma(const std::vector<Candle>& candles) {
  double sum = 0;
  for (size_t i = candles.size() - 21; i < candles.size() - 1; ++i)
    sum += candles[i].volume;
  double ma21 = sum / 21.0;
  return candles.back().volume > ma21 * 1.25;  // +25% above 21‑period MA
}

inline Confirmation entry_confirmation_15m(const Metrics& m) {
  Indicators ind{
      std::vector<Candle>{m.candles.end() - 10 * 8 * 4, m.candles.end()},
      M_15  //
  };

  auto& candles = ind.candles;
  auto n = candles.size();

  if (!volume_above_ma(candles))
    return "low volume";

  // Overextension above EMA21
  if ((ind.price(-1) - ind.ema21(-1)) / ind.ema21(-1) > 0.0125) {
    return "over ema21";  // Over 1.25% above EMA21
  }

  // Multiple strong candles in a row
  int green_count = 0;
  for (size_t i = n - 4; i < n; ++i) {
    if (candles[i].close > candles[i].open)
      ++green_count;
  }
  if (green_count >= 3)
    return "green run";

  // RSI quick spike (momentum fading)
  if (ind.rsi(-1) > 60 && (ind.rsi(-1) - ind.rsi(-2)) > 15)
    return "rsi quick spike";

  // MACD early crossover
  if (ind.macd(-2) < ind.macd_signal(-2) && ind.macd(-1) > ind.macd_signal(-1))
    return "macd early crossover";  // Crossover just happened

  // RSI weakening or divergence (advanced)
  if (ind._rsi.rising() == false && ind.rsi(-1) < 60)
    return "rsi divergence or weak";

  return "ok";
}

Confirmation higher_timeframe_alignment_confirmation(const Metrics& metrics) {
  auto sig_1h = metrics.get_signal(H_1, -1);
  auto sig_4h = metrics.get_signal(H_4, -1);
  auto sig_1d = metrics.get_signal(D_1, -1);

  if (sig_1h.type == Rating::Entry) {
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist)
      return "4h entry";

    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist)
      return "1d entry";
  }

  return "";
}

inline constexpr conf_f confirmation_funcs[] = {
    entry_confirmation_15m,
    higher_timeframe_alignment_confirmation,
};

std::vector<Confirmation> confirmations(const Metrics& m) {
  std::vector<Confirmation> res;
  for (auto f : confirmation_funcs)
    res.push_back(f(m));
  return res;
}

void Indicators::get_stats() {
  Backtest bt{*this};

  for (auto& f : reason_funcs) {
    auto [r, s] = bt.get_stats<ReasonType>(f);
    reason_stats.try_emplace(r, s);
  }

  for (auto& f : hint_funcs) {
    auto [r, s] = bt.get_stats<HintType>(f);
    hint_stats.try_emplace(r, s);
  }
}
