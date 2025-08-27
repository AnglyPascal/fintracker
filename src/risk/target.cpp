#include "risk/target.h"
#include "util/config.h"
#include "util/format.h"

#include <algorithm>

inline auto& risk_config = config.risk_config;

TargetStrategy ProfitTarget::select_strategy(const CombinedSignal& signal,
                                             double resistance_room) const {
  // High quality setup with lots of room
  if (signal.score > 0.7 && signal.forecast.conf > 0.7 &&
      resistance_room > 0.08)
    return TargetStrategy::AGGRESSIVE;  // 3:1 R:R

  // Good setup with decent room
  if (signal.score > 0.5 && resistance_room > 0.05)
    return TargetStrategy::STANDARD;  // 2:1 R:R

  // Lower quality or limited room
  return TargetStrategy::CONSERVATIVE;  // 1.5:1 R:R
}

double ProfitTarget::calculate_atr_projection(const Metrics& m,
                                              LocalTimePoint tp,
                                              int days) const {
  auto& ind = m.ind_1d;
  auto idx = ind.idx_for_time(tp);
  double daily_atr = ind.atr(idx);

  // Average daily range expectation (not full ATR each day)
  // Stocks typically move 0.5-0.7x ATR in trend direction
  double daily_expected_move = daily_atr * 0.6;

  // Account for volatility clustering
  double recent_volatility = 0;
  for (int i = idx - 5; i < idx; i++)
    recent_volatility += ind.atr(i);
  recent_volatility /= 5;

  if (daily_atr > recent_volatility * 1.2)
    daily_expected_move *= 0.8;  // High vol won't sustain
  else if (daily_atr < recent_volatility * 0.8)
    daily_expected_move *= 1.1;  // Vol expansion possible

  return daily_expected_move * days;
}

