#include "signal.h"
#include "indicators.h"

#include <iostream>

template <typename T>
inline T LAST(const std::vector<T>& v) {
  return v.empty() ? T{} : v.back();
}

template <typename T>
inline T PREV(const std::vector<T>& v) {
  return v.size() < 2 ? T{} : v[v.size() - 2];
}

// Entry reasons

inline Reason ema_crossover_entry(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (PREV(ema9) <= PREV(ema21) && LAST(ema9) > LAST(ema21))
    return {ReasonType::EmaCrossover, Severity::High};
  return ReasonType::None;
}

inline Reason rsi_cross_50_entry(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (PREV(rsi) < 50 && LAST(rsi) >= 50)
    return {ReasonType::RsiCross50, Severity::Medium};
  return ReasonType::None;
}

inline Reason pullback_bounce_entry(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  auto& price = m.indicators_1h.candles;

  if (ema9.empty() || ema21.empty() || price.size() < 2)
    return ReasonType::None;

  double close_prev = PREV(price).close;
  double close_now = LAST(price).close;

  bool dipped_below_ema21 = close_prev < PREV(ema21);
  bool recovered_above_ema21 = close_now > LAST(ema21);
  bool ema9_above_ema21 = LAST(ema9) > LAST(ema21);

  if (dipped_below_ema21 && recovered_above_ema21 && ema9_above_ema21)
    return {ReasonType::PullbackBounce, Severity::Urgent};

  return ReasonType::None;
}

inline Reason macd_histogram_cross_entry(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (PREV(hist) < 0 && LAST(hist) >= 0)
    return {ReasonType::MacdHistogramCross, Severity::Medium};
  return ReasonType::None;
}

// Exit reasons

inline Reason ema_crossdown_exit(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (PREV(ema9) >= PREV(ema21) && LAST(ema9) < LAST(ema21))
    return {ReasonType::EmaCrossdown, Severity::High};
  return ReasonType::None;
}

inline Reason rsi_overbought_exit(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (LAST(rsi) >= 70)
    return {ReasonType::RsiOverbought, Severity::Medium};
  return ReasonType::None;
}

inline Reason macd_bearish_cross_exit(const Metrics& m) {
  auto& macd = m.indicators_1h.macd.macd_line;
  auto& signal = m.indicators_1h.macd.signal_line;
  if (macd.size() < 2 || signal.size() < 2)
    return ReasonType::None;

  if (PREV(macd) >= PREV(signal) && LAST(macd) < LAST(signal))
    return {ReasonType::MacdBearishCross, Severity::High};
  return ReasonType::None;
}

inline Reason stop_loss_exit(const Metrics& m) {
  auto price = m.last_price();
  if (price < m.stop_loss.final_stop)
    return {ReasonType::StopLossHit, Severity::Urgent};
  return ReasonType::None;
}

inline constexpr signal_f entry_funcs[] = {
    ema_crossover_entry,
    rsi_cross_50_entry,
    pullback_bounce_entry,
    macd_histogram_cross_entry,
};

inline constexpr signal_f exit_funcs[] = {
    ema_crossdown_exit,
    rsi_overbought_exit,
    macd_bearish_cross_exit,
    stop_loss_exit,
};

// Entry Hints
inline Hint ema_converging_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (ema9.size() < 2 || ema21.size() < 2)
    return "";

  double prev_dist = std::abs(PREV(ema9) - PREV(ema21));
  double curr_dist = std::abs(LAST(ema9) - LAST(ema21));

  if (LAST(ema9) < LAST(ema21) && curr_dist < prev_dist &&
      curr_dist / LAST(ema21) < 0.01) {
    return {"ema9 converging to ema21", Severity::Low};
  }

  return "";
}

inline Hint rsi_approaching_50_hint(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (rsi.size() < 2)
    return "";

  if (PREV(rsi) < LAST(rsi) && LAST(rsi) > 45 && LAST(rsi) < 50)
    return {"rsi approaching 50", Severity::Low};

  return "";
}

inline Hint macd_histogram_rising_hint(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (hist.size() < 2)
    return "";

  if (PREV(hist) < LAST(hist) && LAST(hist) < 0)
    return {"macd rising", Severity::Medium};

  return "";
}

// Exit Hints

inline Hint rsi_falling_from_overbought_hint(const Metrics& m) {
  const auto& rsi = m.indicators_1h.rsi.values;
  if (rsi.size() < 3)
    return "";

  double prev = PREV(rsi);
  double now = LAST(rsi);
  double drop = prev - now;

  if (prev > 70 && now < prev && drop > 3.0)
    return {"RSI falling from overbought zone", Severity::Medium};

  return "";
}

inline Hint macd_histogram_peaking_hint(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (hist.size() < 3)
    return "";

  double h2 = hist[hist.size() - 3];
  double h1 = hist[hist.size() - 2];
  double h0 = hist.back();

  if (h2 < h1 && h1 > h0)
    return {"macd histogram peaked", Severity::Medium};

  return "";
}

