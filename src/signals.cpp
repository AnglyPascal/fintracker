#include "backtest.h"
#include "config.h"
#include "indicators.h"
#include "portfolio.h"

#include <cmath>

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

inline const SignalConfig& sig_config = config.sig_config;

inline Rating gen_rating(double entry_w,
                         double exit_w,
                         double /* past_score */,
                         bool has_reason,
                         auto& hints) {
  // 1. Strong Entry
  if (entry_w >= sig_config.entry_threshold &&
      exit_w <= sig_config.watchlist_threshold && has_reason)
    return Rating::Entry;

  // 2. Strong Exit
  if (exit_w >= sig_config.exit_threshold &&
      entry_w <= sig_config.watchlist_threshold && has_reason)
    return Rating::Exit;

  // 3. Mixed
  if (entry_w >= sig_config.mixed_min && exit_w >= sig_config.mixed_min)
    return Rating::Mixed;

  // 4. Moderate Exit or urgent exit hint
  bool exit_hints_block_watchlist =
      std::any_of(hints.begin(), hints.end(), [](auto& h) {
        return h.cls() == SignalClass::Exit && h.severity() == Severity::Urgent;
      });

  if ((exit_w >= sig_config.exit_min && exit_w < sig_config.exit_threshold) ||
      (exit_hints_block_watchlist && exit_w >= sig_config.exit_min)) {
    return Rating::Caution;
  }

  // 5. Entry bias
  if (entry_w >= sig_config.entry_min && entry_w < sig_config.entry_threshold)
    return Rating::Watchlist;

  // 6. Fallbacks
  bool strong_entry_hint =
      std::any_of(hints.begin(), hints.end(), [](const auto& h) {
        return h.cls() == SignalClass::Entry &&
               h.severity() >= Severity::High && h.source() != Source::SR;
      });
  bool strong_exit_hint =
      std::any_of(hints.begin(), hints.end(), [](const auto& h) {
        return h.cls() == SignalClass::Exit && h.severity() >= Severity::High &&
               h.source() != Source::SR;
      });

  if (strong_entry_hint && strong_exit_hint)
    return Rating::Mixed;
  if (strong_entry_hint)
    return Rating::Watchlist;
  if (strong_exit_hint)
    return Rating::Caution;

  return Rating::None;
}

inline constexpr double weight(double importance, double severity) {
  return importance * severity;
}

inline int severity_weight(Severity s) {
  return static_cast<int>(s);
}

template <typename T>
std::string to_str(const T& t);

inline double signal_score(double entry_w, double exit_w, double past_score) {
  auto raw = sig_config.score_entry_weight * entry_w - exit_w;
  double curr_score = std::tanh(raw / 3.0);  // squashes to [-1,1]
  return curr_score * sig_config.score_curr_alpha +
         past_score * (1 - sig_config.score_curr_alpha);
}

Signal Indicators::gen_signal(int idx) const {
  Signal s;
  s.tp = time(idx);

  double entry_w = 0.0, exit_w = 0.0;

  // Hard signals
  for (auto r : reasons(*this, idx)) {
    if (r.type != ReasonType::None && r.cls() != SignalClass::None) {
      s.reasons.emplace_back(r);

      auto importance = 0.0;
      if (auto it = reason_stats.find(r.type); it != reason_stats.end())
        importance = it->second.importance;
      if (r.source() == Source::Stop)
        importance = sig_config.stop_reason_importance;

      auto severity = severity_weight(r.severity());
      auto w = weight(importance, severity);

      if (r.cls() == SignalClass::Exit)
        exit_w += w;
      else
        entry_w += w;
    }
  }

  // Hints
  for (auto h : hints(*this, idx)) {
    if (h.type != HintType::None && h.cls() != SignalClass::None) {
      s.hints.emplace_back(h);
      if (h.severity() < Severity::High)
        continue;

      auto importance = 0.0;
      if (auto it = hint_stats.find(h.type); it != hint_stats.end())
        importance = it->second.importance;
      if (h.source() == Source::Stop)
        importance = sig_config.stop_hint_importance;

      auto severity = severity_weight(h.severity());
      auto w = sig_config.score_hint_weight * weight(importance, severity);

      if (h.cls() == SignalClass::Exit)
        exit_w += w;
      else
        entry_w += w;
    }
  }

  auto sort = [](auto& v) {
    std::sort(v.begin(), v.end(), [](auto& lhs, auto& rhs) {
      return lhs.severity() < rhs.severity();
    });
  };

  sort(s.reasons);
  sort(s.hints);

  auto past_score = memory.score();
  s.type = gen_rating(entry_w, exit_w, past_score, s.has_reasons(), s.hints);
  s.score = signal_score(entry_w, exit_w, past_score);

  return s;
}

