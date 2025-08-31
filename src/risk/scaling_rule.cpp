#include "risk/risk.h"
#include "util/format.h"

ScalingRules::ScalingRules(const Metrics& m,
                           LocalTimePoint tp,
                           const StopLoss& stop_loss,
                           const PositionSizing& initial_sizing)  //
{
  if (!m.has_position())
    return;

  auto pos = m.position;

  double current_price = m.last_price();
  double entry_price = pos->px;
  double position_pnl_pct = (current_price - entry_price) / entry_price;

  std::vector<fmt_string> reasons;
  std::vector<fmt_string> details;

  // Scale up rules - add to winners
  if (position_pnl_pct >= 0.05) {  // Up 5%
    // Check if breaking resistance
    auto idx_1h = m.ind_1h.idx_for_time(tp);
    auto resistance = m.ind_1h.nearest_resistance_above(idx_1h);
    bool breaking_resistance = false;

    if (resistance) {
      double res_price = resistance->get().lo;
      if (current_price > res_price * 0.995 &&
          current_price < res_price * 1.005) {
        breaking_resistance = true;
      }
    }

    if (breaking_resistance || position_pnl_pct >= 0.07) {
      can_add = true;
      add_trigger_price = current_price * 1.01;  // Add on 1% more gain
      add_size_shares = initial_sizing.rec_shares * 0.25;  // Add 25% of initial

      reasons.push_back(std::format(
          "can add {} shares at {}", tagged(add_size_shares, color_of("good")),
          tagged(add_trigger_price, color_of("good"))));

      if (breaking_resistance)
        details.push_back(
            tagged("breaking resistance", color_of("good"), BOLD));
    }
  }

  // Scale down rules - trim losers
  double halfway_to_stop =
      entry_price - (entry_price - stop_loss.get_stop_price()) * 0.5;

  if (current_price <= halfway_to_stop) {
    should_trim = true;
    trim_trigger_price = current_price * 0.99;  // Trim on 1% more loss
    trim_size_shares = pos->qty * 0.5;          // Trim half

    details.push_back(tagged("halfway to stop", color_of("semi-bad"), BOLD));

    // Check if at support - maybe don't trim
    auto idx_1h = m.ind_1h.idx_for_time(tp);
    auto support = m.ind_1h.nearest_support_below(idx_1h);
    if (support && support->get().is_near(current_price)) {
      should_trim = false;
      details.push_back(
          tagged("but at support - holding", color_of("wishful")));
    }

    if (should_trim)
      reasons.emplace_back("should trim {} shares at {}",
                           tagged(trim_size_shares, color_of("semi-bad")),
                           tagged(trim_trigger_price, color_of("semi-bad")));
  }

  if (!can_add && !should_trim) {
    reasons.push_back(tagged("no scaling action", color_of("comment")));
  }

  rationale = std::format(
      "{}<br><div class=\"rationale-details\">{}</div>",
      join(reasons.begin(), reasons.end(), tagged(" | ", color_of("comment"))),
      join(details.begin(), details.end(), tagged(" | ", color_of("comment"))));
}
