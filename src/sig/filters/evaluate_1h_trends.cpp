#include "ind/indicators.h"
#include "sig/filters.h"

std::vector<Filter> evaluate_1h_trends(const Metrics& m) {
  const auto& ind = m.ind_1h;
  std::vector<Filter> filters;

  // Price trend analysis
  auto price_trend = ind.price_trend(-1);
  if (price_trend.slope() > 0.3 && price_trend.r2 > 0.8)
    filters.emplace_back(Trend::StrongUptrend, Confidence::High, "p⇗");
  else if (price_trend.slope() > 0.15 && price_trend.r2 > 0.6)
    filters.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "p↗");
  else if (price_trend.slope() < -0.3 && price_trend.r2 > 0.8)
    filters.emplace_back(Trend::Bearish, Confidence::High, "p⇘");
  else if (price_trend.slope() < -0.15 && price_trend.r2 > 0.6)
    filters.emplace_back(Trend::Bearish, Confidence::Medium, "p↘");
  else if (price_trend.r2 > 0.5)
    filters.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "p→");

  // EMA21 trend analysis
  auto ema21_trend = ind.ema21_trend(-1);
  if (ema21_trend.slope() > 0.3 && ema21_trend.r2 > 0.8)
    filters.emplace_back(Trend::StrongUptrend, Confidence::High, "e21⇗");
  else if (ema21_trend.slope() > 0.15 && ema21_trend.r2 > 0.8)
    filters.emplace_back(Trend::ModerateUptrend, Confidence::High, "e21↗");
  else if (ema21_trend.slope() < -0.3 && ema21_trend.r2 > 0.8)
    filters.emplace_back(Trend::Bearish, Confidence::High, "e21⇘");
  else if (ema21_trend.slope() < -0.15 && ema21_trend.r2 > 0.8)
    filters.emplace_back(Trend::Bearish, Confidence::High, "e21↘");
  else if (ema21_trend.r2 > 0.6)
    filters.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "e21→");

  // RSI trend analysis
  auto rsi_trend = ind.rsi_trend(-1);
  if (rsi_trend.slope() > 0.3 && rsi_trend.r2 > 0.85)
    filters.emplace_back(Trend::StrongUptrend, Confidence::High, "r⇗");
  else if (rsi_trend.slope() > 0.15 && rsi_trend.r2 > 0.7)
    filters.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "r↗");
  else if (rsi_trend.slope() < -0.3 && rsi_trend.r2 > 0.85)
    filters.emplace_back(Trend::Bearish, Confidence::High, "r⇘");
  else if (rsi_trend.slope() < -0.15 && rsi_trend.r2 > 0.7)
    filters.emplace_back(Trend::Bearish, Confidence::Medium, "r↘");
  else if (rsi_trend.r2 > 0.5)
    filters.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "r→");

  // Additional 1H specific filters

  // EMA alignment (structure filter)
  bool emas_bullish =
      ind.ema9(-1) > ind.ema21(-1) && ind.ema21(-1) > ind.ema50(-1);
  bool emas_bearish =
      ind.ema9(-1) < ind.ema21(-1) && ind.ema21(-1) < ind.ema50(-1);
  bool price_above_emas = ind.price(-1) > ind.ema9(-1);
  bool price_below_emas = ind.price(-1) < ind.ema21(-1);

  if (emas_bullish && price_above_emas)
    filters.emplace_back(Trend::ModerateUptrend, Confidence::High, "ema bull");
  else if (emas_bearish && price_below_emas)
    filters.emplace_back(Trend::Bearish, Confidence::High, "ema bear");
  else if (emas_bullish || (ind.ema9(-1) > ind.ema21(-1)))
    filters.emplace_back(Trend::NeutralOrSideways, Confidence::Medium, "ema ?");

  // RSI momentum filter (different from trend)
  double rsi = ind.rsi(-1);
  if (rsi > 60 && rsi < 75 && ind.rsi(-1) > ind.rsi(-2))
    filters.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "rsi+");
  else if (rsi < 40 && rsi > 25 && ind.rsi(-1) < ind.rsi(-2))
    filters.emplace_back(Trend::Bearish, Confidence::Medium, "rsi-");
  else if (rsi > 75)
    filters.emplace_back(Trend::Caution, Confidence::Medium, "rsi>>");
  else if (rsi < 25)
    filters.emplace_back(Trend::Caution, Confidence::Medium, "rsi<<");

  // MACD momentum
  if (ind.hist(-1) > 0 && ind.hist(-1) > ind.hist(-2))
    filters.emplace_back(Trend::ModerateUptrend, Confidence::Medium, "macd+");
  else if (ind.hist(-1) < 0 && ind.hist(-1) < ind.hist(-2))
    filters.emplace_back(Trend::Bearish, Confidence::Medium, "macd-");
  else if (ind.hist(-1) > ind.hist(-2))
    filters.emplace_back(Trend::NeutralOrSideways, Confidence::Low, "macd~");

  return filters;
}
