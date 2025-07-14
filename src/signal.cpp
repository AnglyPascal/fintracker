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

// Filters

Filter evaluate_daily_trend(const std::vector<Candle>& candles,
                            minutes interval) {
  Indicators ind{downsample(candles, interval, D_1), D_1};

  auto& ema = ind.trends.ema21.top_trends;
  auto& rsi = ind.rsi.values;

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

Filter evaluate_four_hour_trend(const std::vector<Candle>& candles,
                                minutes interval) {
  Indicators ind{downsample(candles, interval, H_4), H_4};

  auto& ema_trend = ind.trends.ema21.top_trends;
  auto& rsi_vals = ind.rsi.values;
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

inline constexpr filter_f filters[] = {
    evaluate_daily_trend,
    evaluate_four_hour_trend,
};

std::pair<bool, std::string> filter(const std::vector<Candle>& candles,
                                    minutes interval) {
  if (candles.empty())
    return {false, ""};

  bool res = true;
  std::string str = "";

  for (auto f : filters) {
    auto [c, s] = f(candles, interval);
    res &= (c != Confidence::Bearish);
    str += s + " ";
  }

  return {res, str};
}

// Entry reasons

inline Reason ema_crossover_entry(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (PREV(ema9) <= PREV(ema21) && LAST(ema9) > LAST(ema21))
    return {ReasonType::EmaCrossover, Severity::High, Source::EMA};
  return ReasonType::None;
}

inline Reason rsi_cross_50_entry(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (PREV(rsi) < 50 && LAST(rsi) >= 50)
    return {ReasonType::RsiCross50, Severity::Medium, Source::RSI};
  return ReasonType::None;
}

inline Reason pullback_bounce_entry(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  auto& candles = m.indicators_1h.candles;

  bool dipped_below_ema21 = PREV(candles).price() < PREV(ema21);
  bool recovered_above_ema21 = LAST(candles).price() > LAST(ema21);
  bool ema9_above_ema21 = LAST(ema9) > LAST(ema21);

  if (dipped_below_ema21 && recovered_above_ema21 && ema9_above_ema21)
    return {ReasonType::PullbackBounce, Severity::Urgent, Source::Price};

  return ReasonType::None;
}

inline Reason macd_histogram_cross_entry(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (PREV(hist) < 0 && LAST(hist) >= 0)
    return {ReasonType::MacdHistogramCross, Severity::Medium, Source::MACD};
  return ReasonType::None;
}

// Exit reasons

inline Reason ema_crossdown_exit(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;
  if (PREV(ema9) >= PREV(ema21) && LAST(ema9) < LAST(ema21))
    return {ReasonType::EmaCrossdown, Severity::High, Source::EMA};
  return ReasonType::None;
}

inline Reason macd_bearish_cross_exit(const Metrics& m) {
  auto& macd = m.indicators_1h.macd.macd_line;
  auto& signal = m.indicators_1h.macd.signal_line;
  if (PREV(macd) >= PREV(signal) && LAST(macd) < LAST(signal))
    return {ReasonType::MacdBearishCross, Severity::High, Source::MACD};
  return ReasonType::None;
}

inline Reason stop_loss_exit(const Metrics& m) {
  auto price = m.last_price();
  if (price < m.stop_loss.final_stop)
    return {ReasonType::StopLossHit, Severity::Urgent, Source::Stop};
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
    macd_bearish_cross_exit,
    stop_loss_exit,
};

// Entry Hints
inline Hint ema_converging_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;

  double prev_dist = std::abs(PREV(ema9) - PREV(ema21));
  double curr_dist = std::abs(LAST(ema9) - LAST(ema21));

  if (LAST(ema9) < LAST(ema21) && curr_dist < prev_dist &&
      curr_dist / LAST(ema21) < 0.01) {
    return {HintType::Ema9ConvEma21, Severity::Low, Source::EMA};
  }

  return HintType::None;
}

inline Hint rsi_approaching_50_hint(const Metrics& m) {
  auto& rsi = m.indicators_1h.rsi.values;
  if (PREV(rsi) < LAST(rsi) && LAST(rsi) > 45 && LAST(rsi) < 50)
    return {HintType::RsiConv50, Severity::Low, Source::RSI};
  return HintType::None;
}

inline Hint macd_histogram_rising_hint(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;
  if (PREV(hist) < LAST(hist) && LAST(hist) < 0)
    return {HintType::MacdRising, Severity::Medium, Source::MACD};
  return HintType::None;
}

// Exit Hints

inline Hint ema_diverging_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;

  double prev_dist = std::abs(PREV(ema9) - PREV(ema21));
  double curr_dist = std::abs(LAST(ema9) - LAST(ema21));

  if (LAST(ema9) > LAST(ema21) && curr_dist > prev_dist &&
      curr_dist / LAST(ema21) > 0.02) {
    return {HintType::Ema9DivergeEma21, Severity::Low, Source::EMA};
  }

  return HintType::None;
}

