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
};

inline const std::unordered_map<TrendType, Meta> trend_meta = {
    {TrendType::None,  //
     {Severity::Low, Source::None, SignalClass::None, ""}},
    // Entry
    {TrendType::PriceUp,  //
     {Severity::Medium, Source::EMA, SignalClass::Entry, "price↗"}},
    {TrendType::Ema21Up,  //
     {Severity::Medium, Source::RSI, SignalClass::Entry, "ema21↗"}},
    {TrendType::RsiUp,  //
     {Severity::Medium, Source::MACD, SignalClass::Entry, "rsi↗"}},
    {TrendType::RsiUpStrongly,  //
     {Severity::Medium, Source::EMA, SignalClass::Exit, "rsi⇗"}},
    // Exit
    {TrendType::PriceDown,  //
     {Severity::Medium, Source::EMA, SignalClass::Exit, "price↘"}},
    {TrendType::Ema21Down,  //
     {Severity::Medium, Source::RSI, SignalClass::Exit, "ema21↘"}},
    {TrendType::RsiDown,  //
     {Severity::Medium, Source::MACD, SignalClass::Exit, "price↘"}},
    {TrendType::RsiDownStrongly,  //
     {Severity::Medium, Source::EMA, SignalClass::Exit, "rsi⇘"}},
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

template <>
SignalType<TrendType, TrendType::None>::SignalType(TrendType type)
    : type{type} {
  auto it = trend_meta.find(type);
  meta = it == trend_meta.end() ? nullptr : &it->second;
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

inline Hint price_pullback_hint(const Metrics& m, int idx) {
  if (m.price(idx - 1) > m.ema21(idx - 1) && m.price(idx) < m.ema21(idx))
    return HintType::Pullback;
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

inline Hint stop_proximity_hint(const Metrics& m, int idx) {
  auto price = m.last_price();
  auto dist = price - m.stop_loss.final_stop;

  if (dist < 0)
    return HintType::StopInATR;

  if (dist < m.atr(idx) * 0.75)
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
    price_pullback_hint,
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
    return TrendType::PriceUp;
  if (best.slope() < -0.2 && best.r2 > 0.8)
    return TrendType::PriceDown;
  return TrendType::None;
}

inline Trend ema21_trending(const Metrics& m) {
  auto& top_trends = m.ind_1h.trends.ema21.top_trends;
  if (top_trends.empty())
    return TrendType::None;

  auto& best = top_trends[0];
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return TrendType::Ema21Up;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return TrendType::Ema21Down;
  return TrendType::None;
}

inline Trend rsi_trending(const Metrics& m) {
  auto& top_trends = m.ind_1h.trends.rsi.top_trends;
  if (top_trends.empty())
    return TrendType::None;

  auto& best = top_trends[0];
  if (best.slope() > 0.3 && best.r2 > 0.85)
    return TrendType::RsiUpStrongly;
  if (best.slope() > 0.15 && best.r2 > 0.8)
    return TrendType::RsiUp;
  if (best.slope() < -0.3 && best.r2 > 0.85)
    return TrendType::RsiDownStrongly;
  if (best.slope() < -0.15 && best.r2 > 0.8)
    return TrendType::RsiDown;
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

bool Signal::is_interesting() const {
  if (type == Rating::Entry || type == Rating::Exit ||
      type == Rating::HoldCautiously)
    return true;

  auto important = [](auto& iter) {
    for (auto& a : iter)
      if (a.severity() >= Severity::High)
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

inline constexpr double ENTRY_MIN = 1.0;  // ignore entry_w < 1.0 entirely
inline constexpr double EXIT_MIN = 1.0;   // ignore exit_w  < 1.0 entirely
inline constexpr double ENTRY_THRESHOLD = 3.5;
inline constexpr double EXIT_THRESHOLD = 3.0;
inline constexpr double MIXED_MIN = 1.2;  // require at least this on both sides
inline constexpr double WATCHLIST_THRESHOLD = ENTRY_THRESHOLD;  // for symmetry

inline Rating gen_rating(double entry_w,
                         double exit_w,
                         double /* past_score */,
                         bool has_position,
                         bool has_reason,
                         auto& hints) {
  // 1. Strong Entry
  if (entry_w >= ENTRY_THRESHOLD && exit_w <= WATCHLIST_THRESHOLD && has_reason)
    return Rating::Entry;

  // 2. Strong Exit
  if (exit_w >= EXIT_THRESHOLD && entry_w <= WATCHLIST_THRESHOLD && has_reason)
    return has_position ? Rating::Exit : Rating::Caution;

  // 3. Mixed
  if (entry_w >= MIXED_MIN && exit_w >= MIXED_MIN)
    return Rating::Mixed;

  // 4. Moderate Exit or urgent exit hint
  bool exit_hints_block_watchlist =
      std::any_of(hints.begin(), hints.end(), [](auto& h) {
        return h.cls() == SignalClass::Exit &&
               (h.type == HintType::StopInATR ||
                h.severity() == Severity::Urgent);
      });

  if ((exit_w >= EXIT_MIN && exit_w < EXIT_THRESHOLD) ||
      (exit_hints_block_watchlist && exit_w >= EXIT_MIN)) {
    return has_position ? Rating::HoldCautiously : Rating::Caution;
  }

  // 5. Entry bias
  if (entry_w >= ENTRY_MIN && entry_w < ENTRY_THRESHOLD)
    return Rating::Watchlist;

  // 6. Strong hint conflicts and fallbacks
  bool strong_entry_hint =
      std::any_of(hints.begin(), hints.end(), [](const auto& h) {
        return h.cls() == SignalClass::Entry && h.severity() >= Severity::High;
      });
  bool strong_exit_hint =
      std::any_of(hints.begin(), hints.end(), [](const auto& h) {
        return h.cls() == SignalClass::Exit && h.severity() >= Severity::High;
      });

  if (strong_entry_hint && strong_exit_hint)
    return Rating::Mixed;
  if (strong_entry_hint)
    return Rating::Watchlist;
  if (strong_exit_hint)
    return has_position ? Rating::HoldCautiously : Rating::Caution;

  return Rating::None;
}

inline constexpr double weight(double importance, double severity) {
  return importance * severity;
}

inline double signal_score(double entry_w,
                           double exit_w,
                           double past_score,
                           bool has_position) {
  double raw = has_position ? entry_w - 1.5 * exit_w : 1.2 * entry_w - exit_w;
  double curr_score = std::tanh((raw) / 3.0);  // squashes to [-1,1]
  constexpr double alpha = 0.7;
  return curr_score * 0.7 + past_score * (1 - alpha);
}

Signal Ticker::gen_signal(int idx) const {
  Signal s;
  double entry_w = 0.0, exit_w = 0.0;
  auto& m = metrics;

  // Hard signals
  for (auto f : reason_funcs) {
    auto r = f(m, idx);
    if (r.type != ReasonType::None && r.cls() != SignalClass::None) {
      auto importance = 0.0;
      if (auto it = reason_stats.find(r.type); it != reason_stats.end())
        importance = it->second.importance;
      if (r.source() == Source::Stop)
        importance = 0.8;

      auto severity = severity_weight(r.severity());
      auto w = weight(importance, severity);

      if (r.cls() == SignalClass::Exit)
        exit_w += w;
      else
        entry_w += w;

      s.reasons.emplace_back(std::move(r));
    }
  }

  // Hints
  for (auto f : hint_funcs) {
    auto h = f(m, idx);
    if (h.type != HintType::None && h.cls() != SignalClass::None) {
      if (h.severity() < Severity::High)
        continue;

      auto importance = 0.0;
      if (auto it = hint_stats.find(h.type); it != hint_stats.end())
        importance = it->second.importance;
      if (h.source() == Source::Stop)
        importance = 0.8;

      auto severity = severity_weight(h.severity());
      // slightly lower weight than reasons
      auto w = 0.7 * weight(importance, severity);

      if (h.cls() == SignalClass::Exit)
        exit_w += w;
      else
        entry_w += w;

      s.hints.emplace_back(std::move(h));
    }
  }

  // Trends
  for (auto f : trend_funcs)
    if (auto t = f(m); t.type != TrendType::None) {
      s.trends.emplace_back(std::move(t));
    }

  auto sort = [](auto& v) {
    std::sort(v.begin(), v.end(), [](auto& lhs, auto& rhs) {
      return lhs.severity() < rhs.severity();
    });
  };

  sort(s.reasons);
  sort(s.hints);

  auto past_score = memory.score();
  s.type = gen_rating(entry_w, exit_w, past_score, m.has_position(),
                      s.has_reasons(), s.hints);
  s.score = signal_score(entry_w, exit_w, past_score, m.has_position());

  if (s.type != Rating::Entry)
    s.confirmations.clear();

  for (auto f : confirmation_funcs)
    if (auto conf = f(m); conf.str != "")
      s.confirmations.push_back(conf);

  return s;
}

double SignalMemory::score() const {
  double scr = 0.0;
  double weight = 0.0;

  double decay = 0.5;
  for (auto& sig : past) {
    scr = scr * decay + sig.score;
    weight = weight * decay + decay;
  }

  return weight > 0.0 ? scr / weight : 0.0;
}
