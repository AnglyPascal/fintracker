#include "prediction.h"
#include "indicators.h"
#include "signals.h"

#include <iostream>
#include <numeric>

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

std::string LinearRegression::to_str() const {
  return std::format("y = {:.2f} * x + {:.2f}", slope, intercept);
}

std::string TrendLine::to_str() const {
  std::ostringstream oss;
  oss << "Period: " << period << ", Slope: " << line.slope << ", R²: " << r2;
  return oss.str();
}

bool TrendLine::operator<(const TrendLine& other) const {
  return r2 > other.r2;  // higher R² is better
}

template <template <typename...> class Container, typename T, typename Func>
TrendLines::TrendLines(const Container<T>& series,
                       size_t min_period,
                       size_t max_period,
                       size_t top_n,
                       int last_idx,
                       Func f) noexcept  //
{
  std::vector<TrendLine> candidates;
  auto len = last_idx < 0 ? series.size() + last_idx + 1 : last_idx + 1;

  max_period = std::min(len, max_period);

  std::deque<Point> window;
  for (size_t i = len - max_period; i < len; i++)
    window.emplace_back((double)i, f(series[i]));

  for (size_t p = max_period; p >= min_period; p--, window.pop_front()) {
    LinearRegression lr{window};
    auto r2 = r_squared(window, lr);
    candidates.emplace_back((int)p, r2, lr);
  }

  std::sort(candidates.begin(), candidates.end());

  auto n = std::min<int>(top_n, candidates.size());
  size_t i = 0;
  while (n > 0 && i < candidates.size()) {
    auto& now = candidates[i++];

    bool valid = true;
    for (auto& trend : top_trends) {
      if (std::abs(trend.period - now.period) <= 5) {
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

std::string TrendLines::to_str() const {
  std::ostringstream oss;
  for (const auto& trend : top_trends)
    oss << trend.to_str() << "\n";
  return oss.str();
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

TrendLines Trends::price_trends(const Indicators& ind, int last_idx) noexcept {
  return TrendLines(ind.candles, 10, 60, 3, last_idx,
                    [](const Candle& cdl) { return cdl.close; });
}

TrendLines Trends::rsi_trends(const Indicators& ind, int last_idx) noexcept {
  return TrendLines(ind._rsi.values, 5, 30, 3, last_idx);
}

TrendLines Trends::ema21_trends(const Indicators& ind, int last_idx) noexcept {
  return TrendLines(ind._ema21.values, 15, 75, 3, last_idx);
}

Trends::Trends(const Indicators& ind, int last_idx) noexcept {
  price = price_trends(ind, last_idx);
  ema21 = ema21_trends(ind, last_idx);
  rsi = rsi_trends(ind, last_idx);

  macd = TrendLines(ind._macd.macd_line, 10, 60, 3, last_idx);
  histogram = TrendLines(ind._macd.histogram, 5, 30, 3, last_idx);
}

