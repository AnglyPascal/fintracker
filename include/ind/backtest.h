#pragma once

#include <cstdlib>
#include <utility>
#include <vector>

struct LookaheadStats {
  double max_return;
  int max_return_n_candles;  // TODO: use it
  double max_drawdown;
  int max_drawdown_n_candles;  // TODO: use it
};

struct SignalStats {
  size_t n_triggers;
  double avg_ret;
  double avg_dd;
  double win_rate;  // % of signals with positive return
  double imp;
  size_t avg_ret_n_candles;

  SignalStats(size_t count,
              double sum_ret,
              double sum_dd,
              size_t wins,
              bool entry,
              size_t n_candles) noexcept;
};

struct IndicatorsTrends;

struct Backtest {
  const IndicatorsTrends& ind;
  std::vector<LookaheadStats> lookahead;

  Backtest(const IndicatorsTrends& ind, size_t max_candles = 8 * 10);

  template <typename T, typename Func>
  std::pair<T, SignalStats> get_stats(Func signal_fn) const;
};
