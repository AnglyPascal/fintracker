#pragma once

#include "signal_types.h"
#include "util/times.h"

#include <cmath>
#include <vector>

struct IndicatorsTrends;
struct Indicators;
struct Metrics;

using signal_f = Reason (*)(const IndicatorsTrends&, int);
using hint_f = Hint (*)(const IndicatorsTrends&, int);
using conf_f = Confirmation (*)(const Metrics&);

struct Score {
  double entry = 0.0;
  double exit = 0.0;
  double final = 0.0;

  operator double() const { return final; }
  static int pretty(double v) { return static_cast<int>(std::round(v * 10)); }
};

struct Stats;
struct Signal;

struct Forecast {
  double exp_pnl = 0.0;
  size_t holding_days = 0;
  double conf = 0.0;

  Forecast() = default;
  Forecast(minutes timeframe, const Signal& sig, const Stats& stats);
};

struct Signal {
  Rating type = Rating::None;
  Score score;
  LocalTimePoint tp;

  std::vector<Reason> reasons;
  std::vector<Hint> hints;
  Forecast forecast;

  bool has_rating() const { return type != Rating::None; }
  bool has_reasons() const { return !reasons.empty(); }
  bool has_hints() const { return !hints.empty(); }

  bool is_interesting() const;

  Signal() = default;
  Signal(const Indicators& ind, int idx = -1);
};

struct StopLoss;
struct Event;

struct CombinedSignal {
  Rating type = Rating::None;
  Score score;
  std::string rationale;

  StopHit stop_hit;
  Filters filters;
  Forecast forecast;

  CombinedSignal() = default;
  CombinedSignal(const Metrics& m,
                 const StopLoss& sl,
                 const Event& ev,
                 int idx = -1);
};
