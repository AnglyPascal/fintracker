#include "sig/position_sizing.h"
#include "ind/stop_loss.h"
#include "util/config.h"
#include "util/format.h"

#include <spdlog/spdlog.h>

inline auto& sizing_config = config.sizing_config;

inline double round_to(double x, int n = 2) {
  double factor = std::pow(10.0, n);
  return std::round(x * factor) / factor;
}

PositionSizing::PositionSizing(const Metrics& m,
                               const CombinedSignal& sig,
                               const StopLoss& sl) {
  auto current_price = m.last_price();
  std::deque<std::string> reasons;

  auto calc_tf_risk = [](const auto& ind) -> std::pair<double, double> {
    double vol_score = (ind.atr(-1) / ind.price(-1)) * 100.0;
    double alignment = 0.0;
    if (ind.ema9(-1) > ind.ema21(-1))
      alignment += 1.0;
    if (ind.ema21(-1) > ind.ema50(-1))
      alignment += 1.0;
    if (ind.price(-1) > ind.ema9(-1))
      alignment += 1.0;
    return {vol_score, alignment / 3.0};
  };

  auto [vol_1h, align_1h] = calc_tf_risk(m.ind_1h);
  auto [vol_4h, align_4h] = calc_tf_risk(m.ind_4h);
  auto [vol_1d, align_1d] = calc_tf_risk(m.ind_1d);

  double avg_vol = (vol_1h + vol_4h + vol_1d) / 3.0;
  double avg_align = (align_1h + align_4h + align_1d) / 3.0;

  // Volatility risk (0-4 points)
  double vol_risk = avg_vol > 3.0   ? 4.0
                    : avg_vol > 2.0 ? 3.0
                    : avg_vol > 1.5 ? 2.0
                    : avg_vol > 1.0 ? 1.0
                                    : 0.0;
  double trend_risk = (1.0 - avg_align) * 3.0;

  // Signal conflict risk (0-3 points)
  int conflicts = 0;
  for (auto rating :
       {m.ind_1h.signal.type, m.ind_4h.signal.type, m.ind_1d.signal.type})
    if (rating == Rating::Mixed || rating == Rating::Caution ||
        rating == Rating::HoldCautiously || rating == Rating::Exit)
      conflicts++;

  risk = std::min(vol_risk + trend_risk + conflicts, 10.0);

  // Base position calculation
  double risk_per_share = current_price - sl.final_stop;
  risk_pct = risk_per_share / current_price;
  double base_shares = sizing_config.max_risk_amount() / risk_per_share;

  // Multipliers
  double signal_mult = sig.type == Rating::Entry       ? 1.0
                       : sig.type == Rating::Watchlist ? 0.6
                                                       : 0.3;
  double conf_mult = sig.forecast.conf > 0.8   ? 1.1
                     : sig.forecast.conf < 0.4 ? 0.7
                                               : 1.0;
  double risk_mult = std::max(1.0 - (risk / 20.0), 0.2);
  double total_mult = std::clamp(signal_mult * conf_mult * risk_mult, 0.1, 1.0);

  // Final position size
  rec_shares = round_to(base_shares * total_mult, 2);
  rec_capital = rec_shares * current_price;

  // Position limit check
  auto max_position = sizing_config.max_position_amount();
  bool was_capped = false;
  if (rec_capital > max_position) {
    rec_shares = round_to(max_position / current_price, 2);
    rec_capital = rec_shares * current_price;
    was_capped = true;
  }

  // Final risk calculation
  risk_amount = rec_shares * risk_per_share;
  auto final_risk_pct = risk_amount / sizing_config.capital_usd;

  // Recommendation logic
  bool possible_entry = sig.type == Rating::Entry ||
                        (sig.type == Rating::Watchlist &&
                         sig.score > sizing_config.entry_score_cutoff);
  bool criteria_met = possible_entry &&
                      risk <= sizing_config.entry_risk_cutoff &&
                      sig.forecast.conf > sizing_config.entry_conf_cutoff;

  if (!criteria_met) {
    rec = Recommendation::Avoid;
    reasons.push_front(tagged("avoid", BOLD, "red"));
  } else if (total_mult >= 0.9) {
    rec = Recommendation::StrongBuy;
    reasons.push_front(tagged("strong buy", BOLD, "green"));
  } else if (total_mult >= 0.7) {
    rec = Recommendation::Buy;
    reasons.push_front(tagged("buy", "green"));
  } else if (total_mult >= 0.4) {
    rec = Recommendation::WeakBuy;
    reasons.push_front(tagged("weak buy", "cyan"));
  } else {
    rec = Recommendation::Caution;
    reasons.push_front(tagged("caution", "yellow"));
  }

  auto risk_color = risk > 0.7 ? "red" : risk > 0.4 ? "coral" : "blush";
  reasons.push_back("risk " + tagged(risk, risk_color));
  reasons.push_back(std::format(                 //
      "{} shares, {} ({}%)",                     //
      tagged(rec_shares, "teal-mist", BOLD),     //
      tagged(rec_capital, "teal-mist"),          //
      tagged(final_risk_pct * 100, "teal-mist")  //
      ));
  if (was_capped)
    reasons.push_back(tagged("capped", "warm-beige", BOLD));

  auto signal_desc =  //
      sig.type == Rating::Entry       ? tagged("entry", "forest")
      : sig.type == Rating::Watchlist ? tagged("watchlist", "sage-teal")
                                      : tagged("weak", "frost-2");
  auto conf_desc =  //
      sig.forecast.conf > 0.8   ? tagged("high conf", IT, BOLD)
      : sig.forecast.conf < 0.4 ? "low conf"
                                : tagged("med conf", IT);

  reasons.push_back(std::format("{}, {}", signal_desc, conf_desc));

  // Detailed second line
  std::vector<std::string> details;
  details.push_back(std::format("vol {:.0f}, trend {:.0f}, conflict {}",
                                vol_risk, trend_risk, conflicts));
  details.push_back(std::format("base {:.0f} shares", base_shares));
  details.push_back(std::format("multipliers: {:.1f} * {:.1f} * {:.2f} = {:.2f}",
                                signal_mult, conf_mult, risk_mult, total_mult));

  rationale = std::format(                                          //
      "{}<br><div class=\"rationale-details\">{}</div>",            //
      join(reasons.begin(), reasons.end(), tagged(" | ", "gray")),  //
      join(details.begin(), details.end(), tagged(" | ", "gray"))   //
  );
}