inline Hint rsi_falling_from_overbought_hint(const Metrics& m) {
  double prev = PREV(m.indicators_1h.rsi.values);
  double now = LAST(m.indicators_1h.rsi.values);
  if (prev > 70 && now < prev && prev - now > 3.0)
    return {HintType::RsiDropFromOverbought, Severity::Medium, Source::RSI};
  return HintType::None;
}

inline Hint macd_histogram_peaking_hint(const Metrics& m) {
  auto& hist = m.indicators_1h.macd.histogram;

  double h2 = hist[hist.size() - 3];
  double h1 = hist[hist.size() - 2];
  double h0 = hist.back();

  if (h2 < h1 && h1 > h0)
    return {HintType::MacdPeaked, Severity::Medium, Source::MACD};

  return HintType::None;
}

inline Hint ema_flattens_hint(const Metrics& m) {
  auto& ema9 = m.indicators_1h.ema9.values;
  auto& ema21 = m.indicators_1h.ema21.values;

  double slope = LAST(ema9) - PREV(ema9);
  bool is_flat = std::abs(slope / LAST(ema9)) < 0.001;

  double dist = std::abs(LAST(ema9) - LAST(ema21));
  bool is_close = dist / LAST(ema21) < 0.01;

  if (is_flat && is_close)
    return {HintType::Ema9Flattening, Severity::Low, Source::EMA};

  return HintType::None;
}

inline Hint stop_proximity_hint(const Metrics& m) {
  auto price = m.last_price();
  auto dist = price - m.stop_loss.final_stop;
  auto dist_pct = dist / price;

  if (dist < 0)
    return HintType::StopInATR;

  if (dist_pct < 0.02)
    return {HintType::StopProximity, Severity::High, Source::Stop};

  if (dist < 1.0 * m.stop_loss.atr_stop - m.stop_loss.final_stop)
    return {HintType::StopInATR, Severity::High, Source::Stop};

  return HintType::None;
}

inline Hint price_trending_upward_hint(const Metrics& m) {
  auto& best = m.indicators_1h.trends.price.top_trends[0];
  if (best.slope() > 0.2 && best.r2 > 0.8)
    return {HintType::PriceTrendingUp, Severity::Medium, Source::Price};
  return HintType::None;
}

inline Hint price_trending_downward_hint(const Metrics& m) {
  auto& price = m.indicators_1h.trends.price;
  auto& best = price.top_trends[0];
  if (best.slope() < -0.2 && best.r2 > 0.8)
    return {HintType::PriceTrendingDown, Severity::Medium, Source::Price};
  return HintType::None;
}

inline Hint ema21_trending_upward_hint(const Metrics& m) {
  auto& ema21 = m.indicators_1h.trends.ema21;
  auto& best = ema21.top_trends[0];
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return {HintType::Ema21TrendingUp, Severity::Medium, Source::EMA};
  return HintType::None;
}

inline Hint ema21_trending_downward_hint(const Metrics& m) {
  auto& ema21 = m.indicators_1h.trends.ema21;
  auto& best = ema21.top_trends[0];
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return {HintType::Ema21TrendingDown, Severity::Medium, Source::EMA};
  return HintType::None;
}

inline Hint rsi_trending_upward_hint(const Metrics& m) {
  auto& best = m.indicators_1h.trends.rsi.top_trends[0];
  if (best.slope() > 0.3 && best.r2 > 0.85)
    return {HintType::RsiTrendingUpStrongly, Severity::High, Source::RSI};
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return {HintType::RsiTrendingUp, Severity::Medium, Source::RSI};
  return HintType::None;
}

