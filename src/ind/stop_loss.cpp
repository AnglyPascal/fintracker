#include "ind/stop_loss.h"
#include "ind/indicators.h"
#include "util/config.h"
#include "util/format.h"

auto& sizing_config = config.sizing_config;

StopLoss::StopLoss(const Metrics& m) noexcept {
  auto& ind = m.ind_1h;

  auto price = ind.price(-1);
  auto atr = ind.atr(-1);

  auto pos = m.position;
  auto entry_price = m.has_position() ? pos->px : price;
  auto max_price_seen = m.has_position() ? pos->max_price_seen : price;

  std::vector<std::string> reasons;
  std::vector<std::string> details;

  constexpr auto atr_color = "amethyst";
  constexpr auto swing_color = "arctic-teal";
  constexpr auto hard_color = "coral";

  is_trailing =
      m.has_position() &&
      (max_price_seen > entry_price + sizing_config.trailing_trigger_atr * atr);

  if (is_trailing) {
    reasons.push_back(tagged("trailing", "sage", BOLD));

    atr_stop = max_price_seen - sizing_config.trailing_atr_multiplier * atr;
    details.emplace_back("atr: " + tagged(atr_stop, atr_color));

    double hard_stop = max_price_seen * (1.0 - sizing_config.trailing_stop_pct);
    details.emplace_back("hard: " + tagged(hard_stop, hard_color));

    final_stop = std::min(atr_stop, hard_stop);
    reasons.push_back(                                         //
        "using: " +                                            //
        (final_stop == atr_stop ? tagged("atr", atr_color)     //
                                : tagged("hard", hard_color))  //
    );

  } else {
    reasons.push_back(tagged("initial", "champagne", BOLD));

    atr_stop = entry_price - sizing_config.stop_atr_multiplier * atr;
    details.emplace_back("atr: " + tagged(atr_stop, atr_color));

    auto support_1d = m.ind_1d.nearest_support_below(-1);
    if (support_1d) {
      swing_low = support_1d->get().lo * 0.998;
      details.emplace_back("support: " + tagged(swing_low, swing_color));

      // Only use support if it's not too tight
      if (swing_low < atr_stop * 0.85) {
        reasons.push_back(tagged("tight support", "storm"));
        swing_low = 0.0;
      }
    }

    double hard_stop = entry_price * (1.0 - sizing_config.stop_pct);
    details.emplace_back("max loss: " + tagged(hard_stop, hard_color));

    final_stop = atr_stop;
    if (swing_low > 0 && swing_low < final_stop) {
      final_stop = swing_low;
      reasons.push_back("using " + tagged("support", swing_color, BOLD));
    } else {
      reasons.push_back("using " + tagged("atr", atr_color, BOLD));
    }

    // But never go below hard stop
    if (final_stop < hard_stop) {
      final_stop = hard_stop;
      reasons.push_back(tagged("capped at ", IT) +
                        tagged("max loss", hard_color, IT));
    }
  }

  stop_pct = (entry_price - final_stop) / entry_price;

  auto final_stop_color = final_stop == atr_stop    ? atr_color
                          : final_stop == swing_low ? swing_color
                                                    : hard_color;
  details.push_back(std::format(                                      //
      "final: {} ({}%)", tagged(final_stop, final_stop_color, BOLD),  //
      tagged(stop_pct * 100, final_stop_color)                        //
      ));

  rationale = std::format(                                //
      "{}<br><div class=\"rationale-details\">{}</div>",  //
      join(reasons.begin(), reasons.end(), ", "),         //
      join(details.begin(), details.end(), ", ")          //
  );
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

  // No support found - use conservative 0.3% gap tolerance
  if (!primary_support)
    return stop_price * (1.0 - 0.003);

  const auto& support_zone = primary_support->get();
  double support_conf = support_zone.conf;

  // Support-based limit price calculation
  if (support_conf >= 0.8) {
    // Strong support: limit slightly above support zone
    double support_based_limit = support_zone.lo * 1.002;

    // Ensure we don't set limit above our stop price
    double max_gap_limit = stop_price * (1.0 - base_gap_tolerance);

    return std::min(support_based_limit, max_gap_limit);
  }
  // Moderate support: use full 0.5% gap tolerance
  else if (support_conf >= 0.6) {
    return stop_price * (1.0 - base_gap_tolerance);
  }
  // Weak support: tighter 0.3% gap tolerance
  else {
    return stop_price * (1.0 - 0.003);
  }
}

