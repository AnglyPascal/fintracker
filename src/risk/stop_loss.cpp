#include "risk/stop_loss.h"

#include "util/config.h"
#include "util/format.h"

#include <algorithm>

inline auto& risk_config = config.risk_config;

// Pure calculation functions
inline double calculate_atr_multiplier(double daily_atr_pct) {
  if (daily_atr_pct > 0.04)
    return 2.3;
  if (daily_atr_pct > 0.03)
    return 2.05;
  if (daily_atr_pct > 0.02)
    return 1.8;
  return 1.6;
}

inline double calculate_support_buffer(double daily_atr_pct) {
  if (daily_atr_pct > 0.03)
    return 0.015;
  if (daily_atr_pct > 0.02)
    return 0.012;
  return 0.01;
}

inline double calculate_regime_adjustment(MarketRegime regime) {
  switch (regime) {
    case MarketRegime::TRENDING_UP:
      return 1.0;
    case MarketRegime::CHOPPY_BULLISH:
      return 1.15;
    case MarketRegime::RANGE_BOUND:
      return 1.2;
    case MarketRegime::CHOPPY_BEARISH:
      return 1.15;
    case MarketRegime::TRENDING_DOWN:
      return 1.05;
  }
  return 1.0;
}

inline double calculate_time_adjustment(int days_held) {
  if (days_held == 0)
    return 1.1;
  if (days_held == 1)
    return 1.07;
  if (days_held == 2)
    return 1.05;
  return 1.0;
}

inline double calculate_trailing_adjustment(int days_to_1r) {
  if (days_to_1r <= 3)
    return 0.7;
  else if (days_to_1r <= 7)
    return 0.85;
  else
    return 1.0;
}

inline std::pair<double, double> calculate_base_atr_stop(
    const Metrics& m,
    LocalTimePoint tp,
    MarketRegime regime)  //
{
  auto& ind_1h = m.ind_1h;
  auto& ind_1d = m.ind_1d;

  auto idx_1h = ind_1h.idx_for_time(tp);
  auto idx_1d = ind_1d.idx_for_time(tp);

  double price = ind_1h.price(idx_1h);
  double daily_atr = ind_1d.atr(idx_1d);
  double daily_atr_pct = daily_atr / price;

  double atr_multiplier = calculate_atr_multiplier(daily_atr_pct);
  double regime_adj = calculate_regime_adjustment(regime);
  double effective_multiplier = atr_multiplier * regime_adj;

  return {atr_multiplier, price - (daily_atr * effective_multiplier)};
}

inline double calculate_support_stop(const Metrics& m,
                                     LocalTimePoint tp,
                                     double atr_stop) {
  auto& ind_1d = m.ind_1d;
  auto idx_1d = ind_1d.idx_for_time(tp);
  double price = ind_1d.price(idx_1d);

  auto support_1d = ind_1d.nearest_support_below(idx_1d);
  if (!support_1d)
    return 0.0;

  double daily_atr_pct = ind_1d.atr(idx_1d) / price;
  double buffer = calculate_support_buffer(daily_atr_pct);

  double candidate_stop = support_1d->get().lo * (1.0 - buffer);

  // Validate support stop isn't too tight or too far
  bool too_tight = candidate_stop > price * 0.97;
  bool too_far = atr_stop > 0 && candidate_stop < atr_stop * 0.85;

  if (too_tight || too_far)
    return 0.0;

  return candidate_stop;
}

inline double apply_time_adjustment(double entry_price,
                                    double base_stop,
                                    int days) {
  double time_adj = calculate_time_adjustment(days);
  double stop_distance = entry_price - base_stop;
  return entry_price - (stop_distance * time_adj);
}

inline double calculate_trailing_stop(double max_seen,
                                      double daily_atr,
                                      double atr_multiplier,
                                      int days_to_1r) {
  double trailing_multiplier = calculate_trailing_adjustment(days_to_1r);
  double trailing_distance = daily_atr * atr_multiplier * trailing_multiplier;
  return max_seen - trailing_distance;
}

inline int calculate_days_held(LocalTimePoint entry_time,
                               LocalTimePoint current_time) {
  auto duration = current_time - entry_time;
  return std::chrono::duration_cast<std::chrono::days>(duration).count();
}

constexpr std::string_view color_of_stops(std::string_view str) {
  if (str == "atr")
    return "amethyst";
  if (str == "support")
    return "arctic-teal";
  if (str == "trailing")
    return "seafoam";
  if (str == "initial")
    return "champagne";
  return "";
}

