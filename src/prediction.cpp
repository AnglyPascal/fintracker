#include "prediction.h"
#include "indicators.h"
#include "signal.h"

#include <iostream>
#include <numeric>

LinearRegression::LinearRegression(const std::vector<Point>& vals) noexcept {
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

TrendLines::TrendLines(const std::vector<double>& series,
                       int min_period,
                       int max_period,
                       int top_n) noexcept  //
{
  std::vector<TrendLine> candidates;
  int len = static_cast<int>(series.size());

  std::vector<Point> window;
  window.reserve(max_period);

  for (int p = min_period; p <= max_period; ++p) {
    if (len < p)
      continue;

    window.clear();
    for (size_t i = series.size() - p; i < series.size(); i++)
      window.emplace_back((double)i, series[i]);

    LinearRegression lr(window);
    double r2 = r_squared(window, lr);

    candidates.push_back({p, r2, lr});
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

double TrendLines::r_squared(const std::vector<Point>& points,
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

Trends::Trends(const Indicators& ind) noexcept {
  std::vector<double> prices;
  for (const auto& c : ind.candles)
    prices.push_back(c.price());

  price = TrendLines(prices, 10, 60, 3);
  ema21 = TrendLines(ind.ema21.values, 15, 75, 3);
  rsi = TrendLines(ind.rsi.values, 5, 30, 3);

  macd = TrendLines(ind.macd.macd_line, 10, 60, 3);
  histogram = TrendLines(ind.macd.histogram, 5, 30, 3);
}

