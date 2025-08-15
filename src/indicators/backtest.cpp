#include "ind/backtest.h"
#include "ind/indicators.h"
#include "util/config.h"

#include <cmath>

Backtest::Backtest(const Indicators& _ind, size_t max_candles) : ind{_ind} {
  size_t n = ind.size();
  lookahead.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    double entry = ind.price(i);
    double best = 0.0;
    int best_n_candles = 0;
    double worst = 0.0;
    int worst_n_candles = 0;

    for (size_t j = i + 1; j <= std::min(n - 1, i + max_candles); ++j) {
      double ret = (ind.price(j) - entry) / entry;
      if (ret > 0 && ret > best) {
        best = ret;
        best_n_candles = j - i;
      } else if (ret < 0 && -ret > worst) {
        worst = -ret;
        worst_n_candles = j - i;
      }
    }

    lookahead[i] = {best * 100, best_n_candles, worst * 100, worst_n_candles};
  }
}

SignalStats::SignalStats(size_t count,
                         double sum_ret,
                         double sum_dd,
                         size_t wins,
                         bool entry,
                         size_t n_candles) noexcept {
  trigger_count = count;
  avg_return = count ? sum_ret / count : 0.0;
  avg_drawdown = count ? sum_dd / count : 0.0;
  win_rate = count ? double(wins) / count : 0.0;

  auto eps = config.ind_config.eps;
  auto kappa = config.ind_config.stats_importance_kappa;

  double raw = 0.0;
  if (entry) {
    double dd = std::max(avg_drawdown, eps);
    raw = win_rate * (avg_return / dd) * std::sqrt(double(trigger_count));
  } else {
    double ret = std::max(avg_return, eps);
    raw = (1 - win_rate) * (avg_drawdown / ret) *
          std::sqrt(double(trigger_count));
  }

  /** Smaller kappa means more restrictive importance,
   *  where larger kappa gives a looser, more uniform importance
   */
  importance = 1.0 - std::exp(-kappa * raw);

  avg_ret_n_candles = n_candles;
}

inline constexpr bool ignore_backtest(Source src) {
  return src == Source::Stop || src == Source::Trend || src == Source::SR;
}

template <typename T, typename Func>
std::pair<T, SignalStats> Backtest::get_stats(Func fn) const {
  size_t count = 0;
  double sum_ret = 0.0;
  double sum_dd = 0.0;
  double sum_ret_n_candles = 0.0;
  size_t wins = 0;

  T r0 = {};
  bool entry = true;
  for (size_t i = 0; i < ind.size(); ++i) {
    auto r = fn(ind, i);
    if (!r.exists() || ignore_backtest(r.source()))
      continue;

    r0 = r.type;
    entry = r.cls() == SignalClass::Entry;

    auto [ret, ret_n_candles, dd, _] = lookahead[i];
    sum_ret += ret;
    sum_ret_n_candles += ret_n_candles;
    sum_dd += dd;
    wins += ret > dd;

    count++;
  }

  auto avg_ret_n_candles = (size_t)std::round(sum_ret_n_candles / count);
  return {r0, {count, sum_ret, sum_dd, wins, entry, avg_ret_n_candles}};
}

template std::pair<ReasonType, SignalStats>  //
    Backtest::get_stats<ReasonType, signal_f>(signal_f) const;
template std::pair<HintType, SignalStats>  //
    Backtest::get_stats<HintType, hint_f>(hint_f) const;