inline Hint rsi_trending_downward_hint(const Metrics& m) {
  auto& best = m.indicators_1h.trends.rsi.top_trends[0];
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return {HintType::RsiTrendingDownStrongly, Severity::High, Source::RSI};
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return {HintType::RsiTrendingDown, Severity::Medium, Source::RSI};
  return HintType::None;
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
    ema_diverging_hint,                //
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
  return static_cast<int>(s);
}

SignalType Signal::gen_signal(bool has_position) const {
  if (entry_score >= 6 && exit_score <= 2)
    return SignalType::Entry;

  if (exit_score >= 5 && entry_score <= 2)
    return has_position ? SignalType::Exit : SignalType::Caution;

  if (entry_score > 0 && entry_score < 5)
    return SignalType::Watchlist;

  bool exit_hints_block_watchlist =
      std::any_of(exit_hints.begin(), exit_hints.end(), [](auto& h) {
        return h.type == HintType::StopProximity ||
               h.severity == Severity::Urgent;
      });

  if ((exit_score > 0 && exit_score < 5) || exit_hints_block_watchlist)
    return has_position ? SignalType::HoldCautiously : SignalType::Caution;

  bool strong_entry_hint =
      !entry_hints.empty() && entry_hints.front().severity >= Severity::High;
  bool strong_exit_hint =
      !exit_hints.empty() && exit_hints.front().severity >= Severity::High;

  if (strong_entry_hint && strong_exit_hint)
    return SignalType::Mixed;

  if (strong_entry_hint)
    return SignalType::Watchlist;

  if (strong_exit_hint)
    return has_position ? SignalType::HoldCautiously : SignalType::Caution;

  if (entry_score > 0 && exit_score > 0)
    return SignalType::Mixed;

  return SignalType::None;
}

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

  if (n < 5 || ind.ema21.values.size() < 1 || ind.rsi.values.size() < 2)
    return "";

  if (!volume_above_ma(candles))
    return "low volume";

  auto price = candles.back().price();
  auto ema21 = LAST(ind.ema21.values);
  auto rsi_now = LAST(ind.rsi.values);
  auto rsi_prev = PREV(ind.rsi.values);

  // Overextension above EMA21
  if ((price - ema21) / ema21 > 0.0125) {
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
  if (rsi_now > 60 && (rsi_now - rsi_prev) > 15)
    return "rsi quick spike";

  // MACD early crossover
  auto& macd_line = ind.macd.macd_line;
  auto& signal_line = ind.macd.signal_line;
  if (macd_line.size() >= 2 && signal_line.size() >= 2) {
    double macd_now = LAST(macd_line);
    double macd_prev = PREV(macd_line);

    double sig_now = LAST(signal_line);
    double sig_prev = PREV(signal_line);

    if (macd_prev < sig_prev && macd_now > sig_now)
      return "macd early crossover";  // Crossover just happened
  }

  // RSI weakening or divergence (advanced)
  if (ind.rsi.rising() == false && rsi_now < 60)
    return "rsi divergence or weak";

  return "ok";
}

inline constexpr conf_f confirmation_funcs[] = {
    entry_confirmation_15m,
};

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

  // Entry hints
  for (auto f : entry_hint_funcs)
    if (auto h = f(m); h.type != HintType::None)
      entry_hints.emplace_back(std::move(h));

  // Exit hints
  for (auto f : exit_hint_funcs)
    if (auto h = f(m); h.type != HintType::None)
      exit_hints.emplace_back(std::move(h));

  auto sort = [](auto& v) {
    std::sort(v.begin(), v.end(),
              [](auto& lhs, auto& rhs) { return lhs.severity < rhs.severity; });
  };

  sort(entry_reasons);
  sort(exit_reasons);
  sort(entry_hints);
  sort(exit_hints);

  type = gen_signal(m.has_position());
  if (type != SignalType::Entry) {
    confirmations.clear();
    return;
  }

  for (auto f : confirmation_funcs)
    if (auto conf = f(m); conf.str != "")
      confirmations.push_back(conf);
}

bool Signal::is_interesting() const {
  if (type == SignalType::Entry || type == SignalType::Exit ||
      type == SignalType::HoldCautiously)
    return true;

  auto important = [](auto& iter) {
    for (auto& a : iter)
      if (a.severity >= Severity::High)
        return true;
    return false;
  };

  if (important(entry_reasons) || important(exit_reasons))
    return true;

  if (important(entry_hints) || important(exit_hints))
    return true;

  return false;
}
