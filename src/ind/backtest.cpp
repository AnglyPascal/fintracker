#include "ind/backtest.h"
#include "ind/indicators.h"
#include "util/config.h"

#include <cmath>

Backtest::Backtest(const IndicatorsTrends& _ind, size_t max_candles)
    : ind{_ind} {
  size_t n = ind.size();
  lookahead.reserve(n);

  double profit_target = config.sizing_config.profit_pct * 100;
  double stop_loss = config.sizing_config.stop_pct * 100;

  for (size_t i = 0; i < n; ++i) {
    double entry = ind.price(i);
    double best = 0.0;
    double worst = 0.0;

    // Then in the lookahead loop:
    bool profit_hit = false, stop_hit = false;
    size_t exit_candles = max_candles;
    double final_pnl = 0.0;

    auto end_idx = std::min(n - 1, i + max_candles);
    for (size_t j = i + 1; j <= end_idx; ++j) {
      double ret = (ind.price(j) - entry) / entry;

      if (ret > 0 && ret > best)
        best = ret;
      else if (ret < 0 && -ret > worst)
        worst = -ret;

      if (ret >= profit_target && !profit_hit) {
        profit_hit = true;
        exit_candles = j - i;
        final_pnl = ret * 100;
        break;
      }

      if (-ret >= stop_loss && !stop_hit) {
        stop_hit = true;
        exit_candles = j - i;
        final_pnl = ret * 100;  // Will be negative
        break;
      }
    }

    if (final_pnl == 0.0) {
      double ret = (ind.price(end_idx) - entry) / entry;
      final_pnl = ret * 100;
    }

    lookahead[i] = {best * 100, worst * 100, final_pnl, exit_candles,
                    profit_hit && !stop_hit};
  }
}

inline constexpr bool ignore_backtest(Source src) {
  return src == Source::Stop || src == Source::Trend || src == Source::SR;
}

template <typename T, typename Func>
std::pair<T, SignalStats> Backtest::get_stats(Func fn) const {
  size_t sample_size = 0;
  double sum_pnl = 0.0;
  double sum_squared_pnl = 0.0;
  double gross_profit = 0.0;
  double gross_loss = 0.0;
  size_t winning_trades = 0;
  size_t sum_winning_holding_period = 0;

  T r0 = {};
  bool is_entry_signal = true;

  auto lookback = config.ind_config.backtest_lookback(ind.interval);
  auto start = lookback > ind.size() ? 0 : ind.size() - lookback;

  for (size_t i = start; i < ind.size(); ++i) {
    auto r = fn(ind, i);
    if (!r.exists() || ignore_backtest(r.source()))
      continue;

    r0 = r.type;
    is_entry_signal = (r.cls() == SignalClass::Entry);

    auto [max_ret, max_dd, realized_pnl, exit_candles, hit_profit] =
        lookahead[i];

    sum_pnl += realized_pnl;
    sum_squared_pnl += realized_pnl * realized_pnl;

    if (realized_pnl > 0) {
      gross_profit += realized_pnl;
      sum_winning_holding_period += exit_candles;
      winning_trades++;
    } else {
      gross_loss += std::abs(realized_pnl);
    }
    sample_size++;
  }

  return {
      r0,
      {
          sample_size, winning_trades, ind.size() - start, sum_pnl,
          sum_squared_pnl, gross_profit, gross_loss, is_entry_signal,
          sum_winning_holding_period  //
      }  //
  };
}

SignalStats::SignalStats(size_t count,
                         size_t winning_trades,
                         size_t total_period_length,
                         double sum_pnl,
                         double sum_squared_pnl,
                         double total_profit,
                         double total_loss,
                         [[maybe_unused]] bool is_entry_signal,
                         size_t sum_winning_holding_period) noexcept
    : sample_size(count) {
  trig_rate = static_cast<double>(sample_size) / total_period_length;
  win_rate =
      sample_size > 0 ? static_cast<double>(winning_trades) / sample_size : 0.0;

  if (sample_size == 0)
    return;

  avg_pnl = sum_pnl / sample_size;
  if (winning_trades > 0)
    avg_profit = total_profit / winning_trades;
  if (winning_trades != sample_size)
    avg_loss = std::abs(total_loss) / (sample_size - winning_trades);

  avg_winning_holding_period =
      std::round((double)sum_winning_holding_period / sample_size);

  double pnl_variance = (sum_squared_pnl / sample_size) - (avg_pnl * avg_pnl);
  pnl_volatility = std::sqrt(std::max(pnl_variance, 0.0));

  imp = calculate_importance();
}

double SignalStats::calculate_importance() const {
  if (sample_size < 3)
    return 0.0;

  // 1. Profitability component (50% weight)
  //
  double profitability_score = 0.0;
  auto profit_factor = avg_loss > 0.001 ? avg_profit / avg_loss : 0.0;
  if (profit_factor > 1.0) {
    // Sigmoid curve: profit_factor of 1.5 gives 0.5, 2.0 gives ~0.81
    profitability_score = 1.0 / (1.0 + std::exp(-3 * (profit_factor - 1.5)));
  }

  // 2. Consistency component (30% weight)
  // Lower P&L volatility is better (more consistent)
  //
  double consistency_score = 0.0;
  if (avg_pnl > 0 && pnl_volatility > 0) {
    double consistency_ratio = avg_pnl / pnl_volatility;
    consistency_score = std::tanh(consistency_ratio / 2.0);
  }

  // 3. Frequency component (20% weight)
  // Sweet spot around 5-15% trigger rate
  //
  double frequency_score = 0.0;
  if (trig_rate >= 0.05 && trig_rate <= 0.15)
    frequency_score = 1.0;
  else if (trig_rate < 0.05)  // Linear penalty for rare signals
    frequency_score = trig_rate / 0.05;
  else  // Exponential penalty for frequent
    frequency_score = std::exp(-(trig_rate - 0.15) * 10.0);

  double base_score = profitability_score * 0.5  //
                      + consistency_score * 0.3  //
                      + frequency_score * 0.2;

  // Sample size penalty for small samples
  double sample_penalty = 1.0;
  if (trig_rate < 0.01)  // Very rare signals get penalized heavily
    sample_penalty = trig_rate / 0.01;
  else if (trig_rate < 0.03)  // Somewhat rare signals get moderate penalty
    sample_penalty = 0.5 + 0.5 * (trig_rate - 0.01) / 0.02;

  return base_score * sample_penalty;
}

template std::pair<ReasonType, SignalStats>  //
    Backtest::get_stats<ReasonType, signal_f>(signal_f) const;
template std::pair<HintType, SignalStats>  //
    Backtest::get_stats<HintType, hint_f>(hint_f) const;
