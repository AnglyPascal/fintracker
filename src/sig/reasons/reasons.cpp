#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

Reason ema_crossover_entry(const IndicatorsTrends& ind, int idx);
Reason rsi_cross_50_entry(const IndicatorsTrends& m, int idx);
Reason pullback_bounce_entry(const IndicatorsTrends& ind, int idx);
Reason macd_histogram_cross_entry(const IndicatorsTrends& ind, int idx);
Reason broke_resistance_entry(const IndicatorsTrends& ind, int idx);

Reason ema_crossdown_exit(const IndicatorsTrends& ind, int idx);
Reason macd_bearish_cross_exit(const IndicatorsTrends& ind, int idx);
Reason broke_support_exit(const IndicatorsTrends& ind, int idx);

inline constexpr signal_f reason_funcs[] = {
    // Entry
    ema_crossover_entry,
    rsi_cross_50_entry,
    pullback_bounce_entry,
    macd_histogram_cross_entry,
    broke_resistance_entry,
    // Exit
    ema_crossdown_exit,
    macd_bearish_cross_exit,
    broke_support_exit,
};

std::vector<Reason> reasons(const IndicatorsTrends& ind, int idx) {
  std::vector<Reason> res;
  for (auto f : reason_funcs) {
    auto reason = f(ind, idx);
    if (reason.type != ReasonType::None) {
      res.push_back(reason);
    }
  }
  return res;
}

std::map<ReasonType, SignalStats> Stats::get_reason_stats(const Backtest& bt) {
  std::map<ReasonType, SignalStats> reason;
  for (auto& f : reason_funcs) {
    auto [r, s] = bt.get_stats<ReasonType>(f);
    reason.try_emplace(r, s);
  }
  return reason;
}