template <>
constexpr std::string_view color_of(StopContext sc) {
  if (sc == StopContext::NEW_POSITION)
    return color_of("wishful");
  if (sc == StopContext::SCALE_UP_ENTRY)
    return color_of("semi-good");
  if (sc == StopContext::EXISTING_INITIAL)
    return color_of("caution");
  if (sc == StopContext::EXISTING_STANDARD)
    return color_of("neutral");
  if (sc == StopContext::EXISTING_TRAILING)
    return color_of("good");
  return "gray";
}

template <>
constexpr std::string to_str(const StopContext& sc) {
  if (sc == StopContext::NEW_POSITION)
    return "new position";
  if (sc == StopContext::SCALE_UP_ENTRY)
    return "scale up entry";
  if (sc == StopContext::EXISTING_INITIAL)
    return "initial";
  if (sc == StopContext::EXISTING_STANDARD)
    return "standard";
  if (sc == StopContext::EXISTING_TRAILING)
    return "trailing";
  return "";
}

StopLoss::StopLoss(const Metrics& m,
                   LocalTimePoint tp,
                   MarketRegime regime_param,
                   StopContext ctx) noexcept
    : regime{regime_param}  //
{
  context = !m.has_position()                  ? StopContext::NEW_POSITION
            : ctx != StopContext::NEW_POSITION ? ctx
                                               : StopContext::EXISTING_STANDARD;

  auto& ind_1h = m.ind_1h;
  auto& ind_1d = m.ind_1d;

  auto idx_1h = ind_1h.idx_for_time(tp);
  auto idx_1d = ind_1d.idx_for_time(tp);

  double price = ind_1h.price(idx_1h);
  double daily_atr = ind_1d.atr(idx_1d);
  double daily_atr_pct = daily_atr / price;

  std::deque<fmt_string> reasons;
  std::vector<fmt_string> details;

  auto [atr_mult, atr_base] = calculate_base_atr_stop(m, tp, regime);
  atr_multiplier = atr_mult;
  atr_stop_base = atr_base;

  support_stop_base = calculate_support_stop(m, tp, atr_stop_base);

  // atr details
  double regime_adj = calculate_regime_adjustment(regime);
  details.emplace_back("atr stop: {:.2f} ({} x {})", atr_stop_base,
                       tagged(atr_multiplier, color_of_stops("atr")),
                       tagged(regime_adj, "slate"));

  // Support details
  if (support_stop_base > 0) {
    double buffer = calculate_support_buffer(daily_atr_pct);
    details.emplace_back("support: {:.2f} (-{}% buffer)", support_stop_base,
                         tagged(buffer * 100, color_of_stops("support")));
  }

  if (context == StopContext::NEW_POSITION) {
    new_position_stop = std::max(atr_stop_base, support_stop_base);
    entry_price = price;

    if (support_stop_base > atr_stop_base) {
      reasons.emplace_back("using " +
                           tagged("support", color_of_stops("support"), BOLD));
    } else {
      reasons.emplace_back("using " +
                           tagged("atr", color_of_stops("atr"), BOLD));
    }
    reasons.emplace_back("{} (-{}%)", tagged(new_position_stop, "blue", BOLD),
                         tagged(get_stop_percentage(price) * 100, "blue"));

  } else if (context == StopContext::SCALE_UP_ENTRY) {
    scale_up_stop = std::max(atr_stop_base, support_stop_base);

    if (support_stop_base > atr_stop_base) {
      reasons.emplace_back("using " +
                           tagged("support", color_of_stops("support"), BOLD));
    } else {
      reasons.emplace_back("using " +
                           tagged("atr", color_of_stops("atr"), BOLD));
    }
    reasons.emplace_back(
        "{} (-{}%)", tagged(scale_up_stop, color_of_stops("trailing"), BOLD),
        tagged((price - scale_up_stop) / price * 100,
               color_of_stops("trailing")));

  } else {
    // Position exists, determine context automatically
    auto pos = m.position;
    entry_price = pos->px;
    max_price_seen = pos->max_price_seen;
    days_held = m.days_held();

    // Calculate standard stop to determine R achievement
    standard_period_stop = std::max(atr_stop_base, support_stop_base);

    double profit_pct = (max_price_seen - entry_price) / entry_price;
    double stop_distance = entry_price - standard_period_stop;
    double r_achieved = profit_pct / (stop_distance / entry_price);

    if (r_achieved >= 1.0) {
      context = StopContext::EXISTING_TRAILING;

      int estimated_days_to_1r = std::max(1, days_held - 3);
      double trailing_adj = calculate_trailing_adjustment(estimated_days_to_1r);

      trailing_stop_value = calculate_trailing_stop(
          max_price_seen, daily_atr, atr_multiplier, estimated_days_to_1r);
      trailing_stop_value = std::max(trailing_stop_value, entry_price);

      if (estimated_days_to_1r <= 3) {
        reasons.emplace_back(
            tagged("tight (fast 1R)", color_of("caution"), BOLD));
      } else if (estimated_days_to_1r <= 7) {
        reasons.emplace_back(tagged("moderate", color_of("semi-good")));
      } else {
        reasons.emplace_back(tagged("loose", color_of("neutral")));
      }

      details.emplace_back("from max {} at {}x",
                           tagged(max_price_seen, color_of("info")),
                           tagged(trailing_adj, color_of("info")));

      reasons.emplace_back(
          "{} (-{}%)",
          tagged(trailing_stop_value, color_of_stops("trailing"), BOLD),
          tagged((price - trailing_stop_value) / price * 100,
                 color_of_stops("trailing")));

    } else if (days_held <= 2) {
      context = StopContext::EXISTING_INITIAL;
      double base_stop = std::max(atr_stop_base, support_stop_base);
      double time_adj = calculate_time_adjustment(days_held);

      initial_period_stop =
          apply_time_adjustment(entry_price, base_stop, days_held);

      if (time_adj > 1.0) {
        reasons.emplace_back("{}% wider", tagged((int)((time_adj - 1.0) * 100),
                                                 color_of("risk")));
      }
      reasons.emplace_back(
          "{} (-{}%)",
          tagged(initial_period_stop, color_of_stops("initial"), BOLD),
          tagged((entry_price - initial_period_stop) / entry_price * 100,
                 color_of_stops("initial")));

    } else {
      context = StopContext::EXISTING_STANDARD;

      if (support_stop_base > atr_stop_base) {
        reasons.emplace_back(
            "using " + tagged("support", color_of_stops("support"), BOLD));
      } else {
        reasons.emplace_back("using  " +
                             tagged("atr", color_of_stops("atr"), BOLD));
      }
      reasons.emplace_back(
          "{} (-{}%)", tagged(standard_period_stop, "blue", BOLD),
          tagged((entry_price - standard_period_stop) / entry_price * 100,
                 "blue"));
    }
  }

  if (context != StopContext::EXISTING_INITIAL)
    reasons.emplace_front(tagged(context, color_of(context), BOLD));
  else
    reasons.emplace_front("{}, (day {})",
                          tagged(context, color_of(context), BOLD),
                          days_held + 1);

  // Add regime context
  reasons.emplace_back(tagged(regime, "blue"));
  if (regime_adj > 1.0) {
    details.emplace_back(
        "regime adj +{}%",
        tagged((int)((regime_adj - 1.0) * 100), color_of("info")));
  }

  // Add volatility context
  details.emplace_back("{}% daily vol",
                       tagged(daily_atr_pct * 100, color_of("info")));

  // Build final rationale
  rationale = std::format(
      "{}<br><div class=\"rationale-details\">{}</div>",
      join(reasons.begin(), reasons.end(), tagged(" | ", color_of("comment"))),
      join(details.begin(), details.end(), tagged(" | ", color_of("comment"))));
}

