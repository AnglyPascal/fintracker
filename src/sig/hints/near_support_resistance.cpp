#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Hint near_support_resistance_hint(const IndicatorsTrends& ind, int idx) {
  auto price = ind.price(idx);
  auto support_opt = ind.nearest_support_below(idx);
  auto resistance_opt = ind.nearest_resistance_above(idx);
  std::vector<std::string> reasons;

  if (!support_opt && !resistance_opt)
    return HintType::None;

  auto near_support = support_opt && support_opt->get().is_near(price);
  auto near_resistance = resistance_opt && resistance_opt->get().is_near(price);

  // Handle tight range
  if (near_support && near_resistance) {
    double score = 1.0;

    auto support = support_opt->get();
    auto resistance = resistance_opt->get();

    // Check zone strengths
    double support_conf = support.conf;
    double resistance_conf = resistance.conf;
    double avg_conf = (support_conf + resistance_conf) / 2;
    score += (avg_conf - 0.5) * 0.6;

    auto lo = std::min(support.lo, resistance.lo);
    auto hi = std::max(support.hi, resistance.hi);

    reasons.push_back(std::format("tight range [{:.2f}, {:.2f}]", lo, hi));

    if (avg_conf >= 0.8)
      reasons.push_back("strong zones");
    else if (avg_conf >= 0.6)
      reasons.push_back("medium zones");
    else
      reasons.push_back("weak zones");

    // Tighter range is more significant
    double range = hi - lo;
    double range_pct = range / price;
    if (range_pct < 0.02) {
      score += 0.2;
      reasons.push_back("very tight");
    } else if (range_pct > 0.04) {
      score -= 0.2;
      reasons.push_back("wide range");
    }

    std::string desc = join(reasons.begin(), reasons.end(), ", ");
    return {HintType::WithinTightRange, std::clamp(score, 0.6, 1.5), desc};
  }

  // Handle near support
  if (near_support) {
    double score = 1.0;
    auto& support = support_opt->get();

    reasons.push_back(std::format("at ~{:.2f}", (support.hi + support.lo) / 2));

    // Zone confidence
    score += (support.conf - 0.5) * 0.8;
    if (support.conf >= 0.8)
      reasons.push_back("strong support");
    else if (support.conf >= 0.6)
      reasons.push_back("medium support");
    else
      reasons.push_back("weak support");

    // Distance to support
    double distance = support.distance(price);
    double distance_pct = distance / price;
    if (distance_pct < 0.003) {
      score += 0.2;
      reasons.push_back("very close");
    } else if (distance_pct > 0.005) {
      score -= 0.15;
      reasons.push_back("distant");
    }

    // Previous hits
    if (support.hits.size() >= 3) {
      score += 0.15;
      reasons.push_back(std::format("{}+ hits", support.hits.size()));
    }

    // Recent tests
    bool recent_test = false;
    for (const auto& hit : support.hits)
      if (idx - hit.r < 10) {
        recent_test = true;
        break;
      }

    if (recent_test) {
      score -= 0.2;
      reasons.push_back("recent test");
    }

    std::string desc = join(reasons.begin(), reasons.end(), ", ");
    return support.is_strong() ? Hint{HintType::NearStrongSupport,
                                      std::clamp(score, 0.5, 1.6), desc}
                               : Hint{HintType::NearWeakSupport,
                                      std::clamp(score, 0.4, 1.3), desc};
  }

  // Handle near resistance
  if (near_resistance) {
    double score = 1.0;
    auto& resistance = resistance_opt->get();

    reasons.push_back(
        std::format("at ~{:.2f}", (resistance.hi + resistance.lo) / 2));

    // Zone confidence
    score += (resistance.conf - 0.5) * 0.8;
    if (resistance.conf >= 0.8)
      reasons.push_back("strong resistance");
    else if (resistance.conf >= 0.6)
      reasons.push_back("medium resistance");
    else
      reasons.push_back("weak resistance");

    // Distance to resistance
    double distance = resistance.distance(price);
    double distance_pct = distance / price;
    if (distance_pct < 0.003) {
      score += 0.25;
      reasons.push_back("very close");
    } else if (distance_pct > 0.005) {
      score -= 0.15;
      reasons.push_back("distant");
    }

    // Number of rejections
    if (resistance.hits.size() >= 3) {
      score += 0.2;
      reasons.push_back(std::format("{}+ rejections", resistance.hits.size()));
    }

    // Recent rejection check
    bool recent_rejection = false;
    for (const auto& hit : resistance.hits)
      if (idx - hit.r < 10) {
        recent_rejection = true;
        break;
      }

    if (recent_rejection) {
      score += 0.15;
      reasons.push_back("recent rejection");
    }

    std::string desc = join(reasons.begin(), reasons.end(), ", ");
    return resistance.is_strong() ? Hint{HintType::NearStrongResistance,
                                         std::clamp(score, 0.5, 1.7), desc}
                                  : Hint{HintType::NearWeakResistance,
                                         std::clamp(score, 0.4, 1.4), desc};
  }

  return HintType::None;
}
