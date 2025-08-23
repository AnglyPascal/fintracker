#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Reason macd_histogram_cross_entry(const IndicatorsTrends& ind, int idx) {
  const int LOOKBACK = sig_config.macd_cross_lookback;
  auto check = [&ind](int i) {
    return ind.hist(i - 1) < 0 && ind.hist(i) >= 0;
  };

  if (ind.hist(idx) < 0)
    return ReasonType::None;

  int cross_idx = idx;
  double score = 1.0;
  std::vector<std::string> reasons;

  // Find the cross
  for (; cross_idx > idx - LOOKBACK; cross_idx--) {
    if (check(cross_idx))
      break;
    score -= 0.1;
  }

  if (cross_idx == idx - LOOKBACK)
    return ReasonType::None;

  // Age penalty description
  int age = idx - cross_idx;
  if (age > 0)
    reasons.push_back(std::format("-{}c", age));
  else 
    reasons.push_back("now");

  // Momentum of the cross
  double hist_momentum = ind.hist(cross_idx) - ind.hist(cross_idx - 1);
  if (hist_momentum > 0.001) {
    score += 0.3;
    reasons.push_back("strong momentum");
  } else if (hist_momentum < 0.0005) {
    score -= 0.2;
    reasons.push_back("weak momentum");
  }

  // Oversold bounce potential
  double pre_cross_hist = ind.hist(cross_idx - 1);
  if (pre_cross_hist < -0.002) {
    score += 0.2;
    reasons.push_back("from oversold");
  } else if (pre_cross_hist > -0.0005) {
    score -= 0.1;
    reasons.push_back("shallow base");
  }

  // Sustained improvement after cross
  int rising_periods = 0;
  for (int i = cross_idx; i <= idx && i > 0; i++) {
    if (ind.hist(i) > ind.hist(i - 1))
      rising_periods++;
    else
      break;
  }

  if (rising_periods >= 2) {
    score += 0.2;
    reasons.push_back("sustained rise");
  } else if (rising_periods == 0 && cross_idx != idx) {
    score -= 0.3;
    reasons.push_back("immediate fade");
  }

  // Histogram trend leading into cross
  int improving_periods = 0;
  for (int i = cross_idx - 3; i < cross_idx; i++) {
    if (i > 0 && ind.hist(i) > ind.hist(i - 1))
      improving_periods++;
  }

  if (improving_periods >= 2) {
    score += 0.15;
    reasons.push_back("buildup");
  }

  // MACD line context
  if (ind.macd(idx) > 0) {
    score += 0.1;
    reasons.push_back("macd positive");
  } else if (ind.macd(idx) > ind.macd(idx - 1)) {
    score += 0.05;
    reasons.push_back("macd rising");
  } else {
    score -= 0.1;
    reasons.push_back("macd falling");
  }

  // Divergence context
  bool price_higher = ind.price(idx) > ind.price(idx - 2);
  bool hist_higher = ind.hist(idx) > ind.hist(idx - 2);
  if (price_higher && hist_higher) {
    score += 0.1;
    reasons.push_back("confirmed div");
  } else if (!price_higher && hist_higher) {
    score += 0.05;
    reasons.push_back("hidden div");
  } else {
    score -= 0.1;
    reasons.push_back("conflicted div");
  }

  // Whipsaw penalty
  int recent_crosses = 0;
  for (int i = idx - 2 * LOOKBACK; i < idx; i++)
    if (check(i))
      recent_crosses++;

  if (recent_crosses > 1) {
    score -= recent_crosses * 0.15;
    reasons.push_back(std::format("{}+ whipsaws", recent_crosses));
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {ReasonType::MacdBullishCross, std::clamp(score, 0.4, 1.8), desc};
}