struct ResistanceOption {
  std::string timeframe;
  double price;
  double conf;
  double rr_ratio;

  bool operator<(const ResistanceOption& b) const {
    auto& a = *this;
    if (a.timeframe != b.timeframe) {
      // prefer 1d > 4h > 1h
      int priority_a = a.timeframe == "1d" ? 3 : a.timeframe == "4h" ? 2 : 1;
      int priority_b = b.timeframe == "1d" ? 3 : b.timeframe == "4h" ? 2 : 1;
      return priority_a < priority_b;
    }
    return a.conf < b.conf;
  };
};

ProfitTarget::ProfitTarget(const Metrics& m,
                           const StopLoss& stop_loss) noexcept {
  auto entry_price = m.has_position() ? m.position->px : m.ind_1h.price(-1);
  auto stop_price = stop_loss.final_stop;
  auto risk_amount = entry_price - stop_price;

  std::vector<ResistanceOption> resistance_options;

  auto add_res = [=, &resistance_options](auto& ind, const std::string& tf) {
    auto res_opt = ind.nearest_resistance_above(-1);
    if (res_opt) {
      auto& zone = res_opt->get();
      double target = zone.lo * 0.998;
      double rr = (target - entry_price) / risk_amount;
      if (zone.conf >= 0.7 && rr >= 1.5) {
        resistance_options.emplace_back(tf, target, zone.conf, rr);
      }
    }
  };

  add_res(m.ind_1d, "1d");
  add_res(m.ind_4h, "4h");
  add_res(m.ind_1h, "1h");

  // Calculate percentage-based target
  double config_target = entry_price * (1.0 + sizing_config.profit_pct);
  double min_rr_target = entry_price + (2.0 * risk_amount);
  double percentage_target = std::max(config_target, min_rr_target);
  double percentage_rr = (percentage_target - entry_price) / risk_amount;

  // Target selection logic
  if (!resistance_options.empty()) {
    auto best =
        std::max_element(resistance_options.begin(), resistance_options.end());
    double resistance_target = best->price;
    double resistance_rr = best->rr_ratio;

    if (resistance_rr >= 1.5 && resistance_rr <= percentage_rr * 1.2) {
      type = TargetType::RESISTANCE_BASED;
      target_price = resistance_target;
      risk_reward_ratio = resistance_rr;
      resistance_conf = best->conf;
      resistance_inv = best->timeframe == "1d"   ? D_1
                       : best->timeframe == "4h" ? H_4
                                                 : H_1;

      rationale = std::format(                                     //
          "{}: {:.2f} ({:.1f}:1)",                                 //
          tagged(std::format("{}_res", best->timeframe), "cyan"),  //
          target_price, risk_reward_ratio                          //
      );
    } else {
      type = TargetType::PERCENTAGE_BASED;
      target_price = percentage_target;
      risk_reward_ratio = percentage_rr;

      auto reason = resistance_rr < 1.5  //
                        ? tagged("too close", "red")
                        : tagged("too far", "yellow");
      rationale = std::format(                                        //
          "{}: {:.2f} ({:.1f}:1), resistance {} at {:.2f}",           //
          tagged("perc", "blue"),                                     //
          target_price, risk_reward_ratio, reason, resistance_target  //
      );
    }
  } else {
    type = TargetType::PERCENTAGE_BASED;
    target_price = percentage_target;
    risk_reward_ratio = percentage_rr;

    rationale = std::format(                     //
        "{}: {:.2f} ({:.1f}:1), no resistance",  //
        tagged("perc", "arctic-teal"),           //
        target_price, risk_reward_ratio          //
    );
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
