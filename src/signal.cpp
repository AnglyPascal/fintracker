#include "signal.h"
#include "backtest.h"
#include "indicators.h"
#include "portfolio.h"

#include <iostream>

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

inline Reason ema_crossover_entry(const Metrics& m, int idx) {
  if (m.ema9(idx - 1) <= m.ema21(idx - 1) && m.ema9(idx) > m.ema21(idx))
    return ReasonType::EmaCrossover;
  return ReasonType::None;
}

inline Reason rsi_cross_50_entry(const Metrics& m, int idx) {
  if (m.rsi(idx - 1) < 50 && m.rsi(idx) >= 50)
    return ReasonType::RsiCross50;
  return ReasonType::None;
}

inline Reason pullback_bounce_entry(const Metrics& m, int idx) {
  bool dipped_below_ema21 = m.price(idx - 1) < m.ema21(idx - 1);
  bool recovered_above_ema21 = m.price(idx) > m.ema21(idx);
  bool ema9_above_ema21 = m.ema9(idx) > m.ema21(idx);

  if (dipped_below_ema21 && recovered_above_ema21 && ema9_above_ema21)
    return ReasonType::PullbackBounce;

  return ReasonType::None;
}

inline Reason macd_histogram_cross_entry(const Metrics& m, int idx) {
  if (m.hist(idx - 1) < 0 && m.hist(idx) >= 0)
    return ReasonType::MacdHistogramCross;
  return ReasonType::None;
}

// Exit reasons

inline Reason ema_crossdown_exit(const Metrics& m, int idx) {
  if (m.ema9(idx - 1) >= m.ema21(idx - 1) && m.ema9(idx) < m.ema21(idx))
    return ReasonType::EmaCrossdown;
  return ReasonType::None;
}

inline Reason macd_bearish_cross_exit(const Metrics& m, int idx) {
  if (m.macd(idx - 1) >= m.signal(idx - 1) && m.macd(idx) < m.signal(idx))
    return ReasonType::MacdBearishCross;
  return ReasonType::None;
}

