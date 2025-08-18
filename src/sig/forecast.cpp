#include "ind/indicators.h"

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

  double ret_sum = 0.0, dd_sum = 0.0;
  double ret_imp = 0.0, dd_imp = 0.0;
  double n_candles_sum = 0.0, total_imp = 0.0;

  auto process = [&](auto& obj, auto& stats_map, double base_weight) {
    auto it = stats_map.find(obj.type);
    if (it == stats_map.end())
      return;

    auto& [_, avg_ret, avg_dd, _, imp, n_candles] = it->second;
    if (imp == 0.0)
      return;

    auto m_imp = imp * base_weight;
    auto s_imp = (1 - imp) * base_weight;

    if (obj.cls() == SignalClass::Entry) {
      ret_sum += avg_ret * m_imp;
      ret_imp += m_imp;

      dd_sum += avg_dd * s_imp;
      dd_imp += s_imp;
    } else {
      dd_sum += avg_dd * m_imp;
      dd_imp += m_imp;

      ret_sum += avg_ret * s_imp;
      ret_imp += s_imp;
    }

    n_candles_sum += 0.7 * n_candles * m_imp;
    total_imp += m_imp;
  };

  for (auto& r : sig.reasons)
    process(r, stats.reason, 1);
  for (auto& h : sig.hints)
    process(h, stats.hint, 0.75);

  if (ret_imp == 0.0 || dd_imp == 0.0 || total_imp == 0.0)
    return;

  exp_ret = ret_sum / ret_imp;
  exp_dd = dd_sum / dd_imp;

  n_min_candles =  //
      static_cast<int>(std::round(0.75 * n_candles_sum / total_imp));
  n_max_candles =  //
      static_cast<int>(std::round(1.5 * n_candles_sum / total_imp));

  conf = simple_conf(sig, stats);
}

// Forecast Metrics::gen_forecast(int idx) const {
//   return {get_signal(idx), stats};
// }
