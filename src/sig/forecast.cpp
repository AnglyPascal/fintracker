#include "ind/indicators.h"
#include "util/config.h"

#include <cmath>

Forecast::Forecast(const Signal& sig, const Stats& stats) {
  double pnl_sum = 0.0;
  double holding_sum = 0.0, total_imp = 0.0;

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
    holding_sum += avg_holding * m_imp;
    total_imp += m_imp;
  };

  for (auto& r : sig.reasons)
    process(r, stats.reason, 1);
  for (auto& h : sig.hints)
    process(h, stats.hint, 0.75);

  if (total_imp == 0.0)
    return;

  exp_pnl = pnl_sum / total_imp;
  holding_period = static_cast<size_t>(std::round(holding_sum / total_imp));
  conf = total_imp;
}
