#include "ind/indicators.h"
#include "sig/filters.h"
#include "util/config.h"
#include "util/format.h"

inline auto& sig_config = config.sig_config;

std::vector<Filter> evaluate_1d_trends(const Metrics& m) {
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
