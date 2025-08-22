#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Reason rsi_cross_50_entry(const IndicatorsTrends& ind, int idx) {
  const int LOOKBACK = sig_config.rsi_cross_lookback;
  auto check = [&ind](int i) {
    return ind.rsi(i - 1) < 50 && ind.rsi(i) >= 50;
  };

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
    reasons.push_back(std::format("{}c ago", age));

  // Cross momentum
  double rsi_momentum = ind.rsi(cross_idx) - ind.rsi(cross_idx - 1);
  if (rsi_momentum > 3.0) {
    score += 0.3;
    reasons.push_back("strong momentum");
  } else if (rsi_momentum < 1.5) {
    score -= 0.3;
    reasons.push_back("weak momentum");
  }

  // Oversold recovery / overbought penalty
  if (ind.rsi(cross_idx - 1) < 40) {
    score += 0.2;
    reasons.push_back("from oversold");
  }

  if (ind.rsi(idx) > 65) {
    score -= 0.3;
    reasons.push_back("near overbought");
  }

  // Sustained strength after cross
  int above_count = 0;
  bool dipped_below = false;
  for (int i = cross_idx; i <= idx; i++) {
    if (ind.rsi(i) >= 50)
      above_count++;
    if (ind.rsi(i) < 48)
      dipped_below = true;
  }

  if (!dipped_below && above_count >= (idx - cross_idx + 1)) {
    score += 0.25;
    reasons.push_back("held above 50");
  } else if (dipped_below && ind.rsi(idx) < 50) {
    score -= 0.3;
    reasons.push_back("failed to hold");
  }

  // General trend direction
  if (ind.rsi(idx) > ind.rsi(cross_idx) + 1.0) {
    score += 0.15;
    reasons.push_back("rising trend");
  } else {
    score -= 0.1;
    reasons.push_back("weak follow-through");
  }

  // Average RSI strength
  double avg_rsi = 0.0;
  for (int i = cross_idx; i <= idx; i++)
    avg_rsi += ind.rsi(i);
  avg_rsi /= (idx - cross_idx + 1);

  if (avg_rsi > 55) {
    score += 0.15;
    reasons.push_back("strong avg");
  } else if (avg_rsi < 50.5) {
    score -= 0.1;
    reasons.push_back("weak avg");
  }

  // Price confirmation
  if (ind.price(idx) > ind.price(cross_idx)) {
    score += 0.1;
    reasons.push_back("price confirmed");
  } else {
    score -= 0.05;
    reasons.push_back("price lagging");
  }

  // Short-term alignment
  if (ind.price(idx) > ind.price(idx - 1) && ind.rsi(idx) > ind.rsi(idx - 1)) {
    score += 0.1;
    reasons.push_back("aligned");
  } else if (ind.price(idx) < ind.price(idx - 1) &&
             ind.rsi(idx) > ind.rsi(idx - 1)) {
    score -= 0.1;
    reasons.push_back("conflicted");
  }

  // Whipsaw penalty
  int recent_crosses = 0;
  for (int i = idx - 6; i < idx; i++)
    if (check(i))
      recent_crosses++;

  if (recent_crosses > 1) {
    score -= recent_crosses * 0.15;
    reasons.push_back(std::format("{}+ whipsaws", recent_crosses));
  }

  std::string desc = join(reasons.begin(), reasons.end(), ", ");
  return {ReasonType::RsiCross50, std::clamp(score, 0.4, 1.5), desc};
}
