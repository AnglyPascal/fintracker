#include "ind/ticker.h"
#include "util/config.h"

#include <spdlog/spdlog.h>
#include <iostream>

inline auto& sig_config = config.sig_config;

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

  std::cout << std::format("{}:  {:.2f}, {:.2f}, {:.2f}\n",  //
                           symbol, sig.forecast.expected_return,
                           sig.forecast.expected_drawdown,
                           sig.forecast.confidence);

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

void Ticker::calculate_signal() {
  stop_loss = StopLoss(metrics);
  signal = gen_signal(-1);
  position_sizing = PositionSizing(metrics, signal, stop_loss);

  spdlog::trace("[stop] {}: {} price={:.2f} atr_stop={:.2f} final={:.2f}",
                symbol, stop_loss.is_trailing, metrics.last_price(),
                stop_loss.atr_stop, stop_loss.final_stop);
  spdlog::info("[sizing] {}: {} {}", symbol, position_sizing.recommended_shares,
               position_sizing.recommended_capital);
}