ProfitTarget::ProfitTarget(const Metrics& m,
                           LocalTimePoint tp,
                           const CombinedSignal& signal,
                           const StopLoss& stop_loss,
                           const PositionSizing& sizing) noexcept {
  // Skip if position wasn't approved
  if (sizing.rec == Recommendation::Avoid)
    return;

  double current_price = m.last_price();
  double stop_price = stop_loss.get_stop_price();
  double risk_amount = current_price - stop_price;

  std::vector<std::string> reasons;
  std::vector<std::string> details;

  // Find resistance levels across timeframes
  struct ResistanceLevel {
    double price;
    double confidence;
    double distance_pct;
    std::string timeframe;
  };
  std::vector<ResistanceLevel> resistances;

  // Check each timeframe
  auto check_resistance = [&](const auto& ind, const std::string& tf) {
    auto res_opt = ind.nearest_resistance_above(ind.idx_for_time(tp));
    if (res_opt) {
      const auto& zone = res_opt->get();
      double res_price = zone.lo * 0.998;  // Slightly below resistance
      double distance = (res_price - current_price) / current_price;

      if (distance > 0.01 && distance < 0.20) {  // 1% to 20% away
        resistances.push_back({res_price, zone.conf, distance, tf});
      }
    }
  };

  check_resistance(m.ind_1d, "1D");
  check_resistance(m.ind_4h, "4H");
  check_resistance(m.ind_1h, "1H");

  // Sort by confidence and timeframe priority
  std::sort(
      resistances.begin(), resistances.end(), [](const auto& a, const auto& b) {
        // Prioritize daily over 4H over 1H
        int priority_a = a.timeframe == "1D" ? 3 : a.timeframe == "4H" ? 2 : 1;
        int priority_b = b.timeframe == "1D" ? 3 : b.timeframe == "4H" ? 2 : 1;

        if (priority_a != priority_b)
          return priority_a > priority_b;

        return a.confidence > b.confidence;
      });

  // Store nearest strong resistance
  if (!resistances.empty() && resistances[0].confidence >= 0.6) {
    nearest_resistance = resistances[0].price;
    resistance_room_pct = resistances[0].distance_pct;
    resistance_timeframe = resistances[0].timeframe == "1D"   ? D_1
                           : resistances[0].timeframe == "4H" ? H_4
                                                              : H_1;

    details.push_back(std::format("{} resistance at {:.2f} (+{:.1f}%)",
                                  tagged(resistances[0].timeframe, "cyan"),
                                  nearest_resistance,
                                  resistance_room_pct * 100));
  } else {
    resistance_room_pct = 0.15;  // Assume 15% room if no resistance
    details.push_back(tagged("No major resistance", "green"));
  }

  // Select strategy based on setup quality and room
  strategy = select_strategy(signal, resistance_room_pct);

  // Set R:R based on strategy
  double target_rr =
      strategy == TargetStrategy::AGGRESSIVE ? risk_config.max_rr_ratio
      : strategy == TargetStrategy::STANDARD ? risk_config.target_rr_ratio
                                             : risk_config.min_rr_ratio;

  // Calculate price targets
  initial_target = current_price + (risk_amount * risk_config.min_rr_ratio);
  stretch_target = current_price + (risk_amount * risk_config.max_rr_ratio);

  // Primary target based on strategy
  double rr_based_target = current_price + (risk_amount * target_rr);

  // ATR projection for expected days
  expected_days_to_target = strategy == TargetStrategy::AGGRESSIVE ? 12
                            : strategy == TargetStrategy::STANDARD ? 8
                                                                   : 5;

  atr_projection =
      current_price + calculate_atr_projection(m, tp, expected_days_to_target);

  details.push_back(std::format("ATR projection: {:.2f} in {} days",
                                atr_projection, expected_days_to_target));

  // Determine final target
  if (nearest_resistance > 0 && nearest_resistance < rr_based_target) {
    // Resistance is closer than R:R target
    if (nearest_resistance >= initial_target) {
      // At least 1.5:1, use resistance
      target_price = nearest_resistance;
      risk_reward_ratio = (target_price - current_price) / risk_amount;
      reasons.push_back(
          std::format("Using {} resistance",
                      tagged(resistances[0].timeframe, "cyan", BOLD)));
    } else {
      // Resistance too close, skip to next resistance or use R:R
      if (resistances.size() > 1 && resistances[1].price < rr_based_target) {
        target_price = resistances[1].price;
        risk_reward_ratio = (target_price - current_price) / risk_amount;
        reasons.push_back(
            std::format("Using {} resistance (2nd)",
                        tagged(resistances[1].timeframe, "cyan")));
      } else {
        target_price = rr_based_target;
        risk_reward_ratio = target_rr;
        reasons.push_back(std::format("R:R based (res too close)",
                                      tagged(target_rr, "blue", BOLD)));
      }
    }
  } else if (atr_projection < rr_based_target * 0.9) {
    // ATR suggests lower target is more realistic
    target_price = atr_projection;
    risk_reward_ratio = (target_price - current_price) / risk_amount;
    reasons.push_back(std::format("ATR-limited target", tagged("amber", BOLD)));
  } else {
    // Use R:R based target
    target_price = rr_based_target;
    risk_reward_ratio = target_rr;
    reasons.push_back(
        std::format("{}:1 R:R target", tagged(target_rr, "forest", BOLD)));
  }

  // Calculate final metrics
  target_pct = (target_price - current_price) / current_price;

  // Adjust if target seems unrealistic
  if (target_pct > 0.20) {  // >20% target for swing trade is ambitious
    target_price = current_price * 1.15;  // Cap at 15%
    risk_reward_ratio = (target_price - current_price) / risk_amount;
    target_pct = 0.15;
    reasons.push_back(tagged("Capped at 15%", "yellow"));
  }

  // Strategy description
  std::string strategy_str =
      strategy == TargetStrategy::AGGRESSIVE ? "Aggressive"
      : strategy == TargetStrategy::STANDARD ? "Standard"
                                             : "Conservative";

  reasons.insert(reasons.begin(), tagged(strategy_str, "sage", BOLD));

  // Summary
  details.push_back(std::format("Target: {} (+{}%)",
                                tagged(target_price, "emerald", BOLD),
                                tagged(target_pct * 100, "emerald")));
  details.push_back(
      std::format("R:R: {}:1", tagged(risk_reward_ratio, "emerald", BOLD)));

  // Special conditions
  if (risk_reward_ratio < risk_config.min_rr_ratio) {
    details.push_back(tagged("Below min R:R - consider skipping", "red", BOLD));
  }

  if (signal.forecast.exp_pnl > 0) {
    double expected_return = signal.forecast.exp_pnl;
    if (expected_return < target_pct * 100 * 0.5) {
      details.push_back(std::format("Forecast suggests {}% only",
                                    tagged(expected_return, "amber")));
    }
  }

  rationale = std::format("{}<br><div class=\"rationale-details\">{}</div>",
                          join(reasons.begin(), reasons.end(), " | "),
                          join(details.begin(), details.end(), " | "));
}