Signal Metrics::get_signal(minutes interval, int idx) const {
  if (interval == H_1)
    return idx == -1 ? ind_1h.signal : ind_1h.gen_signal(idx);
  else if (interval == H_4)
    return idx == -1 ? ind_4h.signal : ind_4h.gen_signal(idx);
  else if (interval == D_1)
    return idx == -1 ? ind_1d.signal : ind_1d.gen_signal(idx);
  return {};
}

inline bool disqualify(auto& filters) {
  int strongBearish1D = 0;
  int weakBearish1D = 0;
  int bearish4H = 0;

  for (const auto& f : filters.trends_1d) {
    if (f.trend == Trend::Bearish) {
      if (f.confidence == Confidence::High)
        ++strongBearish1D;
      else
        ++weakBearish1D;
    }
  }

  for (const auto& f : filters.trends_4h) {
    if (f.trend == Trend::Bearish)
      ++bearish4H;
  }

  if (strongBearish1D > 0)
    return true;
  if (bearish4H >= 2 && weakBearish1D > 0)
    return true;

  // allow if only 4H is weakly bearish and daily is neutral/bullish
  return false;
}

Rating contextual_rating(auto& sig_1h,
                         auto& sig_4h,
                         auto& sig_1d,
                         auto& filters,
                         bool has_position,
                         auto& stop_hit) {
  Rating base_rating = sig_1h.type;

  if (disqualify(filters))
    return Rating::Skip;

  if (base_rating == Rating::Caution && has_position)
    base_rating = Rating::HoldCautiously;

  if (base_rating == Rating::Exit && !has_position)
    base_rating = Rating::Caution;

  if (stop_hit.type == StopHitType::TimeExit)
    base_rating = Rating::Exit;
  if (stop_hit.type == StopHitType::StopLossHit)
    base_rating = Rating::Exit;
  else if (stop_hit.type == StopHitType::StopInATR)
    base_rating = Rating::HoldCautiously;
  else if (stop_hit.type == StopHitType::StopProximity) {
    if (base_rating == Rating::Watchlist)
      base_rating = Rating::Mixed;
    else if (base_rating == Rating::Mixed || base_rating == Rating::Caution)
      base_rating = Rating::HoldCautiously;
  }

  // Upgrade to Entry if higher timeframes show strong entry signals
  if (base_rating == Rating::Watchlist) {
    if (sig_4h.type == Rating::Watchlist &&
        sig_4h.score > sig_config.entry_4h_score_confirmation)
      return Rating::Entry;
    if (sig_1d.type == Rating::Watchlist &&
        sig_1d.score > sig_config.entry_1d_score_confirmation)
      return Rating::Entry;
  }

  // Higher timeframe exits should be respected
  if (sig_4h.type == Rating::Exit || sig_1d.type == Rating::Exit ||
      (sig_4h.type == Rating::HoldCautiously &&
       sig_4h.score < sig_config.exit_4h_score_confirmation) ||
      (sig_1d.type == Rating::HoldCautiously &&
       sig_1d.score < sig_config.exit_1d_score_confirmation)) {
    if (base_rating == Rating::Entry || base_rating == Rating::Watchlist)
      return Rating::Mixed;  // Conflict requires caution
    if (base_rating == Rating::HoldCautiously)
      return Rating::Exit;  // Escalate to exit
  }

  return base_rating;
}

