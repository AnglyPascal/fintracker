#include "ind/indicators.h"
#include "sig/signals.h"

std::vector<Filter> evaluate_timeframe_alignment(const Metrics& m);
std::vector<Filter> evaluate_1h_trends(const Metrics& m);
std::vector<Filter> evaluate_4h_trends(const Metrics& m);
std::vector<Filter> evaluate_1d_trends(const Metrics& m);

Filters evaluate_filters(const Metrics& m) {
  Filters filters;
  filters.try_emplace(0, evaluate_timeframe_alignment(m));
  filters.try_emplace(H_1.count(), evaluate_1h_trends(m));
  filters.try_emplace(H_4.count(), evaluate_4h_trends(m));
  filters.try_emplace(D_1.count(), evaluate_1d_trends(m));
  return filters;
}
