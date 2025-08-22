#include "ind/stop_loss.h"
#include "ind/indicators.h"
#include "util/config.h"

auto& sizing_config = config.sizing_config;

// StopLoss::StopLoss(const Metrics& m) noexcept {
//   auto& ind = m.ind_1h;

//   auto price = ind.price(-1);
//   auto atr = ind.atr(-1);

//   auto pos = m.position;
//   auto entry_price = m.has_position() ? pos->px : price;
//   auto max_price_seen = m.has_position() ? pos->max_price_seen : price;

//   // trailing kicks in after ~1R move
//   is_trailing =
//       m.has_position() &&
//       (max_price_seen > entry_price + sizing_config.trailing_trigger_atr *
//       atr);

//   auto support_1d = m.ind_1d.nearest_support_below(-1);
//   auto support_4h = m.ind_4h.nearest_support_below(-1);
//   auto support_1h = m.ind_1h.nearest_support_below(-1);

//   auto primary_support =
//       support_1d ? support_1d : (support_4h ? support_4h : support_1h);

//   swing_low = 0.0;
//   if (primary_support) {
//     double buffer = support_1d ? 0.012 : support_4h ? 0.008 : 0.005;
//     swing_low = primary_support->get().lo * (1 - buffer);
//   }

//   double hard_stop = 0.0;

//   // Initial Stop
//   if (!is_trailing) {
//     ema_stop = ind.ema21(-1) * (1.0 - sizing_config.ema_stop_pct);
//     auto best = std::min(swing_low, ema_stop) * 0.999;

//     atr_stop = entry_price - sizing_config.stop_atr_multiplier * atr;
//     hard_stop = entry_price * (1.0 - sizing_config.stop_pct);

//     final_stop = std::max({best, atr_stop, hard_stop});
//     stop_pct = (entry_price - final_stop) / entry_price;
//   }

//   // Trailing Stop
//   else {
//     atr_stop = max_price_seen - sizing_config.trailing_atr_multiplier * atr;
//     hard_stop = max_price_seen * (1.0 - sizing_config.trailing_stop_pct);

//     final_stop = std::max({swing_low, atr_stop, hard_stop});
//     stop_act = (price - final_stop) / price;
//   }
// }

StopLoss::StopLoss(const Metrics& m) noexcept {
  auto& ind = m.ind_1h;

  auto price = ind.price(-1);
  auto atr = ind.atr(-1);

  auto pos = m.position;
  auto entry_price = m.has_position() ? pos->px : price;
  auto max_price_seen = m.has_position() ? pos->max_price_seen : price;

  // Determine if we're in profit or loss
  bool in_profit = price > entry_price;

  // trailing kicks in after ~1R move
  is_trailing =
      m.has_position() &&
      (max_price_seen > entry_price + sizing_config.trailing_trigger_atr * atr);

  auto support_1d = m.ind_1d.nearest_support_below(-1);
  auto support_4h = m.ind_4h.nearest_support_below(-1);
  auto support_1h = m.ind_1h.nearest_support_below(-1);

  auto primary_support =
      support_1d ? support_1d : (support_4h ? support_4h : support_1h);

  swing_low = 0.0;
  if (primary_support) {
    double buffer = support_1d ? 0.012 : support_4h ? 0.008 : 0.005;
    swing_low = primary_support->get().lo * (1 - buffer);
  }

  double hard_stop = 0.0;

  // Initial Stop
  if (!is_trailing) {
    ema_stop = ind.ema21(-1) * (1.0 - sizing_config.ema_stop_pct);
    auto best = std::min(swing_low, ema_stop) * 0.999;

    atr_stop = entry_price - sizing_config.stop_atr_multiplier * atr;
    hard_stop = entry_price * (1.0 - sizing_config.stop_pct);

    final_stop = std::max({best, atr_stop, hard_stop});
    stop_pct = (entry_price - final_stop) / entry_price;
  }
  // Trailing Stop
  else {
    atr_stop = max_price_seen - sizing_config.trailing_atr_multiplier * atr;
    hard_stop = max_price_seen * (1.0 - sizing_config.trailing_stop_pct);

    final_stop = std::max({swing_low, atr_stop, hard_stop});
    stop_pct = (price - final_stop) / price;
  }

  // Hybrid approach: Use stop-limit only for losing trades
  use_stop_limit = !in_profit && m.has_position();

  if (use_stop_limit) {
    order_type = StopOrderType::STOP_LIMIT;
    limit_price = calculate_limit_price(m, final_stop);
    limit_pct = (price - limit_price) / price;
  } else {
    order_type = StopOrderType::STOP_LOSS;
    limit_price = final_stop;  // Same as stop for regular stop-loss
    limit_pct = stop_pct;
  }
}