inline double weighted_score(double score_1h,
                             double score_4h,
                             double score_1d) {
  double base_score = score_1h;

  if ((score_1h > 0 && score_4h > 0) || (score_1h > 0 && score_1d > 0))
    base_score *= sig_config.score_mod_4h_1d_agree;

  if (score_1h > 0 && score_4h > 0 && score_1d > 0)
    base_score *= sig_config.score_mod_4h_1d_align;

  if ((score_1h > 0 && score_4h < -0.5) || (score_1h > 0 && score_1d < -0.5))
    base_score *= sig_config.score_mod_4h_1d_conflict;

  return base_score;
}

StopHit stop_loss_hits(const Metrics& m, const StopLoss& stop_loss);

CombinedSignal Ticker::gen_signal(int idx) const {
  auto sig_1h = metrics.get_signal(H_1, idx);
  auto sig_4h = metrics.get_signal(H_4, idx);
  auto sig_1d = metrics.get_signal(D_1, idx);

  auto& ind_1h = metrics.ind_1h;

  CombinedSignal sig;

  sig.stop_hit = stop_loss_hits(metrics, stop_loss);
  sig.forecast = ind_1h.gen_forecast(idx);

  sig.filters = evaluate_filters(metrics);
  sig.type = contextual_rating(sig_1h, sig_4h, sig_1d, sig.filters,
                               metrics.has_position(), sig.stop_hit);
  sig.score = weighted_score(sig_1h.score, sig_4h.score, sig_1d.score);

  if (sig.type == Rating::Entry) {
    for (auto conf : confirmations(metrics))
      if (conf.str != "")
        sig.confirmations.push_back(conf);

    // disqualify if earnings is near
    if (ev.is_earnings() && ev.days_until() >= 0 &&
        ev.days_until() < config.sizing_config.earnings_volatility_buffer)
      sig.type = Rating::Watchlist;
  }

  return sig;
}

double SignalMemory::score() const {
  double scr = 0.0;
  double weight = 0.0;

  double decay = sig_config.memory_score_decay;
  for (auto& sig : past) {
    scr = scr * decay + sig.score;
    weight = weight * decay + decay;
  }

  return weight > 0.0 ? scr / weight : 0.0;
}

Forecast::Forecast(const Signal& sig,
                   const std::map<ReasonType, SignalStats>& reason_stats,
                   const std::map<HintType, SignalStats>& hint_stats) {
  double ret_sum = 0.0, dd_sum = 0.0;
  double ret_weight = 0.0, dd_weight = 0.0;

  auto process = [&](auto& obj, auto& stats_map) {
    auto it = stats_map.find(obj.type);
    if (it == stats_map.end())
      return;

    const SignalStats& stats = it->second;
    const bool is_entry = obj.cls() == SignalClass::Entry;
    const double imp = stats.importance;

    // Forecast return
    if (is_entry) {
      // Use importance as-is
      ret_sum += stats.avg_return * imp;
      ret_weight += imp;
    } else {
      // High importance = bad exit -> suppress expected return
      // So weight by 1/importance or similar
      if (imp > 0) {
        double w = 1.0 / imp;
        ret_sum += stats.avg_return * w;
        ret_weight += w;
      }
    }

    // Forecast drawdown
    if (is_entry) {
      // Low drawdown is good -> low importance -> suppress contribution
      // So only mildly weight these
      dd_sum += stats.avg_drawdown * (imp * 0.5);  // or just skip them entirely
      dd_weight += imp * 0.5;
    } else {
      // High drawdown = high importance -> weight directly
      dd_sum += stats.avg_drawdown * imp;
      dd_weight += imp;
    }
  };

  for (const auto& r : sig.reasons)
    process(r, reason_stats);
  for (const auto& h : sig.hints)
    process(h, hint_stats);

  if (ret_weight > 0.0)
    expected_return = ret_sum / ret_weight;
  if (dd_weight > 0.0)
    expected_drawdown = dd_sum / dd_weight;

  confidence =
      std::max(ret_weight, dd_weight);  // Or average both if you prefer
}
