#pragma once

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

  template <template <typename...> class Container>
  LinearRegression(const Container<Point>& vals) noexcept;

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

  template <template <typename...> class Container,
            typename T,
            typename Func = decltype([](const T& t) -> double { return t; })>
  TrendLines(const Container<T>& series,
             size_t min_period = 10,
             size_t max_period = 60,
             size_t top_n = 3,
             int last_idx = -1,
             Func f = {}) noexcept;

  std::string to_str() const;

 private:
  template <template <typename...> class Container>
  static double r_squared(const Container<Point>& y_vals,
                          const LinearRegression& lr);
};

struct Indicators;
struct Metrics;

struct Trends {
  TrendLines price, ema21, rsi, macd, histogram;

  Trends() noexcept = default;
  Trends(const Indicators& ind, int last_idx = -1) noexcept;

  static TrendLines price_trends(const Indicators& ind,
                                 int last_idx = -1) noexcept;
  static TrendLines rsi_trends(const Indicators& ind,
                               int last_idx = -1) noexcept;
  static TrendLines ema21_trends(const Indicators& ind,
                               int last_idx = -1) noexcept;
};

enum class ForecastType {
  NEUTRAL,
  PROMISING,
  ALARMING,
};

