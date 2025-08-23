#include "ind/indicators.h"
#include "util/config.h"

#include <spdlog/spdlog.h>
#include <cmath>

std::vector<Reason> reasons(const IndicatorsTrends& ind, int idx);
std::vector<Hint> hints(const IndicatorsTrends& ind, int idx);

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
  if (entry_w >= sig_config.watchlist_threshold)
    return Rating::Watchlist;

  if (exit_w >= sig_config.watchlist_threshold)
    return Rating::Caution;

  // // 6. Fallbacks
  // bool strong_entry_hint =
  //     std::any_of(hints.begin(), hints.end(), [](const auto& h) {
  //       return h.cls() == SignalClass::Entry &&
  //              h.severity() >= Severity::High && h.source() != Source::SR;
  //     });
  // bool strong_exit_hint =
  //     std::any_of(hints.begin(), hints.end(), [](const auto& h) {
  //       return h.cls() == SignalClass::Exit && h.severity() >= Severity::High
  //       &&
  //              h.source() != Source::SR;
  //     });

  // if (strong_entry_hint && strong_exit_hint)
  //   return Rating::Mixed;
  // if (strong_entry_hint)
  //   return Rating::Watchlist;
  // if (strong_exit_hint)
  //   return Rating::Caution;

  return Rating::None;
}

inline Score gen_score(double entry_w, double exit_w) {
  auto weight = sig_config.score_entry_weight;
  auto net = entry_w * weight - exit_w * (1 - weight);
  double net_score = std::tanh(net * sig_config.score_squash_factor);
  return {entry_w, exit_w, net_score};
}

Signal::Signal(const Indicators& ind, int idx) {
  tp = ind.time(idx);

  double entry_w = 0.0, exit_w = 0.0;
  auto add_w = [&entry_w, &exit_w](auto r, auto w, double gw = 1) {
    if (w <= 0.02 || w >= 0.98)  // ignore extremes
      return;
    auto r_w = r.severity_w() * r.score * gw;
    if (r.cls() == SignalClass::Entry)
      entry_w += w * r_w;
    else if (r.cls() == SignalClass::Exit)
      exit_w += (1 - w) * r_w;
  };

  auto& stats = ind.stats;

  // Hard signals
  for (auto r : ::reasons(ind, idx)) {
    if (r.type == ReasonType::None || r.cls() == SignalClass::None)
      continue;
    reasons.emplace_back(r);

    auto imp = 0.0;
    if (auto it = stats.reason.find(r.type); it != stats.reason.end())
      imp = it->second.imp;
    if (r.source() == Source::Stop)
      imp = sig_config.stop_reason_importance;

    add_w(r, imp);
  }

  // Hints
  for (auto h : ::hints(ind, idx)) {
    if (h.type == HintType::None || h.cls() == SignalClass::None)
      continue;

    hints.emplace_back(h);

    auto imp = 0.0;
    if (auto it = stats.hint.find(h.type); it != stats.hint.end())
      imp = it->second.imp;
    if (h.source() == Source::Stop)
      imp = sig_config.stop_hint_importance;

    add_w(h, imp, sig_config.score_hint_weight);
  }

  auto sort = [](auto& v) {
    std::sort(v.begin(), v.end(), [](auto& lhs, auto& rhs) {
      return lhs.severity() < rhs.severity();
    });
  };

  sort(reasons);
  sort(hints);

  type = gen_rating(entry_w, exit_w, reasons, hints);
  score = gen_score(entry_w, exit_w);

  forecast = Forecast{ind.interval, *this, ind.stats};
}
