#pragma once

#include "signal_types.h"

#include "util/times.h"

#include <deque>
#include <vector>

struct Indicators;
struct Metrics;

using signal_f = Reason (*)(const Indicators&, int idx);
using hint_f = Hint (*)(const Indicators&, int idx);
using conf_f = Confirmation (*)(const Metrics&);

std::vector<Reason> reasons(const Indicators& ind, int idx);
std::vector<Hint> hints(const Indicators& ind, int idx);
std::vector<Confirmation> confirmations(const Metrics& m);
Filters evaluate_filters(const Metrics& m);

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
};

struct SignalMemory {
  std::deque<Signal> past;
  int memory_length = 0;

  SignalMemory(minutes interval) noexcept;

  void push_back(const Signal& sig) {
    past.push_back(sig);
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
  double expected_return = 0.0;
  double expected_drawdown = 0.0;
  int n_min_candles = 0;
  int n_max_candles = 0;
  double confidence = 0.0;

  Forecast() = default;
  Forecast(const Signal& sig, const Stats& stats);
};

struct CombinedSignal {
  Rating type = Rating::None;
  double score;

  StopHit stop_hit;
  Filters filters;
  std::vector<Confirmation> confirmations;

  Forecast forecast;
};
