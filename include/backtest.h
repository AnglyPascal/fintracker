#pragma once

#include <cstdlib>
#include <utility>
#include <vector>

struct LookaheadStats {
  double max_return;
  int max_return_n_candles;
  double max_drawdown;
  int max_drawdown_n_candles;
};  // FIXME: use the n_candles

struct SignalStats {
  size_t trigger_count;
  double avg_return;
  double avg_drawdown;
  double win_rate;  // % of signals with positive return
  double importance;
  size_t avg_ret_n_candles;

  SignalStats(size_t count,
              double sum_ret,
              double sum_dd,
              size_t wins,
              bool entry,
              size_t n_candles) noexcept;
};

struct Indicators;

struct Backtest {
  const Indicators& ind;
  std::vector<LookaheadStats> lookahead;  // same length as candles

  Backtest(const Indicators& ind, size_t max_candles = 8 * 10);

  template <typename T, typename Func>
  std::pair<T, SignalStats> get_stats(Func signal_fn) const;
};
