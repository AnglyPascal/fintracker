#pragma once

#include "signal.h"

#include <sstream>
#include <string>
#include <vector>

struct Point {
  double x;
  double y;
};

struct LinearRegression {
  double slope = 0.0;
  double intercept = 0.0;

  LinearRegression() noexcept = default;
  LinearRegression(const std::vector<Point>& vals) noexcept;

  double predict(double x) const { return slope * x + intercept; }
  std::string to_str() const;
};

struct TrendLine {
  int period = 0;
  double r2 = 0.0;
  LinearRegression line;

  double slope() const { return line.slope; }
  double eval(size_t idx) const { return line.predict(idx); }

  std::string to_str() const;
  bool operator<(const TrendLine& other) const;
};

class TrendLines {
 public:
  std::vector<TrendLine> top_trends;

  TrendLines() noexcept = default;
  TrendLines(const std::vector<double>& series,
             int min_period = 10,
             int max_period = 60,
             int top_n = 3) noexcept;

  std::string to_str() const;

 private:
  static double r_squared(const std::vector<Point>& y_vals,
                          const LinearRegression& lr);
};

struct Indicators;
struct Metrics;

struct Trends {
  TrendLines price, ema21, rsi, macd, histogram;

  Trends() noexcept = default;
  Trends(const Indicators& ind) noexcept;
};

enum class ForecastType {
  NEUTRAL,
  PROMISING,
  ALARMING,
};

