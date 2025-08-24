#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

Filter swing_base_pattern(const Metrics& m) {
  auto& ind = m.ind_4h;

  const int base_window = 10;  // 40 hours for better base detection
  double current_price = ind.price(-1);
  double atr = ind.atr(-1);

  // Get actual support/resistance levels
  auto support_opt = ind.nearest_support_below(-1);
  auto resistance_opt = ind.nearest_resistance_above(-1);

  // Price compression analysis
  double max_price = current_price;
  double min_price = current_price;
  for (int i = -base_window; i <= -1; ++i) {
    max_price = std::max(max_price, ind.high(i));
    min_price = std::min(min_price, ind.low(i));
  }
  double price_range = max_price - min_price;
  bool tight_range = price_range < 2.0 * atr;

  // Trend analysis within base
  auto price_trend = ind.price_trend(-1);
  bool consolidating = std::abs(price_trend.slope()) < 0.05;

  // Support/Resistance based analysis
  bool near_support = false;
  bool support_holding = false;
  bool resistance_overhead = false;

  if (support_opt) {
    auto& support = support_opt->get();
    near_support = support.is_near(current_price);

    // Check if support held during base formation
    support_holding = true;
    for (int i = -base_window; i <= -1; ++i) {
      if (ind.low(i) < support.lo * 0.98) {  // Allow 2% penetration
        support_holding = false;
        break;
      }
    }
  }

  if (resistance_opt) {
    auto& resistance = resistance_opt->get();
    double distance_to_resistance =
        (resistance.lo - current_price) / current_price;
    resistance_overhead = distance_to_resistance > 0 &&
                          distance_to_resistance < 0.05;  // Within 5%
  }

  // Volume/momentum during base
  bool momentum_building = ind.rsi(-1) > ind.rsi(-3) && ind.rsi(-1) > 45;

  // MACD improvement
  bool macd_improving = ind.hist(-1) > ind.hist(-3);

  // Enhanced scoring with actual S/R levels
  int base_score = 0;
  std::string base_str = "base";
  std::string base_desc = "Base: ";

  if (tight_range) {
    base_score++;
    base_str += "T";  // Tight
    base_desc += tagged("tight range", "yellow") + ", ";
  }

  if (consolidating) {
    base_score++;
    base_str += "C";  // Consolidating
    base_desc += tagged("consolidating", "yellow") + ", ";
  }

  if (support_holding && near_support) {
    base_score += 2;   // Strong support is worth more
    base_str += "S+";  // Strong support
    base_desc += tagged("strong support holding", "green", BOLD) + ", ";
  } else if (support_holding) {
    base_score++;
    base_str += "S";  // Support holding
    base_desc += tagged("support holding", "green") + ", ";
  }

  if (momentum_building) {
    base_score++;
    base_str += "M";  // Momentum
    base_desc += tagged("momentum building", "green") + ", ";
  }

  if (macd_improving) {
    base_score++;
    base_str += "H";  // Histogram improving
    base_desc += tagged("MACD improving", "green") + ", ";
  }

  // Bonus/penalty for resistance overhead
  if (resistance_overhead) {
    base_score--;      // Nearby resistance is concerning
    base_str += "R-";  // Resistance overhead
    base_desc += tagged("resistance overhead", "red") + ", ";
  }

  // Remove trailing comma and space
  if (base_desc.ends_with(", "))
    base_desc = base_desc.substr(0, base_desc.length() - 2);

  // Quality assessment
  if (base_score >= 5)
    return {Trend::StrongUptrend, Confidence::High, base_str + "+++",
            "Excellent base pattern.<br>" + base_desc};
  else if (base_score >= 4)
    return {Trend::ModerateUptrend, Confidence::High, base_str + "++",
            "Strong base pattern.<br>" + base_desc};
  else if (base_score >= 3)
    return {Trend::ModerateUptrend, Confidence::Medium, base_str + "+",
            "Moderate base pattern.<br>" + base_desc};
  else if (base_score >= 2 && (tight_range || support_holding))
    return {Trend::NeutralOrSideways, Confidence::Medium, base_str + "~",
            "Weak base pattern.<br>" + base_desc};
  else
    return {Trend::Caution, Confidence::Low, "noBase",
            "No base pattern: lack of consolidation, "
            "unclear support structure"};
}

std::vector<Filter> evaluate_4h_trends(const Metrics& m) {
  auto& ind = m.ind_4h;
  std::vector<Filter> res;

  auto price_trend = ind.price_trend(-1);
  auto ema_trend = ind.ema21_trend(-1);
  double rsi = ind.rsi(-1);

  // Primary 4H trend (confirmation role for swing trades)
  if (price_trend.slope() > 0.15 && price_trend.r2 > 0.8 && rsi > 55)
    res.emplace_back(Trend::StrongUptrend, Confidence::High, "⇗",
                     "Strong 4H price uptrend: slope > 0.15, "
                     "R² > 0.8, RSI > 55");
  else if (price_trend.slope() > 0.05 && ema_trend.slope() > 0.02 && rsi > 50)
    res.emplace_back(Trend::ModerateUptrend, Confidence::High, "↗",
                     "Moderate 4H uptrend: price slope > 0.05, "
                     "EMA21 rising, RSI > 50");
  else if (rsi > 45 && ema_trend.slope() > -0.02)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "→",
                     "Neutral 4H trend: RSI > 45, stable EMA21");
  else if (price_trend.slope() < -0.05 && rsi < 45)
    res.emplace_back(Trend::Bearish, Confidence::Medium, "↘",
                     "Bearish 4H trend: price slope < -0.05, RSI < 45");
  else
    res.emplace_back(Trend::Caution, Confidence::Low, "?",
                     "Unclear 4H price trend: mixed directional signals");

  // MACD momentum (important for swing timing)
  if (ind.hist(-1) > 0 && ind.macd(-1) > ind.macd_signal(-1))
    res.emplace_back(
        Trend::ModerateUptrend, Confidence::High, "macd++",
        "Strong MACD momentum: histogram positive, MACD above signal line");
  else if (ind.hist(-1) > ind.hist(-2) && ind.hist(-2) > ind.hist(-3))
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "macd+",
                     "Improving MACD: histogram rising for 3 periods");
  else if (ind.hist(-1) > ind.hist(-2))
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "macd~",
                     "Neutral MACD: histogram slightly improving");
  else
    res.emplace_back(Trend::Caution, Confidence::Medium, "macd-",
                     "Weak MACD: histogram declining");

  // RSI positioning
  if (rsi >= 50 && rsi <= 70)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "rsi+",
                     "Healthy 4H RSI: between 50-70, good momentum zone");
  else if (rsi > 70)
    res.emplace_back(Trend::Caution, Confidence::Medium, "rsi>>",
                     "Overbought 4H RSI: above 70, caution for pullback");
  else if (rsi >= 40 && rsi < 50)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "rsi~",
                     "Neutral 4H RSI: between 40-50, balanced");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "rsi-",
                     "Weak 4H RSI: below 40, bearish momentum");

  // Add base pattern analysis
  res.emplace_back(swing_base_pattern(m));

  return res;
}
