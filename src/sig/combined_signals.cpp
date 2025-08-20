#include "ind/calendar.h"
#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

#include <spdlog/spdlog.h>

inline auto& sig_config = config.sig_config;

inline bool disqualify(auto& filters) {
  int strongBearish1D = 0;
  int weakBearish1D = 0;
  int bearish4H = 0;

  for (auto& f : filters.at(D_1.count())) {
    if (f.trend == Trend::Bearish) {
      if (f.conf == Confidence::High)
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
                                                auto& stop_hit,
                                                auto& filters,
                                                bool has_position) {
  auto& sig_1h = ind_1h.signal;
  auto& sig_4h = ind_4h.signal;
  auto& sig_1d = ind_1d.signal;

  Rating base_rating = sig_1h.type;
  int score_mod = 0;

  if (stop_hit.type == StopHitType::None && disqualify(filters))
    return {Rating::Skip, score_mod};

  if (base_rating == Rating::Caution && has_position)
    base_rating = Rating::HoldCautiously;

  if (base_rating == Rating::Exit && !has_position)
    base_rating = Rating::Caution;

  if (stop_hit.type == StopHitType::TimeExit ||
      stop_hit.type == StopHitType::StopLossHit)
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

inline Score weighted_score(Score score_1h,
                            Score score_4h,
                            Score score_1d,
                            double mod) {
  auto [entry, exit, past, final] = score_1h;

  if ((score_1h > 0 && score_4h > 0) || (score_1h > 0 && score_1d > 0))
    final *= sig_config.score_mod_4h_1d_agree;

  if (score_1h > 0 && score_4h > 0 && score_1d > 0)
    final *= sig_config.score_mod_4h_1d_align;

  if ((score_1h > 0 && score_4h < -0.5) || (score_1h > 0 && score_1d < -0.5))
    final *= sig_config.score_mod_4h_1d_conflict;

  final += mod;
  return {entry, exit, past, final};
}

StopHit stop_loss_hits(const Metrics& m, const StopLoss& stop_loss);
Filters evaluate_filters(const Metrics& m);
std::vector<Confirmation> confirmations(const Metrics& m);

CombinedSignal::CombinedSignal(const Metrics& m,
                               const StopLoss& sl,
                               const Event& ev,
                               [[maybe_unused]] int idx) {
  auto& ind_1h = m.ind_1h;
  auto& ind_4h = m.ind_4h;
  auto& ind_1d = m.ind_1d;

  stop_hit = stop_loss_hits(m, sl);
  forecast = Forecast{ind_1h.signal, ind_1h.stats};

  filters = evaluate_filters(m);
  auto [t, mod] = contextual_rating(ind_1h, ind_4h, ind_1d, stop_hit, filters,
                                    m.has_position());
  type = t;
  score = weighted_score(ind_1h.signal.score, ind_4h.signal.score,
                         ind_1d.signal.score, mod);

  if (type == Rating::Entry) {
    for (auto conf : confirmations(m))
      if (conf.str != "")
        confs.push_back(conf);

    // disqualify if earnings is near
    if (ev.is_earnings() && ev.days_until() >= 0 &&
        ev.days_until() < config.sizing_config.earnings_volatility_buffer)
      type = Rating::Watchlist;
  }
}
