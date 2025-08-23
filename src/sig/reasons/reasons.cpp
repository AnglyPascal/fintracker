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

// Check for conflicting hints and apply penalties
inline void check_conflicts(std::vector<Reason>& reasons) {
  // Find conflicting pairs
  bool has_ema_bull = false, has_ema_bear = false;
  bool has_macd_bull = false, has_macd_bear = false;
  // bool has_macd_rise = false, has_macd_peak = false;

  for (auto& r : reasons) {
    if (r.type == ReasonType::EmaCrossover)
      has_ema_bull = true;
    if (r.type == ReasonType::EmaCrossdown)
      has_ema_bear = true;
    if (r.type == ReasonType::MacdBullishCross)
      has_macd_bull = true;
    if (r.type == ReasonType::MacdBearishCross)
      has_macd_bear = true;
  }

  double penalty = 0.7;
  for (auto& r : reasons) {
    if (r.type == ReasonType::EmaCrossover && has_ema_bear)
      r.score *= penalty;
    if (r.type == ReasonType::EmaCrossdown && has_ema_bull)
      r.score *= penalty;
    if (r.type == ReasonType::MacdBullishCross && has_macd_bear)
      r.score *= penalty;
    if (r.type == ReasonType::MacdBearishCross && has_macd_bull)
      r.score *= penalty;
  }
}

std::vector<Reason> reasons(const IndicatorsTrends& ind, int idx) {
  std::vector<Reason> res;
  for (auto f : reason_funcs) {
    auto reason = f(ind, idx);
    if (reason.type != ReasonType::None) {
      res.push_back(reason);
    }
  }
  check_conflicts(res);
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