// Primary interface methods
double StopLoss::get_stop_price() const {
  switch (context) {
    case StopContext::NEW_POSITION:
      return new_position_stop;
    case StopContext::EXISTING_INITIAL:
      return initial_period_stop;
    case StopContext::EXISTING_STANDARD:
      return standard_period_stop;
    case StopContext::EXISTING_TRAILING:
      return trailing_stop_value;
    case StopContext::SCALE_UP_ENTRY:
      return scale_up_stop;
  }
  return 0.0;
}

double StopLoss::get_stop_distance(double current_price) const {
  return current_price - get_stop_price();
}

double StopLoss::get_stop_percentage(double current_price) const {
  return get_stop_distance(current_price) / current_price;
}

int StopLoss::calculate_days_held(LocalTimePoint entry_time,
                                  LocalTimePoint current_time) const {
  return ::calculate_days_held(entry_time, current_time);
}

// External function (keeping existing interface)
StopHit calculate_stop_hit(const Metrics& m, const StopLoss& sl) {
  if (!m.has_position())
    return StopHitType::None;

  if (m.days_held() > config.risk_config.max_hold_days)
    return StopHitType::TimeExit;

  auto price = m.last_price();
  double stop_price = sl.get_stop_price();

  if (price < stop_price)
    return StopHitType::StopLossHit;

  auto dist = price - stop_price;

  if (dist > 0 &&
      dist < m.ind_1h.atr(-1) * config.sig_config.stop_atr_proximity)
    return StopHitType::StopProximity;

  return StopHitType::None;
}
