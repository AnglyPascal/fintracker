#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"

inline auto& sig_config = config.sig_config;

std::vector<Filter> evaluate_timeframe_alignment(const Metrics& m);
std::vector<Filter> evaluate_4h_trend(const Metrics& m);
std::vector<Filter> evaluate_1d_trend(const Metrics& m);

Filters evaluate_filters(const Metrics& m) {
  Filters filters;
  filters.try_emplace(H_1.count(), evaluate_timeframe_alignment(m));
  filters.try_emplace(H_4.count(), evaluate_4h_trend(m));
  filters.try_emplace(D_1.count(), evaluate_1d_trend(m));
  return filters;
}
