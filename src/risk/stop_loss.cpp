#include "risk/stop_loss.h"
#include "ind/indicators.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sizing_config = config.sizing_config;

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
