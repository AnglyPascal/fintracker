#pragma once

#include <vector>

struct Point {
  double x;
  double y;
};

struct LinearRegression {
  double slope = 0.0;
  double intercept = 0.0;

  LinearRegression() noexcept = default;

  template <template <typename...> class Container>
  LinearRegression(const Container<Point>& vals) noexcept;

  double predict(double x) const { return slope * x + intercept; }
};

struct TrendLine {
  size_t period = 0;
  double r2 = 0.0;
  LinearRegression line;

  double slope() const { return line.slope; }
  double eval(size_t idx) const { return line.predict(idx); }

  bool operator<(const TrendLine& other) const;
};

struct IndicatorsCore;

struct TrendLines {
  std::vector<TrendLine> top_trends;

  TrendLines() noexcept = default;

  template <typename Func>
  TrendLines(const IndicatorsCore& ind,
             Func f,
             int last_idx,
             size_t min_period,
             size_t max_period,
             size_t top_n) noexcept;

  TrendLine operator[](size_t idx) const {
    if (!top_trends.empty() && idx < top_trends.size())
      return top_trends[idx];
    return {};
  }

 private:
  template <template <typename...> class Container>
  static double r_squared(const Container<Point>& y_vals,
                          const LinearRegression& lr);
};

struct Trends {
  TrendLines price, ema21, rsi;

  Trends() noexcept = default;
  Trends(const IndicatorsCore& ind, int last_idx = -1) noexcept;

  static TrendLines price_trends(const IndicatorsCore& ind,
                                 int last_idx) noexcept;
  static TrendLines ema21_trends(const IndicatorsCore& ind,
                                 int last_idx) noexcept;
  static TrendLines rsi_trends(const IndicatorsCore& ind,
                               int last_idx) noexcept;
};
