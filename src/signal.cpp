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
    return Reason::EmaCrossover;
  return Reason::None;
}

inline Reason rsi_cross_50_entry(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (PREV(rsi) < 50 && LAST(rsi) >= 50)
    return Reason::RsiCross50;
  return Reason::None;
}

inline Reason pullback_bounce_entry(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  auto& price = m.indicators_1h.candles;

  if (ema9.empty() || ema21.empty() || price.size() < 2)
    return Reason::None;

  double close_prev = PREV(price).close;
  double close_now = LAST(price).close;

  bool dipped_below_ema21 = close_prev < PREV(ema21);
  bool recovered_above_ema21 = close_now > LAST(ema21);
  bool ema9_above_ema21 = LAST(ema9) > LAST(ema21);

  if (dipped_below_ema21 && recovered_above_ema21 && ema9_above_ema21)
    return Reason::PullbackBounce;

  return Reason::None;
}

inline Reason macd_histogram_cross_entry(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (PREV(hist) < 0 && LAST(hist) >= 0)
    return Reason::MacdHistogramCross;
  return Reason::None;
}

// Exit reasons

inline Reason ema_crossdown_exit(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (PREV(ema9) >= PREV(ema21) && LAST(ema9) < LAST(ema21))
    return Reason::EmaCrossdown;
  return Reason::None;
}

inline Reason rsi_overbought_exit(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (LAST(rsi) >= 70)
    return Reason::RsiOverbought;
  return Reason::None;
}

inline Reason macd_bearish_cross_exit(const Metrics& m) {
  auto& macd = m.indicators_1h.macd.macd_line;
  auto& signal = m.indicators_1h.macd.signal_line;
  if (macd.size() < 2 || signal.size() < 2)
    return Reason::None;

  if (PREV(macd) >= PREV(signal) && LAST(macd) < LAST(signal))
    return Reason::MacdBearishCross;
  return Reason::None;
}

inline Reason stop_loss_exit(const Metrics& m) {
  auto price = m.last_price();
  if (price < m.stop_loss.final_stop)
    return Reason::StopLossHit;
  return Reason::None;
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
inline std::string ema_converging_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (ema9.size() < 2 || ema21.size() < 2)
    return "";

  double prev_dist = std::abs(PREV(ema9) - PREV(ema21));
  double curr_dist = std::abs(LAST(ema9) - LAST(ema21));

  if (LAST(ema9) < LAST(ema21) && curr_dist < prev_dist &&
      curr_dist / LAST(ema21) < 0.01) {
    return "ema9 converging to ema21";
  }

  return "";
}

inline std::string rsi_approaching_50_hint(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (rsi.size() < 2)
    return "";

  if (PREV(rsi) < LAST(rsi) && LAST(rsi) > 45 && LAST(rsi) < 50)
    return "rsi approaching 50";

  return "";
}

inline std::string macd_histogram_rising_hint(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (hist.size() < 2)
    return "";

  if (PREV(hist) < LAST(hist) && LAST(hist) < 0)
    return "macd rising";

  return "";
}

// Exit Hints

inline std::string rsi_falling_from_overbought_hint(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (rsi.size() < 2)
    return "";

  if (PREV(rsi) > 70 && LAST(rsi) < PREV(rsi))
    return "rsi falling from overbought";

  return "";
}

inline std::string macd_histogram_peaking_hint(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (hist.size() < 3)
    return "";

  double h2 = hist[hist.size() - 3];
  double h1 = hist[hist.size() - 2];
  double h0 = hist.back();

  if (h2 < h1 && h1 > h0)
    return "macd histogram peaked";

  return "";
}

inline std::string ema_flattens_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (ema9.size() < 3 || ema21.size() < 3)
    return "";

  double slope = LAST(ema9) - PREV(ema9);
  bool is_flat = std::abs(slope / LAST(ema9)) < 0.001;

  double dist = std::abs(LAST(ema9) - LAST(ema21));
  bool is_close = dist / LAST(ema21) < 0.01;

  if (is_flat && is_close) {
    return "ema9 flattening near ema21";
  }

  return "";
}

inline std::string stop_proximity_hint(const Metrics& m) {
  auto price = m.last_price();
  auto dist = price - m.stop_loss.final_stop;
  auto dist_pct = dist / price;

  if (dist < 0)
    return "";

  if (dist_pct < 0.02)
    return "stop very close";
  else if (dist < 1.0 * m.stop_loss.atr_stop - m.stop_loss.final_stop)
    return "stop inside ATR range";

  return "";
}

inline constexpr hint_f entry_hint_funcs[] = {
    ema_converging_hint,
    rsi_approaching_50_hint,
    macd_histogram_rising_hint,
};

inline constexpr hint_f exit_hint_funcs[] = {
    rsi_falling_from_overbought_hint,
    macd_histogram_peaking_hint,
    ema_flattens_hint,
    stop_proximity_hint,
};

Signal::Signal(const Metrics& m) noexcept {
  // Hard entry signals
  for (auto& f : entry_funcs) {
    Reason r = f(m);
    if (r != Reason::None) {
      type = SignalType::Entry;
      entry_reasons.push_back(r);
    }
  }

  // Hard exit signals
  for (auto f : exit_funcs) {
    Reason r = f(m);
    if (r != Reason::None) {
      type = SignalType::Exit;
      exit_reasons.push_back(r);
    }
  }

  // Entry hints
  for (auto f : entry_hint_funcs) {
    auto msg = f(m);
    if (msg != "")
      entry_hints.push_back(msg);
  }

  // Exit hints
  for (auto f : exit_hint_funcs) {
    auto msg = f(m);
    if (msg != "")
      exit_hints.push_back(msg);
  }
}
