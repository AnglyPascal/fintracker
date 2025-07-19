#pragma once

#include "signals.h"

#include <cmath>
#include <unordered_map>

struct LookaheadStats {
  double max_return;
  double max_drawdown;
};

struct SignalStats {
  size_t trigger_count;
  double avg_return;
  double avg_drawdown;
  double win_rate;  // % of signals with positive return
  double importance;

  SignalStats(size_t count, double sum_ret, double sum_dd, size_t wins);
};

struct Metrics;

struct Backtest {
  const Metrics& m;
  std::vector<LookaheadStats> lookahead;  // same length as candles

  Backtest(const Metrics& m, size_t max_candles = 8 * 10);

  template <typename T, typename Func>
  std::pair<T, SignalStats> get_stats(Func signal_fn) const;
};

