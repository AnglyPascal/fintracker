#include "config.h"
#include "indicators.h"
#include "signals.h"

inline auto& sig_config = config.sig_config;

// Filters

std::vector<Filter> evaluate_1d_trend(const Indicators& ind) {
  std::vector<Filter> res;

  double slope = ind.ema21_trend(-1).slope();
  double r2 = ind.ema21_trend(-1).r2;
  double rsi = ind.rsi(-1);

  // Trend strength
  if (slope > 0.08 && r2 > 0.8 && rsi > 55)
    res.emplace_back(Trend::StrongUptrend, Confidence::High, "⇗");
  else if (slope > 0.02 && r2 > 0.6 && rsi > 50)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "↗");
  else if (rsi > 45)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "🡒");
  else
    res.emplace_back(Trend::Bearish, Confidence::High, "↘");

  // EMA21 alignment
  if (ind.price(-1) > ind.ema21(-1))
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "p⌃21");
  else if (ind.price(-1) > ind.ema21(-1) * 0.99)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p~21");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "p⌄21");

  // EMA50
  if (ind.price(-1) > ind.ema50(-1))
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p⌃50");
  else if (ind.price(-1) > ind.ema50(-1) * 0.99)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p~50");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "p⌄50");

  // RSI signal
  if (rsi >= 50 && rsi <= 65)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "r50-65");
  else if (rsi > 65)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "r>65");
  else if (rsi < 50 && rsi > 40)
    res.emplace_back(Trend::Caution, Confidence::Low, "r40-50");
  else
    res.emplace_back(Trend::Bearish, Confidence::High, "r<40");

  if (ind.atr(-1) / ind.price(-1) < 0.01)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "lowVol");

  return res;
}

Filter potential_base(const Indicators& ind_4h) {
  const int base_window = 6;  // last 6 candles (24 hours on 4H)
  int start = -base_window;

  // Price compression (range < 1.5 * ATR)
  double max_price = ind_4h.price(start);
  double min_price = ind_4h.price(start);
  for (int i = start + 1; i < -1; ++i) {
    max_price = std::max(max_price, ind_4h.price(i));
    min_price = std::min(min_price, ind_4h.price(i));
  }
  double price_range = max_price - min_price;
  double atr = ind_4h.atr(-1);
  bool tight_range = price_range < 1.5 * atr;

  // MACD histogram rising
  bool hist_rising =
      ind_4h.hist(-1) > ind_4h.hist(-2) && ind_4h.hist(-2) > ind_4h.hist(-3);

  // RSI stable or rising
  bool rsi_stable = ind_4h.rsi(-1) >= ind_4h.rsi(-2) - 2;

  // EMA21 trend flat or rising
  auto ema_trend = ind_4h.ema21_trend(-1);
  bool ema_flat_or_up = ema_trend.slope() >= -0.0001;

  // No lower lows (support holding)
  bool higher_lows = true;
  for (int i = start + 1; i < -1; ++i) {
    if (ind_4h.low(i) < ind_4h.low(i - 1)) {
      higher_lows = false;
      break;
    }
  }

  if (tight_range && hist_rising && rsi_stable && ema_flat_or_up && higher_lows)
    return {Trend::StrongUptrend, Confidence::High, "base+"};

  if ((tight_range && ema_flat_or_up) && (hist_rising || rsi_stable) &&
      higher_lows)
    return {Trend::ModerateUptrend, Confidence::Medium, "base~"};

  if (tight_range && !hist_rising && !higher_lows)
    return {Trend::NeutralOrSideways, Confidence::Low, "base-"};

  return {};
}

std::vector<Filter> evaluate_4h_trend(const Indicators& ind_4h) {
  std::vector<Filter> res;

  auto& ind = ind_4h;

  auto ema_trend = ind.ema21_trend(-1);
  auto rsi_trend = ind.rsi_trend(-1);

  double ema_slope = ema_trend.slope();
  double ema_r2 = ema_trend.r2;

  double rsi = ind.rsi(-1);
  double rsi_slope = rsi_trend.slope();
  double rsi_r2 = rsi_trend.r2;

  if (ema_slope > 0.10 && ema_r2 > 0.8 && rsi > 55 && rsi_slope > 0.1 &&
      rsi_r2 > 0.7)
    res.emplace_back(Trend::StrongUptrend, Confidence::High, "⇗");
  else if ((ema_slope > 0.03 && ema_r2 > 0.6 && rsi > 50) ||
           (rsi_slope > 0.05 && rsi_r2 > 0.6 && rsi > 50))
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "↗");
  else if (rsi > 45)
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "🡒");
  else
    res.emplace_back(Trend::Bearish, Confidence::Medium, "↘");

  if (ind.hist(-1) > 0)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "h+");
  else if (ind.hist(-1) > ind.hist(-2))
    res.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "h~");
  else
    res.emplace_back(Trend::Bearish, Confidence::Low, "h-");

  if (rsi > 65)
    res.emplace_back(Trend::StrongUptrend, Confidence::Low, "r>65");
  else if (rsi < 65 && rsi > 50)
    res.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "r50-65");
  else if (rsi < 50 && rsi > 40)
    res.emplace_back(Trend::Caution, Confidence::Medium, "r40-50");
  else
    res.emplace_back(Trend::Bearish, Confidence::Low, "r<40");

  res.emplace_back(potential_base(ind_4h));

  return res;
}

Filters evaluate_filters(const Metrics& m) {
  Filters filters;
  filters.try_emplace(H_4.count(), evaluate_4h_trend(m.ind_4h));
  filters.try_emplace(D_1.count(), evaluate_1d_trend(m.ind_1d));
  return filters;
}
