#include "backtest.h"
#include "indicators.h"
#include "portfolio.h"
#include "signal.h"

Backtest::Backtest(const Metrics& m, size_t max_candles) : m{m} {
  auto& candles = m.ind_1h.candles;
  size_t n = candles.size();
  lookahead.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    double entry = candles[i].close;
    double best = -std::numeric_limits<double>::infinity();
    double worst = -std::numeric_limits<double>::infinity();
    for (size_t j = i; j < std::min(n, i + max_candles + 1); ++j) {
      double ret = (candles[j].close / entry) - 1.0;
      double dd = (entry / candles[j].low) - 1.0;
      // worst drop from entry to that bar’s low
      best = std::max(best, ret);
      worst = std::max(worst, dd);
    }
    lookahead[i] = {best * 100, worst * 100};
  }
}

SignalStats::SignalStats(size_t count,
                         double sum_ret,
                         double sum_dd,
                         size_t wins) {
  trigger_count = count;
  avg_return = count ? sum_ret / count : 0.0;
  avg_drawdown = count ? sum_dd / count : 0.0;
  win_rate = count ? double(wins) / count : 0.0;

  constexpr double EPS = 1e-6;
  constexpr double kappa = 0.2;

  double dd = std::max(avg_drawdown, EPS);
  double raw = win_rate * (avg_return / dd) * std::sqrt(double(trigger_count));
  importance = 1.0 - std::exp(-kappa * raw);
}

// unified handle for signal_f or hint_f
template <typename T, typename Func>
std::pair<T, SignalStats> Backtest::get_stats(Func signal_fn) const {
  auto& candles = m.ind_1h.candles;

  size_t count = 0;
  double sum_ret = 0.0;
  double sum_dd = 0.0;
  size_t wins = 0;

  T r0 = {};
  for (int i = 0; i < (int)candles.size(); ++i) {
    auto r = signal_fn(m, i);
    if (!r.exists() || r.source == Source::Stop)
      continue;
    r0 = r.type;

    ++count;
    double future_ret = lookahead[i].max_return;
    double future_dd = lookahead[i].max_drawdown;
    sum_ret += future_ret;
    sum_dd += future_dd;
    if (future_ret > future_dd)
      ++wins;
  }

  return {r0, {count, sum_ret, sum_dd, wins}};
}

template std::pair<ReasonType, SignalStats>  //
    Backtest::get_stats<ReasonType, signal_f>(signal_f) const;
template std::pair<HintType, SignalStats>  //
    Backtest::get_stats<HintType, hint_f>(hint_f) const;