double StopLoss::calculate_limit_price(const Metrics& m,
                                       double stop_price) const {
  auto support_1d = m.ind_1d.nearest_support_below(-1);
  auto support_4h = m.ind_4h.nearest_support_below(-1);
  auto support_1h = m.ind_1h.nearest_support_below(-1);

  auto primary_support =
      support_1d ? support_1d : (support_4h ? support_4h : support_1h);

  // Base gap tolerance: 0.5%
  double base_gap_tolerance = 0.005;

  if (!primary_support) {
    // No support found - use conservative 0.3% gap tolerance
    return stop_price * (1.0 - 0.003);
  }

  const auto& support_zone = primary_support->get();
  double support_conf = support_zone.conf;

  // Support-based limit price calculation
  if (support_conf >= 0.8) {
    // Strong support: limit slightly above support zone
    double support_based_limit = support_zone.lo * 1.002;

    // Ensure we don't set limit above our stop price
    double max_gap_limit = stop_price * (1.0 - base_gap_tolerance);

    return std::min(support_based_limit, max_gap_limit);
  } else if (support_conf >= 0.6) {
    // Moderate support: use full 0.5% gap tolerance
    return stop_price * (1.0 - base_gap_tolerance);
  } else {
    // Weak support: tighter 0.3% gap tolerance
    return stop_price * (1.0 - 0.003);
  }
}

ProfitTarget::ProfitTarget(const Metrics& m,
                           const StopLoss& stop_loss) noexcept {
  if (!m.has_position())
    return;

  auto entry_price = m.position->px;
  auto stop_price = stop_loss.final_stop;

  auto [zone, timeframe] = calculate_resistance_target(m);
  auto resistance_target = zone.lo * 0.998;

  double percentage_target =
      calculate_percentage_target(entry_price, stop_price);

  // Determine which approach to use
  bool has_strong_resistance =
      resistance_target > 0.0 && resistance_conf >= 0.7;
  double risk_amount = entry_price - stop_price;

  if (has_strong_resistance) {
    double resistance_rr = (resistance_target - entry_price) / risk_amount;
    double percentage_rr = (percentage_target - entry_price) / risk_amount;

    // Use resistance if it provides reasonable R:R (1.5:1 minimum)
    if (resistance_rr >= 1.5 && resistance_rr <= percentage_rr * 1.2) {
      type = TargetType::RESISTANCE_BASED;
      target_price = resistance_target;
      risk_reward_ratio = resistance_rr;
      // rationale = "Strong " + resistance_timeframe + " resistance at " +
      //             std::to_string(resistance_rr) + ":1 R:R";
    } else {
      type = TargetType::PERCENTAGE_BASED;
      target_price = percentage_target;
      risk_reward_ratio = percentage_rr;
      // rationale = "Resistance too close/far - using " +
      //             std::to_string(percentage_rr) + ":1 R:R target";
    }
  } else {
    type = TargetType::PERCENTAGE_BASED;
    target_price = percentage_target;
    risk_reward_ratio = (percentage_target - entry_price) / risk_amount;
    // rationale = "No strong resistance nearby - using " +
    //             std::to_string(risk_reward_ratio) + ":1 R:R target";
  }

  target_pct = (target_price - entry_price) / entry_price;
}

std::pair<Zone, minutes> ProfitTarget::calculate_resistance_target(
    const Metrics& m) const {
  // Check resistance levels in order of preference: 1D -> 4H -> 1H
  auto resistance_1d = m.ind_1d.nearest_resistance_above(-1);
  auto resistance_4h = m.ind_4h.nearest_resistance_above(-1);
  auto resistance_1h = m.ind_1h.nearest_resistance_above(-1);

  if (resistance_1d) {
    auto& zone = resistance_1d->get();
    if (zone.conf >= 0.7)
      return {zone, D_1};
  }

  if (resistance_4h) {
    const auto& zone = resistance_4h->get();
    if (zone.conf >= 0.7)
      return {zone, H_4};
  }

  if (resistance_1h) {
    const auto& zone = resistance_1h->get();
    if (zone.conf >= 0.6)
      return {zone, H_1};
  }

  return {};
}

double ProfitTarget::calculate_percentage_target(double entry_price,
                                                 double stop_price) const {
  double risk_amount = entry_price - stop_price;

  // Use config profit percentage as base, but ensure minimum 2:1 R:R
  double config_target = entry_price * (1.0 + sizing_config.profit_pct);
  double min_rr_target = entry_price + (2.0 * risk_amount);  // 2:1 minimum

  return std::max(config_target, min_rr_target);
}
