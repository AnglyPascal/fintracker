#include "trendlines.h"
#include "config.h"
#include "format.h"
#include "indicators.h"
#include "signals.h"

#include <cmath>
#include <iostream>

template <template <typename...> class Container>
LinearRegression::LinearRegression(const Container<Point>& vals) noexcept {
  auto n = vals.size();
  if (n < 2)
    return;

  double sum_x = 0, sum_y = 0, sum_x2 = 0, sum_xy = 0;
  for (auto [x, y] : vals) {
    sum_x += x;
    sum_y += y;
    sum_x2 += x * x;
    sum_xy += x * y;
  }

  double denom = n * sum_x2 - sum_x * sum_x;
  if (denom == 0.0)
    return;

  slope = (n * sum_xy - sum_x * sum_y) / denom;
  intercept = (sum_y - slope * sum_x) / n;
}

bool TrendLine::operator<(const TrendLine& other) const {
  auto score = [](auto& t) {
    constexpr double alpha = 0.5;
    return t.r2 * std::pow(static_cast<double>(t.period), alpha);
  };
  return score(*this) > score(other);
}

template <typename Func>
TrendLines::TrendLines(const Indicators& ind,
                       Func f,
                       int last_idx,
                       [[maybe_unused]] size_t min_period,
                       size_t max_period,
                       size_t top_n) noexcept  //
{
  std::vector<TrendLine> candidates;

  auto N = ind.size();
  auto len = last_idx < 0 ? N + last_idx + 1 : last_idx + 1;

  max_period = std::min(len, max_period);

  std::deque<Point> window;
  for (int i = len - max_period; i < static_cast<int>(len); i++)
    window.emplace_back(static_cast<double>(i), f(ind, i));

  for (int p = max_period; p >= 0; p--, window.pop_front()) {
    LinearRegression lr{window};
    auto r2 = r_squared(window, lr);
    candidates.emplace_back(p, r2, lr);
  }

  std::sort(candidates.begin(), candidates.end());

  auto n = std::min<int>(top_n, candidates.size());
  size_t i = 0;
  while (n > 0 && i < candidates.size()) {
    auto& now = candidates[i++];

    bool valid = true;
    for (auto& trend : top_trends) {
      auto dist = std::abs(static_cast<int>(trend.period) -
                           static_cast<int>(now.period));
      if (dist < 5) {
        valid = false;
        break;
      }
    }

    if (valid) {
      top_trends.emplace_back(now);
      n--;
    }
  }
}

template <template <typename...> class Container>
double TrendLines::r_squared(const Container<Point>& points,
                             const LinearRegression& lr) {
  int n = points.size();
  if (n < 2)
    return 0.0;

  double mean = 0.0;
  for (auto [x, y] : points)
    mean += y;
  mean /= n;

  auto ss_tot = 0.0, ss_res = 0.0;

  for (auto [x, y] : points) {
    double pred = lr.predict(x);
    ss_tot += (y - mean) * (y - mean);
    ss_res += (y - pred) * (y - pred);
  }

  return (ss_tot == 0.0) ? 0.0 : (1.0 - ss_res / ss_tot);
}

const auto& ind_config = config.ind_config;

TrendLines Trends::price_trends(const Indicators& ind, int last_idx) noexcept {
  return TrendLines(
      ind, [](auto& ind, int idx) { return ind.price(idx); }, last_idx,  //
      ind_config.price_trend_min_candles, ind_config.price_trend_max_candles,
      ind_config.n_top_trends);
}

TrendLines Trends::rsi_trends(const Indicators& ind, int last_idx) noexcept {
  return TrendLines(
      ind, [](auto& ind, int idx) { return ind.rsi(idx); }, last_idx,  //
      ind_config.rsi_trend_min_candles, ind_config.rsi_trend_max_candles,
      ind_config.n_top_trends);
}

TrendLines Trends::ema21_trends(const Indicators& ind, int last_idx) noexcept {
  return TrendLines(
      ind, [](auto& ind, int idx) { return ind.ema21(idx); }, last_idx,  //
      ind_config.ema21_trend_min_candles, ind_config.ema21_trend_max_candles,
      ind_config.n_top_trends);
}

Trends::Trends(const Indicators& ind, int last_idx) noexcept
    : price{price_trends(ind, last_idx)},
      ema21{ema21_trends(ind, last_idx)},
      rsi{rsi_trends(ind, last_idx)} {}
