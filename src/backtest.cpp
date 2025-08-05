#include "backtest.h"
#include "indicators.h"

#include <cmath>

Backtest::Backtest(const Indicators& _ind, size_t max_candles) : ind{_ind} {
  auto& candles = ind.candles;
  size_t n = candles.size();
  lookahead.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    double entry = candles[i].close;
    double best = 0.0;
    double worst = 0.0;

    for (size_t j = i + 1; j <= std::min(n - 1, i + max_candles); ++j) {
      double ret = (candles[j].close - entry) / entry;

      if (ret > 0)
        best = std::max(best, ret);
      else
        worst = std::max(worst, -ret);
    }

    lookahead[i] = {best * 100, worst * 100};  // In percentage
  }
}

SignalStats::SignalStats(size_t count,
                         double sum_ret,
                         double sum_dd,
                         size_t wins,
                         bool entry) noexcept {
  trigger_count = count;
  avg_return = count ? sum_ret / count : 0.0;
  avg_drawdown = count ? sum_dd / count : 0.0;
  win_rate = count ? double(wins) / count : 0.0;

  constexpr double EPS = 1e-6;
  constexpr double kappa = 0.2;

  double raw = 0.0;
  if (entry) {
    double dd = std::max(avg_drawdown, EPS);
    raw = win_rate * (avg_return / dd) * std::sqrt(double(trigger_count));
  } else {
    double ret = std::max(avg_return, EPS);
    raw = (1 - win_rate) * (avg_drawdown / ret) *
          std::sqrt(double(trigger_count));
  }
  importance = 1.0 - std::exp(-kappa * raw);
}

// unified handle for signal_f or hint_f
template <typename T, typename Func>
std::pair<T, SignalStats> Backtest::get_stats(Func signal_fn) const {
  auto& candles = ind.candles;

  size_t count = 0;
  double sum_ret = 0.0;
  double sum_dd = 0.0;
  size_t wins = 0;

  T r0 = {};
  bool entry = true;
  for (size_t i = 0; i < candles.size(); ++i) {
    auto r = signal_fn(ind, i);
    if (!r.exists() || r.source() == Source::Stop ||
        r.source() == Source::Trend)
      continue;

    r0 = r.type;
    entry = r.cls() == SignalClass::Entry;

    count++;
    double future_ret = lookahead[i].max_return;
    double future_dd = lookahead[i].max_drawdown;
    sum_ret += future_ret;
    sum_dd += future_dd;
    if (future_ret > future_dd)
      wins++;
  }

  return {r0, {count, sum_ret, sum_dd, wins, entry}};
}

template std::pair<ReasonType, SignalStats>  //
    Backtest::get_stats<ReasonType, signal_f>(signal_f) const;
template std::pair<HintType, SignalStats>  //
    Backtest::get_stats<HintType, hint_f>(hint_f) const;