inline Hint ema_flattens_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (ema9.size() < 3 || ema21.size() < 3)
    return "";

  double slope = LAST(ema9) - PREV(ema9);
  bool is_flat = std::abs(slope / LAST(ema9)) < 0.001;

  double dist = std::abs(LAST(ema9) - LAST(ema21));
  bool is_close = dist / LAST(ema21) < 0.01;

  if (is_flat && is_close)
    return {"ema9 flattening near ema21", Severity::Low};

  return "";
}

inline Hint stop_proximity_hint(const Metrics& m) {
  auto price = m.last_price();
  auto dist = price - m.stop_loss.final_stop;
  auto dist_pct = dist / price;

  if (dist < 0)
    return "";

  if (dist_pct < 0.02)
    return {"stop very close", Severity::Urgent};
  else if (dist < 1.0 * m.stop_loss.atr_stop - m.stop_loss.final_stop)
    return {"stop inside ATR range", Severity::High};

  return "";
}

inline Hint price_trending_upward_hint(const Metrics& m) {
  auto& price = m.indicators_1h.trends.price;
  auto& best = price.top_trends[0];
  if (best.slope() > 0.2 && best.r2 > 0.8)
    return {"price trending upward", Severity::High};
  return "";
}

inline Hint price_trending_downward_hint(const Metrics& m) {
  auto& price = m.indicators_1h.trends.price;
  auto& best = price.top_trends[0];
  if (best.slope() < -0.2 && best.r2 > 0.8)
    return {"price trending downward", Severity::High};
  return "";
}

inline Hint ema21_trending_upward_hint(const Metrics& m) {
  auto& ema21 = m.indicators_1h.trends.ema21;
  auto& best = ema21.top_trends[0];
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return {"ema21 rising", Severity::Medium};
  return "";
}

inline Hint ema21_trending_downward_hint(const Metrics& m) {
  auto& ema21 = m.indicators_1h.trends.ema21;
  auto& best = ema21.top_trends[0];
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return {"ema21 falling", Severity::Medium};
  return "";
}

inline Hint rsi_trending_upward_hint(const Metrics& m) {
  auto& rsi = m.indicators_1h.trends.rsi;
  auto& best = rsi.top_trends[0];
  if (best.slope() > 0.3 && best.r2 > 0.85)
    return {"rsi rising strongly", Severity::High};
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return {"rsi rising", Severity::Medium};
  return "";
}

inline Hint rsi_trending_downward_hint(const Metrics& m) {
  auto& rsi = m.indicators_1h.trends.rsi;
  auto& best = rsi.top_trends[0];
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return {"rsi falling strongly", Severity::High};
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return {"rsi falling", Severity::Medium};
  return "";
}

inline constexpr hint_f entry_hint_funcs[] = {
    ema_converging_hint,         //
    rsi_approaching_50_hint,     //
    macd_histogram_rising_hint,  //
                                 //
    price_trending_upward_hint,  //
    ema21_trending_upward_hint,  //
    rsi_trending_upward_hint,    //
};

inline constexpr hint_f exit_hint_funcs[] = {
    rsi_falling_from_overbought_hint,  //
    rsi_trending_downward_hint,        //
                                       //
    macd_histogram_peaking_hint,       //
    ema_flattens_hint,                 //
    stop_proximity_hint,               //
                                       //
    price_trending_downward_hint,      //
    ema21_trending_downward_hint,      //
};

inline int severity_weight(Severity s) {
  switch (s) {
    case Severity::Low:
      return 1;
    case Severity::Medium:
      return 2;
    case Severity::High:
      return 3;
    case Severity::Urgent:
      return 4;
    default:
      return 0;
  }
}

SignalType Signal::gen_signal(const Metrics& m) const {
  if (entry_score >= 5 && exit_score <= 2)
    return SignalType::Entry;

  if (exit_score >= 5 && entry_score <= 2)
    return m.has_position() ? SignalType::Exit : SignalType::Caution;

  if (entry_score > 0 && exit_score > 0)
    return SignalType::Mixed;

  if (entry_score > 0 && entry_score < 5)
    return SignalType::Watchlist;

  if (exit_score > 0 && exit_score < 5)
    return m.has_position() ? SignalType::HoldCautiously : SignalType::Caution;

  return SignalType::None;
}

Signal::Signal(const Metrics& m) noexcept {
  // Hard entry signals
  for (auto f : entry_funcs)
    if (auto r = f(m); r.type != ReasonType::None) {
      entry_score += severity_weight(r.severity);
      entry_reasons.emplace_back(std::move(r));
    }

  // Hard exit signals
  for (auto f : exit_funcs)
    if (auto r = f(m); r.type != ReasonType::None) {
      exit_score += severity_weight(r.severity);
      exit_reasons.emplace_back(std::move(r));
    }

  type = gen_signal(m);

  // Entry hints
  for (auto f : entry_hint_funcs)
    if (auto h = f(m); h.str != "")
      entry_hints.emplace_back(std::move(h));

  // Exit hints
  for (auto f : exit_hint_funcs)
    if (auto h = f(m); h.str != "")
      exit_hints.emplace_back(std::move(h));

  auto sort = [](auto& v) { std::sort(v.begin(), v.end()); };
  sort(entry_reasons);
  sort(exit_reasons);
  sort(entry_hints);
  sort(exit_hints);
}

