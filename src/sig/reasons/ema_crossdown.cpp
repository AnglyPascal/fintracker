#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Reason ema_crossdown_exit(const IndicatorsTrends& ind, int idx) {
  const int LOOKBACK = sig_config.ema_cross_lookback;
  int cross_idx = idx;
  double score = 1.0;
  std::vector<std::string> reasons;

  // Find the crossdown
  for (; cross_idx > idx - LOOKBACK; cross_idx--) {
    if (ind.ema9(cross_idx - 1) >= ind.ema21(cross_idx - 1) &&
        ind.ema9(cross_idx) < ind.ema21(cross_idx))
      break;
    score -= 0.15;
  }

  if (cross_idx == idx - LOOKBACK)
    return ReasonType::None;

  // Age penalty description
  int age = idx - cross_idx;
  if (age > 0)
    reasons.push_back(std::format("-{}c", age));
  else 
    reasons.push_back("now");

  // Separation bonus
  if (ind.ema21(idx) > ind.ema9(idx) * 1.005) {
    score += 0.2;
    reasons.push_back("wide spread");
  }

  // Price confirmation
  if (ind.price(idx) < ind.price(idx - 1)) {
    score += 0.1;
    reasons.push_back("price down");
  }

  // Whipsaw analysis
  int crossover_count = 0, crossdown_count = 0;
  for (int i = idx - 8; i < idx; i++) {
    if (ind.ema9(i - 1) <= ind.ema21(i - 1) && ind.ema9(i) > ind.ema21(i))
      crossover_count++;
    if (ind.ema9(i - 1) >= ind.ema21(i - 1) && ind.ema9(i) < ind.ema21(i))
      crossdown_count++;
  }

  int total_crosses = crossover_count + crossdown_count;
  if (total_crosses >= 3) {
    score += 0.4;
    reasons.push_back("very choppy");
  } else if (total_crosses == 2) {
    score += 0.2;
    reasons.push_back("choppy");
  } else if (total_crosses == 1) {
    score += 0.1;
    reasons.push_back("some chop");
  }

  // Recent whipsaw urgency
  int recent_crosses = 0;
  for (int i = idx - 4; i < idx; i++) {
    if ((ind.ema9(i - 1) <= ind.ema21(i - 1) && ind.ema9(i) > ind.ema21(i)) ||
        (ind.ema9(i - 1) >= ind.ema21(i - 1) && ind.ema9(i) < ind.ema21(i)))
      recent_crosses++;
  }

  if (recent_crosses >= 2) {
    score += 0.3;
    reasons.push_back(std::format("{} recent whipsaws", recent_crosses));
  }

  // Strengthening breakdown
  double current_spread = (ind.ema21(idx) - ind.ema9(idx)) / ind.ema21(idx);
  double cross_spread =
      (ind.ema21(cross_idx) - ind.ema9(cross_idx)) / ind.ema21(cross_idx);
  if (current_spread > cross_spread * 1.1) {
    score += 0.2;
    reasons.push_back("strengthening");
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {ReasonType::EmaCrossdown, std::clamp(score, 0.3, 1.5), desc};
}
