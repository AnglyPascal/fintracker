#include "ind/indicators.h"
#include "sig/signals.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

auto evaluate_1d_trend(const Metrics& m) {
  std::vector<Filter> res;

  auto& ind = m.ind_1d;
  double slope = ind.ema21_trend(-1).slope();
  double r2 = ind.ema21_trend(-1).r2;
  double rsi = ind.rsi(-1);
  double price_vs_ema21 = (ind.price(-1) - ind.ema21(-1)) / ind.ema21(-1);
  double price_vs_ema50 = (ind.price(-1) - ind.ema50(-1)) / ind.ema50(-1);

  // Primary trend assessment (more important for swing trades)
  if (slope > 0.12 && r2 > 0.85 && rsi > 60 && price_vs_ema21 > 0.02)
    res.emplace_back(Trend::StrongUptrend, Confidence::High, "⇗",
                     "Strong daily uptrend: EMA21 slope > 0.12, R² > 0.85, "
                     "RSI > 60, price 2%+ above EMA21");
  else if (slope > 0.05 && r2 > 0.7 && rsi > 50 && price_vs_ema21 > 0)
    res.emplace_back(Trend::ModerateUptrend, Confidence::High, "↗",
                     "Moderate daily uptrend: EMA21 slope > 0.05, R² > 0.7, "
                     "RSI > 50, price above EMA21");
  else if (slope > -0.02 && rsi > 45 && price_vs_ema21 > -0.02)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "→",
                     "Neutral daily trend: minimal EMA21 slope, "
                     "RSI > 45, price near EMA21");
  else if (slope < -0.05 && rsi < 40)
    res.emplace_back(Trend::Bearish, Confidence::High, "↘",
                     "Bearish daily trend: EMA21 slope < -0.05, RSI < 40");
  else
    res.emplace_back(Trend::Caution, Confidence::Medium, "?",
                     "Cautionary daily trend: mixed signals, "
                     "unclear direction");

  // EMA structure - critical for swing trades
  if (ind.ema21(-1) > ind.ema50(-1) && price_vs_ema21 > 0.01)
    res.emplace_back(
        Trend::ModerateUptrend, Confidence::High, "ema↗",
        "Bullish EMA structure: EMA21 > EMA50, price 1%+ above EMA21");
  else if (ind.ema21(-1) > ind.ema50(-1) && price_vs_ema21 > -0.01)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "ema→",
                     "Neutral EMA structure: EMA21 > EMA50, price near EMA21");
  else if (ind.ema21(-1) < ind.ema50(-1) && price_vs_ema21 < -0.02)
    res.emplace_back(Trend::Bearish, Confidence::High, "ema↘",
                     "Bearish EMA structure: EMA21 < EMA50, "
                     "price 2%+ below EMA21");
  else
    res.emplace_back(Trend::Caution, Confidence::Low, "ema?",
                     "Unclear EMA structure: mixed positioning signals");

  // EMA50 position - major trend context for swing trades
  if (price_vs_ema50 > 0.03)
    res.emplace_back(Trend::ModerateUptrend, Confidence::High, "p>>50",
                     "Strong position above EMA50: "
                     "price 3%+ above daily EMA50");
  else if (price_vs_ema50 > 0)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "p>50",
                     "Above EMA50: price moderately above daily EMA50");
  else if (price_vs_ema50 > -0.03)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p~50",
                     "Near EMA50: price within 3% of daily EMA50");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "p<<50",
                     "Below EMA50: price 3%+ below daily EMA50");

  // RSI regime - less restrictive for swing trades
  if (rsi >= 55 && rsi <= 75)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "rsi+",
                     "Healthy RSI: between 55-75, "
                     "bullish momentum without overbought");
  else if (rsi > 75)
    res.emplace_back(Trend::Caution, Confidence::Medium, "rsi>>",
                     "Overbought RSI: above 75, "
                     "potential pullback risk");
  else if (rsi >= 45 && rsi < 55)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "rsi→",
                     "Neutral RSI: between 45-55, "
                     "balanced momentum");
  else if (rsi >= 30 && rsi < 45)
    res.emplace_back(Trend::Caution, Confidence::Medium, "rsi↓",
                     "Weak RSI: between 30-45, "
                     "bearish momentum");
  else
    res.emplace_back(Trend::Bearish, Confidence::High, "rsi<<",
                     "Oversold RSI: below 30, strong "
                     "bearish momentum");

  // Volatility context
  double atr_pct = ind.atr(-1) / ind.price(-1);
  if (atr_pct < 0.015)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "vol",
                     "Low volatility: ATR < 1.5% of price, "
                     "compressed trading");
  else if (atr_pct > 0.04)
    res.emplace_back(Trend::Caution, Confidence::Medium, "Vol",
                     "High volatility: ATR > 4% of price, "
                     "elevated risk");

  return res;
}

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
    base_desc += tagged("tight range", YELLOW) + ", ";
  }

  if (consolidating) {
    base_score++;
    base_str += "C";  // Consolidating
    base_desc += tagged("consolidating", YELLOW) + ", ";
  }

  if (support_holding && near_support) {
    base_score += 2;   // Strong support is worth more
    base_str += "S+";  // Strong support
    base_desc += tagged("strong support holding", GREEN, BOLD) + ", ";
  } else if (support_holding) {
    base_score++;
    base_str += "S";  // Support holding
    base_desc += tagged("support holding", GREEN) + ", ";
  }

  if (momentum_building) {
    base_score++;
    base_str += "M";  // Momentum
    base_desc += tagged("momentum building", GREEN) + ", ";
  }

  if (macd_improving) {
    base_score++;
    base_str += "H";  // Histogram improving
    base_desc += tagged("MACD improving", GREEN) + ", ";
  }

  // Bonus/penalty for resistance overhead
  if (resistance_overhead) {
    base_score--;      // Nearby resistance is concerning
    base_str += "R-";  // Resistance overhead
    base_desc += tagged("resistance overhead", RED) + ", ";
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

std::vector<Filter> evaluate_4h_trend(const Metrics& m) {
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

// Add this new function for multi-timeframe filters
std::vector<Filter> evaluate_timeframe_alignment(const Metrics& m) {
  std::vector<Filter> res;

  auto sig_1h = m.get_signal(H_1);
  auto sig_4h = m.get_signal(H_4);
  auto sig_1d = m.get_signal(D_1);

  // Higher timeframe alignment (from confirmations)
  if (sig_1h.type == Rating::Entry) {
    if (sig_4h.type == Rating::Entry || sig_4h.type == Rating::Watchlist)
      res.emplace_back(Trend::ModerateUptrend, Confidence::High, "4h↗",
                       "4H timeframe confirms 1H entry signal");

    if (sig_1d.type == Rating::Entry || sig_1d.type == Rating::Watchlist)
      res.emplace_back(Trend::StrongUptrend, Confidence::High, "1d↗",
                       "Daily timeframe confirms 1H entry signal");
  }

  // Daily trend alignment (from confirmations)
  const auto& ind_1d = m.ind_1d;
  bool daily_uptrend = ind_1d.ema21(-1) > ind_1d.ema50(-1);
  bool price_above_daily_ema = ind_1d.price(-1) > ind_1d.ema21(-1);

  if (daily_uptrend && price_above_daily_ema)
    res.emplace_back(Trend::ModerateUptrend, Confidence::High, "1dTrend",
                     "Daily trend alignment: EMA21 > EMA50, price above EMA21");

  return res;
}

Filters evaluate_filters(const Metrics& m) {
  Filters filters;
  filters.try_emplace(H_1.count(), evaluate_timeframe_alignment(m));
  filters.try_emplace(H_4.count(), evaluate_4h_trend(m));
  filters.try_emplace(D_1.count(), evaluate_1d_trend(m));
  return filters;
}
