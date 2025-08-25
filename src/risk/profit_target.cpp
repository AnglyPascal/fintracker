#include "risk/profit_target.h"
#include "ind/indicators.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sizing_config = config.sizing_config;

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
