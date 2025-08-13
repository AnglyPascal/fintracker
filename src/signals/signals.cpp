#include "config.h"
#include "format.h"
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

auto& sig_config = config.sig_config;

inline Rating gen_rating(double entry_w,
                         double exit_w,
                         double /* past_score */,
                         auto& reasons,
                         auto& hints) {
  // 1. Strong Entry
  if (entry_w >= sig_config.entry_threshold &&
      exit_w <= sig_config.watchlist_threshold && !reasons.empty())
    return Rating::Entry;

  // 2. Strong Exit
  if (exit_w >= sig_config.exit_threshold &&
      entry_w <= sig_config.watchlist_threshold && !reasons.empty())
    return Rating::Exit;

  // 3. Mixed
  if (entry_w >= sig_config.mixed_min && exit_w >= sig_config.mixed_min)
    return Rating::Mixed;

  // 4. Moderate Exit or urgent exit hint
  bool has_urgent_exit_hint =
      std::any_of(hints.begin(), hints.end(), [](auto& h) {
        return h.cls() == SignalClass::Exit && h.severity() == Severity::Urgent;
      });

  if (has_urgent_exit_hint)
    return Rating::Caution;

  // 5. Entry bias
  if (entry_w >= sig_config.entry_min)
    return Rating::Watchlist;

  if (exit_w >= sig_config.exit_min)
    return Rating::Caution;

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

inline double gen_score(double entry_w, double exit_w, double past_score) {
  auto weight = sig_config.score_entry_weight;
  auto raw = [=]() {
    if (entry_w == 0)
      return -exit_w * 0.5;
    if (exit_w == 0)
      return entry_w * 0.5;
    return entry_w * weight - exit_w * (1 - weight);
  }();

  // larger gives more dichotomy between smaller raw and larger raw
  auto squash_factor = sig_config.score_squash_factor;
  auto curr_score = std::tanh(raw * squash_factor);  // squashes to [-1,1]

  auto alpha = sig_config.score_curr_alpha;
  return curr_score * alpha + past_score * (1 - alpha);
}

Signal Indicators::gen_signal(int idx) const {
  Signal s;
  s.tp = time(idx);

  double entry_w = 0.0, exit_w = 0.0;
  auto add_w = [&](auto cls, auto w) {
    (cls == SignalClass::Entry) ? entry_w += w : exit_w += w;
  };

  // Hard signals
  for (auto r : reasons(*this, idx)) {
    if (r.type == ReasonType::None || r.cls() == SignalClass::None)
      continue;

    s.reasons.emplace_back(r);

    auto imp = 0.0;  // in [0, 1]
    if (auto it = stats.reason.find(r.type); it != stats.reason.end())
      imp = it->second.importance;
    if (r.source() == Source::Stop)
      imp = sig_config.stop_reason_importance;

    add_w(r.cls(), imp * r.severity_w());
  }

  // Hints
  for (auto h : hints(*this, idx)) {
    if (h.type == HintType::None || h.cls() == SignalClass::None)
      continue;

    s.hints.emplace_back(h);
    if (h.severity() < Severity::High)
      continue;

    auto imp = 0.0;
    if (auto it = stats.hint.find(h.type); it != stats.hint.end())
      imp = it->second.importance;
    if (h.source() == Source::Stop)
      imp = sig_config.stop_hint_importance;

    auto w = sig_config.score_hint_weight * imp * h.severity_w();
    add_w(h.cls(), w);
  }

  auto sort = [](auto& v) {
    std::sort(v.begin(), v.end(), [](auto& lhs, auto& rhs) {
      return lhs.severity() < rhs.severity();
    });
  };

  sort(s.reasons);
  sort(s.hints);

  auto past_score = memory.score();
  s.type = gen_rating(entry_w, exit_w, past_score, s.reasons, s.hints);
  s.score = gen_score(entry_w, exit_w, past_score);

  return s;
}

inline bool disqualify(auto& filters) {
  int strongBearish1D = 0;
  int weakBearish1D = 0;
  int bearish4H = 0;

  for (auto& f : filters.at(D_1.count())) {
    if (f.trend == Trend::Bearish) {
      if (f.confidence == Confidence::High)
        ++strongBearish1D;
      else
        ++weakBearish1D;
    }
  }

  for (auto& f : filters.at(H_4.count())) {
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

inline std::pair<Rating, int> contextual_rating(auto& ind_1h,
                                                auto& ind_4h,
                                                auto& ind_1d,
                                                auto& sig,
                                                bool has_position) {
  auto& sig_1h = ind_1h.signal;
  auto& sig_4h = ind_4h.signal;
  auto& sig_1d = ind_1d.signal;

  Rating base_rating = sig_1h.type;
  int score_mod = 0;

  if (sig.stop_hit.type != StopHitType::None && disqualify(sig.filters))
    return {Rating::Skip, score_mod};

  if (base_rating == Rating::Caution && has_position)
    base_rating = Rating::HoldCautiously;

  if (base_rating == Rating::Exit && !has_position)
    base_rating = Rating::Caution;

  if (sig.stop_hit.type == StopHitType::TimeExit ||
      sig.stop_hit.type == StopHitType::StopLossHit)
    base_rating = Rating::Exit;
  else if (sig.stop_hit.type == StopHitType::StopInATR)
    base_rating = Rating::HoldCautiously;
  else if (sig.stop_hit.type == StopHitType::StopProximity) {
    if (base_rating == Rating::Watchlist)
      base_rating = Rating::Mixed;
    else if (base_rating == Rating::Mixed || base_rating == Rating::Caution)
      base_rating = Rating::HoldCautiously;
  }

  // Upgrade to Entry if higher timeframes show strong entry signals
  if (base_rating == Rating::Watchlist) {
    if (sig_4h.type == Rating::Watchlist &&
        sig_4h.score > sig_config.entry_4h_score_confirmation)
      return {Rating::Entry, score_mod};
    if (sig_1d.type == Rating::Watchlist &&
        sig_1d.score > sig_config.entry_1d_score_confirmation)
      return {Rating::Entry, score_mod};
  }

  // If strong history: upgrade
  if (ind_1h.memory.rating_score() >= 1.5 ||
      ind_4h.memory.rating_score() >= 1.5 ||
      ind_1d.memory.rating_score() >= 1.5) {
    if (base_rating == Rating::None)
      base_rating = Rating::OldWatchlist;
    if (base_rating == Rating::Watchlist)
      score_mod += 0.1;
  }

  // Higher timeframe exits should be respected
  if (sig_4h.type == Rating::Exit || sig_1d.type == Rating::Exit ||
      (sig_4h.type == Rating::HoldCautiously &&
       sig_4h.score < sig_config.exit_4h_score_confirmation) ||
      (sig_1d.type == Rating::HoldCautiously &&
       sig_1d.score < sig_config.exit_1d_score_confirmation)) {
    if (base_rating == Rating::Entry || base_rating == Rating::Watchlist)
      return {Rating::Mixed, score_mod};  // Conflict requires caution
    if (base_rating == Rating::HoldCautiously)
      return {Rating::Exit, score_mod};
  }

  return {base_rating, score_mod};
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
  auto& ind_1h = metrics.ind_1h;
  auto& ind_4h = metrics.ind_4h;
  auto& ind_1d = metrics.ind_1d;

  CombinedSignal sig;

  sig.stop_hit = stop_loss_hits(metrics, stop_loss);
  sig.forecast = ind_1h.gen_forecast(idx);

  std::cout << symbol << ": " << sig.forecast.expected_return << " "
            << sig.forecast.confidence << std::endl;

  sig.filters = evaluate_filters(metrics);
  auto [type, mod] =
      contextual_rating(ind_1h, ind_4h, ind_1d, sig, metrics.has_position());
  sig.type = type;
  sig.score = weighted_score(ind_1h.signal.score, ind_4h.signal.score,
                             ind_1d.signal.score) +
              mod;

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

SignalMemory::SignalMemory(minutes interval) noexcept {
  if (interval == H_1)
    memory_length = 16;
  else if (interval == H_4)
    memory_length = 10;
  else
    memory_length = 6;
}

double SignalMemory::score() const {
  double scr = 0.0;
  double weight = 0.0;

  double lambda = sig_config.score_memory_lambda;
  for (auto& sig : past) {
    scr = scr * lambda + sig.score;
    weight = weight * lambda + lambda;
  }

  return weight > 0.0 ? scr / weight : 0.0;
}

int SignalMemory::rating_score() const {
  int n_entry = 0;
  int n_strong_watchlist = 0;
  int n_exit = 0;
  int n_strong_caution = 0;

  auto strength_threshold = sig_config.mem_strength_threshold;
  for (auto& sig : past) {
    switch (sig.type) {
      case Rating::Entry:
        n_entry += 1 + (sig.score >= strength_threshold);
        break;
      case Rating::Watchlist:
        n_strong_watchlist += sig.score >= strength_threshold;
        break;
      case Rating::Exit:
        n_exit += 1 + (sig.score <= -strength_threshold);
        break;
      case Rating::Caution:
      case Rating::HoldCautiously:
        n_strong_caution += sig.score <= -strength_threshold;
        break;
      default:
        break;
    }
  }

  n_entry += (n_strong_watchlist + 1) / 2;
  n_exit += (n_strong_caution + 1) / 2;

  return n_entry - n_exit;
}
