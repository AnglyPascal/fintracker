#include "ind/indicators.h"
#include "util/config.h"

#include <cmath>

inline double simple_conf(const Signal& sig, const Stats& stats) {
  double total_importance = 0.0;
  double total_win_rate = 0.0;
  size_t total_weight = 0;

  auto process_signals = [&](auto& vec, auto& map) {
    for (auto& r : vec) {
      auto it = map.find(r.type);
      if (it == map.end())
        continue;

      auto& stats = it->second;

      auto w = r.severity_w();
      total_importance += stats.imp * w;
      total_win_rate += stats.win_rate * w;
      total_weight += w;
    }
  };

  process_signals(sig.reasons, stats.reason);
  process_signals(sig.hints, stats.hint);

  if (total_weight == 0)
    return 0.0;

  auto avg_win_rate = total_win_rate / total_weight;
  auto avg_importance = total_importance / total_weight;

  return avg_win_rate * 0.5 + avg_importance * 0.5;
}

Forecast::Forecast(const Metrics& m, int idx) {
  auto sig = m.get_signal(H_1, idx);
  auto& stats = m.get_stats(H_1);

  auto sig_4h = m.get_signal(H_4, idx);
  auto sig_1d = m.get_signal(D_1, idx);

  double pnl_sum = 0.0;
  double holding_period_sum = 0.0, total_imp = 0.0;

  auto process = [&](auto& obj, auto& stats_map, double base_weight) {
    auto it = stats_map.find(obj.type);
    if (it == stats_map.end())
      return;

    auto& [trig_rate, avg_pnl, win_rate, _, _, vol, imp, sample_sz,
           avg_holding] = it->second;
    if (imp == 0.0)
      return;

    auto m_imp = imp * base_weight;
    pnl_sum += avg_pnl * m_imp;
    holding_period_sum += avg_holding;
    total_imp += m_imp;
  };

  for (auto& r : sig.reasons)
    process(r, stats.reason, 1);
  for (auto& h : sig.hints)
    process(h, stats.hint, 0.75);

  if (total_imp == 0.0)
    return;

  exp_pnl = pnl_sum / total_imp;
  n_min_candles =  //
      static_cast<int>(std::round(0.75 * holding_period_sum / total_imp));
  n_max_candles =  //
      static_cast<int>(std::round(1.5 * holding_period_sum / total_imp));

  conf = simple_conf(sig, stats);
  auto decay = config.ind_config.backtest_memory_decay;
  auto weight = 1;
  auto& mem = m.get_memories(H_1);
  for (auto it = mem.past.rbegin(); it != mem.past.rend(); it++) {
    conf += simple_conf(*it, stats) * weight;
    weight *= decay;
  }
}