inline Reason stop_loss_exit(const Metrics& m, int) {
  auto price = m.last_price();
  if (price < m.stop_loss.final_stop)
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

// Entry Hints
inline Hint ema_converging_hint(const Metrics& m, int idx) {
  double prev_dist = std::abs(m.ema9(idx - 1) - m.ema21(idx - 1));
  double curr_dist = std::abs(m.ema9(idx) - m.ema21(idx));

  if (m.ema9(idx) < m.ema21(idx) && curr_dist < prev_dist &&
      curr_dist / m.ema21(idx) < 0.01) {
    return HintType::Ema9ConvEma21;
  }

  return HintType::None;
}

inline Hint rsi_approaching_50_hint(const Metrics& m, int idx) {
  if (m.rsi(idx - 1) < m.rsi(idx) && m.rsi(idx) > 45 && m.rsi(idx) < 50)
    return HintType::RsiConv50;
  return HintType::None;
}

inline Hint macd_histogram_rising_hint(const Metrics& m, int idx) {
  if (m.hist(idx - 1) < m.hist(idx) && m.hist(idx) < 0)
    return HintType::MacdRising;
  return HintType::None;
}

// Exit Hints

inline Hint ema_diverging_hint(const Metrics& m, int idx) {
  double prev_dist = std::abs(m.ema9(idx - 1) - m.ema21(idx - 1));
  double curr_dist = std::abs(m.ema9(idx) - m.ema21(idx));

  if (m.ema9(idx) > m.ema21(idx) && curr_dist > prev_dist &&
      curr_dist / m.ema21(idx) > 0.02) {
    return HintType::Ema9DivergeEma21;
  }

  return HintType::None;
}

inline Hint rsi_falling_from_overbought_hint(const Metrics& m, int idx) {
  double prev = m.rsi(idx - 1), now = m.rsi(idx);
  if (prev > 70 && now < prev && prev - now > 3.0)
    return HintType::RsiDropFromOverbought;
  return HintType::None;
}

inline Hint macd_histogram_peaking_hint(const Metrics& m, int idx) {
  if (m.hist(idx - 2) < m.hist(idx - 1) && m.hist(idx - 1) > m.hist(idx))
    return HintType::MacdPeaked;
  return HintType::None;
}

inline Hint ema_flattens_hint(const Metrics& m, int idx) {
  double slope = m.ema9(idx) - m.ema9(idx - 1);
  bool is_flat = std::abs(slope / m.ema9(idx)) < 0.001;

  double dist = std::abs(m.ema9(idx) - m.ema21(idx));
  bool is_close = dist / m.ema21(idx) < 0.01;

  if (is_flat && is_close)
    return HintType::Ema9Flattening;

  return HintType::None;
}

inline Hint stop_proximity_hint(const Metrics& m, int) {
  auto price = m.last_price();
  auto dist = price - m.stop_loss.final_stop;
  auto dist_pct = dist / price;

  if (dist < 0)
    return HintType::StopInATR;

  if (dist_pct < 0.02)
    return HintType::StopProximity;

  if (dist < 1.0 * m.stop_loss.atr_stop - m.stop_loss.final_stop)
    return HintType::StopInATR;

  return HintType::None;
}

inline constexpr hint_f hint_funcs[] = {
    // Entry
    ema_converging_hint,
    rsi_approaching_50_hint,
    macd_histogram_rising_hint,
    // Exit
    ema_diverging_hint,
    rsi_falling_from_overbought_hint,
    macd_histogram_peaking_hint,
    ema_flattens_hint,
    stop_proximity_hint,
};

// Trends

inline Trend price_trending(const Metrics& m) {
  auto& top_trends = m.ind_1h.trends.price.top_trends;
  if (top_trends.empty())
    return TrendType::None;

  auto& best = top_trends[0];
  if (best.slope() > 0.2 && best.r2 > 0.8)
    return TrendType::PriceTrendingUp;
  if (best.slope() < -0.2 && best.r2 > 0.8)
    return TrendType::PriceTrendingDown;
  return TrendType::None;
}

inline Trend ema21_trending(const Metrics& m) {
  auto& top_trends = m.ind_1h.trends.ema21.top_trends;
  if (top_trends.empty())
    return TrendType::None;

  auto& best = top_trends[0];
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return TrendType::Ema21TrendingUp;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return TrendType::Ema21TrendingDown;
  return TrendType::None;
}

inline Trend rsi_trending(const Metrics& m) {
  auto& top_trends = m.ind_1h.trends.rsi.top_trends;
  if (top_trends.empty())
    return TrendType::None;

  auto& best = top_trends[0];
  if (best.slope() > 0.3 && best.r2 > 0.85)
    return TrendType::RsiTrendingUpStrongly;
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return TrendType::RsiTrendingUp;
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return TrendType::RsiTrendingDownStrongly;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return TrendType::RsiTrendingDown;
  return TrendType::None;
}

inline constexpr trend_f trend_funcs[] = {
    price_trending,
    ema21_trending,
    rsi_trending,
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
      std::any_of(hints.begin(), hints.end(), [](auto& h) {
        return h.cls == SignalClass::Exit &&
               (h.type == HintType::StopProximity ||
                h.severity == Severity::Urgent);
        ;
      });

  if ((exit_score > 0 && exit_score < 5) || exit_hints_block_watchlist)
    return has_position ? SignalType::HoldCautiously : SignalType::Caution;

  bool strong_entry_hint = std::any_of(hints.begin(), hints.end(), [](auto& h) {
    return h.cls == SignalClass::Entry && h.severity >= Severity::High;
  });
  bool strong_exit_hint = std::any_of(hints.begin(), hints.end(), [](auto& h) {
    return h.cls == SignalClass::Exit && h.severity >= Severity::High;
  });

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
  if ((m.price(-1) - m.ema21(-1)) / m.ema21(-1) > 0.0125) {
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
  if (m.rsi(-1) > 60 && (m.rsi(-1) - m.rsi(-2)) > 15)
    return "rsi quick spike";

  // MACD early crossover
  if (m.macd(-2) < m.signal(-2) && m.macd(-1) > m.signal(-1))
    return "macd early crossover";  // Crossover just happened

  // RSI weakening or divergence (advanced)
  if (ind.rsi.rising() == false && m.rsi(-1) < 60)
    return "rsi divergence or weak";

  return "ok";
}

inline constexpr conf_f confirmation_funcs[] = {
    entry_confirmation_15m,
};

Signal::Signal(const Metrics& m) noexcept {
  // Hard signals
  for (auto f : reason_funcs)
    if (auto r = f(m, -1);
        r.type != ReasonType::None && r.cls != SignalClass::None) {
      if (r.cls == SignalClass::Exit) {
        exit_score += severity_weight(r.severity);
      } else {
        entry_score += severity_weight(r.severity);
      }
      reasons.emplace_back(std::move(r));
    }

  // Hints
  for (auto f : hint_funcs)
    if (auto h = f(m, -1);
        h.type != HintType::None && h.cls != SignalClass::None) {
      hints.emplace_back(std::move(h));
    }

  // Trends
  for (auto f : trend_funcs)
    if (auto t = f(m); t.type != TrendType::None) {
      trends.emplace_back(std::move(t));
    }

  auto sort = [](auto& v) {
    std::sort(v.begin(), v.end(),
              [](auto& lhs, auto& rhs) { return lhs.severity < rhs.severity; });
  };

  sort(reasons);
  sort(hints);

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

  return important(reasons) || important(hints);
}

void Ticker::get_stats() {
  Backtest bt{metrics};

  for (auto& f : reason_funcs) {
    auto [r, s] = bt.get_stats<ReasonType>(f);
    reason_stats.try_emplace(r, s);
  }

  for (auto& f : hint_funcs) {
    auto [r, s] = bt.get_stats<HintType>(f);
    hint_stats.try_emplace(r, s);
  }
}
