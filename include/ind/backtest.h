#pragma once

#include <cstdlib>
#include <utility>
#include <vector>

struct LookaheadStats {
  double max_return;    // always positive
  double max_drawdown;  // same
  double realized_pnl;  // positive or negative
  size_t exit_n_candles;
  bool hit_profit_target;
};

/**
 * @brief Performance statistics for a specific trading signal type.
 *
 * Aggregated metrics calculated from backtesting historical signal occurrences,
 * providing comprehensive signal quality assessment with importance scoring.
 */
struct SignalStats {
  double trig_rate;       // Signal frequency: triggers / total_candles
  double avg_pnl;         // Average P&L per trade (%)
  double win_rate;        // Winning trades / total trades (0.0-1.0)
  double avg_profit;      // Average profit from winning trades only (%)
  double avg_loss;        // Average loss from losing trades only (%) [positive]
  double pnl_volatility;  // Standard deviation of P&L (consistency measure)
  double imp;             // Composite importance score (0.0-1.0)
  size_t sample_size;     // Number of trades in backtest period
  size_t avg_winning_holding_period;  // Average candles held for winning trades
                                      // only

 private:
  friend class Backtest;

  SignalStats(size_t count,
              size_t winning_trades,
              size_t total_period_length,
              double sum_pnl,
              double sum_squared_pnl,
              double total_profit,
              double total_loss,
              bool is_entry_signal,
              size_t sum_winning_holding_period) noexcept;

  double calculate_importance() const;
};

struct IndicatorsTrends;

/**
 * @brief Backtesting engine for evaluating trading signal performance over
 * historical data.
 *
 * Calculates forward-looking performance metrics for each candle in the dataset
 * by simulating trades with realistic exit conditions: profit target hit, stop
 * loss hit, or timeout after max_candles. Uses actual historical price data to
 * determine exact exit points and P&L.
 *
 * Exit Priority: 1) Profit target -> 2) Stop loss -> 3) Timeout at max_candles
 * Entry: Close price of signal candle
 * Lookback: Recent period defined by config.backtest_lookback() for statistical
 * relevance
 */
class Backtest {
  const IndicatorsTrends& ind;
  std::vector<LookaheadStats> lookahead;

 public:
  Backtest(const IndicatorsTrends& ind, size_t max_candles = 8 * 10);

  template <typename T, typename Func>
  std::pair<T, SignalStats> get_stats(Func signal_fn) const;
};

