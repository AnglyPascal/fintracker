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
  bool empty() const { return exp_pnl == 0.0; }
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

