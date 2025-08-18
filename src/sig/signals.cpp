#include "ind/indicators.h"
#include "util/config.h"

#include <spdlog/spdlog.h>
#include <cmath>

std::vector<Reason> reasons(const Indicators& ind, int idx);
std::vector<Hint> hints(const Indicators& ind, int idx);

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

inline auto& sig_config = config.sig_config;

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

Signal::Signal(const Indicators& ind, int idx) {
  tp = ind.time(idx);

  double entry_w = 0.0, exit_w = 0.0;
  auto add_w = [&](auto cls, auto w) {
    (cls == SignalClass::Entry) ? entry_w += w : exit_w += w;
  };

  auto& stats = ind.stats;

  // Hard signals
  for (auto r : ::reasons(ind, idx)) {
    if (r.type == ReasonType::None || r.cls() == SignalClass::None)
      continue;

    reasons.emplace_back(r);

    auto imp = 0.0;  // in [0, 1]
    if (auto it = stats.reason.find(r.type); it != stats.reason.end())
      imp = it->second.imp;
    if (r.source() == Source::Stop)
      imp = sig_config.stop_reason_importance;

    add_w(r.cls(), imp * r.severity_w());
  }

  // Hints
  for (auto h : ::hints(ind, idx)) {
    if (h.type == HintType::None || h.cls() == SignalClass::None)
      continue;

    hints.emplace_back(h);
    if (h.severity() < Severity::High)
      continue;

    auto imp = 0.0;
    if (auto it = stats.hint.find(h.type); it != stats.hint.end())
      imp = it->second.imp;
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

  sort(reasons);
  sort(hints);

  auto past_score = ind.memory.score();
  type = gen_rating(entry_w, exit_w, past_score, reasons, hints);
  score = gen_score(entry_w, exit_w, past_score);
}

