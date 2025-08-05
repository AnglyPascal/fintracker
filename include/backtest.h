#pragma once

#include "signals.h"

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

  SignalStats(size_t count,
              double sum_ret,
              double sum_dd,
              size_t wins,
              bool entry) noexcept;
};

struct Indicators;

struct Backtest {
  const Indicators& ind;
  std::vector<LookaheadStats> lookahead;  // same length as candles

  Backtest(const Indicators& ind, size_t max_candles = 8 * 10);

  template <typename T, typename Func>
  std::pair<T, SignalStats> get_stats(Func signal_fn) const;
};

