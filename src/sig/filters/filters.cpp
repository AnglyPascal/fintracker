#include "sig/filters.h"
#include "ind/indicators.h"

std::vector<Filter> evaluate_timeframe_alignment(const Metrics& m);
std::vector<Filter> evaluate_1h_trends(const Metrics& m);
std::vector<Filter> evaluate_4h_trends(const Metrics& m);
std::vector<Filter> evaluate_1d_trends(const Metrics& m);

Filters::Filters(const Metrics& m) {
  try_emplace(0, evaluate_timeframe_alignment(m));
  try_emplace(H_1.count(), evaluate_1h_trends(m));
  try_emplace(H_4.count(), evaluate_4h_trends(m));
  try_emplace(D_1.count(), evaluate_1d_trends(m));
  classify();
}
