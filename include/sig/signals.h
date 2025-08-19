#pragma once

#include "signal_types.h"
#include "util/times.h"

#include <deque>
#include <vector>

struct IndicatorsTrends;
struct Indicators;
struct Metrics;

using signal_f = Reason (*)(const IndicatorsTrends&, int);
using hint_f = Hint (*)(const IndicatorsTrends&, int);
using conf_f = Confirmation (*)(const Metrics&);

struct Signal {
  Rating type = Rating::None;
  double score;
  LocalTimePoint tp;

  std::vector<Reason> reasons;
  std::vector<Hint> hints;

  bool has_rating() const { return type != Rating::None; }
  bool has_reasons() const { return !reasons.empty(); }
  bool has_hints() const { return !hints.empty(); }

  bool is_interesting() const;

  Signal() = default;
  Signal(const Indicators& ind, int idx = -1);
};

struct SignalMemory {
  std::deque<Signal> past;
  int memory_length = 0;

  SignalMemory(minutes interval) noexcept;

  template <typename... Args>
  void emplace_back(Args&&... args) {
    past.emplace_back(std::forward<Args>(args)...);
    if (past.size() > (size_t)memory_length)
      past.pop_front();
  }

  void pop_back() {
    if (!past.empty())
      past.pop_back();
  }

  double score() const;
  int rating_score() const;
};

struct Stats;

struct Forecast {
  double exp_ret = 0.0;
  double exp_dd = 0.0;
  int n_min_candles = 0;
  int n_max_candles = 0;
  double conf = 0.0;

  Forecast() = default;
  Forecast(const Metrics& m, int idx);
};

struct StopLoss;
struct Event;

struct CombinedSignal {
  Rating type = Rating::None;
  double score;

  StopHit stop_hit;
  Filters filters;
  std::vector<Confirmation> confs;

  Forecast forecast;

  CombinedSignal() = default;
  CombinedSignal(const Metrics& m,
                 const StopLoss& sl,
                 const Event& ev,
                 int idx = -1);
};
