#include "support_resistance.h"
#include "indicators.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

constexpr double PERCENT(double x) {
  return x / 100.0;
}

inline bool is_swing_low(auto& ind, int i, int window = 3) {
  double cur = ind.low(i);
  for (int j = 1; j <= window; ++j) {
    if (ind.low(i - j) <= cur || ind.low(i + j) <= cur)
      return false;
  }
  return true;
}

inline bool is_swing_high(auto& ind, int i, int window = 3) {
  double cur = ind.high(i);
  for (int j = 1; j <= window; ++j) {
    if (ind.high(i - j) >= cur || ind.high(i + j) >= cur)
      return false;
  }
  return true;
}

// FIXME: also fix count_bounces
template <bool is_support>
int count_bounces(auto& ind, double lower, double upper) {
  int count = 0;
  constexpr size_t max_inside = 3;

  auto N = ind.candles.size();
  for (size_t i = 3; i < N - max_inside - 1; ++i) {
    double prev_close = ind.price(i - 1);
    double curr_close = ind.price(i);

    bool entered = is_support ? (prev_close < lower && curr_close >= lower &&
                                 curr_close <= upper)
                              : (prev_close > upper && curr_close <= upper &&
                                 curr_close >= lower);

    if (!entered)
      continue;

    bool clean_exit = false;
    bool broken = false;
    size_t j = 0;
    for (; j < max_inside; ++j) {
      auto idx = i + j;
      double close = ind.price(idx);
      double low = ind.low(idx);
      double high = ind.high(idx);

      if (is_support && low < lower) {
        broken = true;
        break;
      }
      if (!is_support && high > upper) {
        broken = true;
        break;
      }

      if (is_support && close > upper) {
        clean_exit = true;
        break;
      }
      if (!is_support && close < lower) {
        clean_exit = true;
        break;
      }
    }

    if (clean_exit && !broken) {
      count++;
      i += j + 1;
    }
  }

  return count;
}

// FIXME fix the clustering code
template <bool is_support>
auto cluster_into_zones(auto& ind, auto& points, double percent_width) {
  std::vector<Zone> zones;
  if (points.empty())
    return zones;

  auto add_zone = [&](auto lo, auto hi) {
    auto mid = (lo + hi) / 2;
    lo = mid * (1 - percent_width / 2);
    hi = mid * (1 + percent_width / 2);
    auto conf = count_bounces<is_support>(ind, lo, hi);
    zones.emplace_back(lo, hi, conf);
  };

  std::vector<double> sorted = points;
  std::sort(sorted.begin(), sorted.end());

  double start = sorted[0];
  double end = sorted[0];

  for (size_t i = 1; i < sorted.size(); ++i) {
    if ((sorted[i] - end) / end <= percent_width) {
      end = sorted[i];
    } else {
      add_zone(start, end);
      start = end = sorted[i];
    }
  }

  add_zone(start, end);
  return zones;
}

inline auto find_support_zones(auto& ind, int lookback, int window = 3) {
  std::vector<double> swing_lows;

  for (int i = -lookback; i < -window - 1; ++i) {
    if (is_swing_low(ind, i, window))
      swing_lows.push_back(ind.low(i));
  }

  return cluster_into_zones<true>(ind, swing_lows, 0.02);  // 0.5% zone width
}

inline auto find_resistance_zones(auto& ind, int lookback, int window = 3) {
  std::vector<double> swing_highs;

  for (int i = -lookback; i < -window - 1; ++i) {
    if (is_swing_high(ind, i, window))
      swing_highs.push_back(ind.high(i));
  }

  return cluster_into_zones<false>(ind, swing_highs, 0.005);  // 0.5% zone width
}

inline auto find_ema_bounces(auto& ind) {
  std::vector<double> bounces;
  for (size_t i = 1; i < ind.candles.size(); ++i) {
    if (ind.price(i - 1) < ind.ema21(i - 1) && ind.price(i) > ind.ema21(i))
      bounces.push_back(ind.low(i));
  }
  return bounces;
}

inline auto find_ema_rejections(auto& ind) {
  std::vector<double> rejections;
  for (size_t i = 1; i < ind.candles.size(); ++i) {
    if (ind.price(i - 1) > ind.ema21(i - 1) && ind.price(i) < ind.ema21(i))
      rejections.push_back(ind.low(i));
  }
  return rejections;
}

inline auto confirm_levels(auto& lv1, auto& lv2, double tolerance_pct = 0.3) {
  std::set<double> confirmed;
  for (auto a : lv1) {
    for (auto b : lv2) {
      auto diff_pct = std::abs(a - b) / std::max(std::abs(a), std::abs(b));
      if (diff_pct < PERCENT(tolerance_pct))
        confirmed.insert((a + b) / 2.0);
    }
  }
  return std::vector<double>(confirmed.begin(), confirmed.end());
}

Support::Support(const Indicators& ind, int lookback) noexcept
    : zones{find_support_zones(ind, lookback)} {}

bool Support::is_near(double price) const {
  for (const auto& zone : zones) {
    if (zone.contains(price) && zone.confidence >= 2)
      return true;
  }
  return false;
}

Resistance::Resistance(const Indicators& ind, int lookback) noexcept
    : zones{find_resistance_zones(ind, lookback)} {}

bool Resistance::is_near(double price) const {
  for (const auto& zone : zones) {
    if (zone.contains(price) && zone.confidence >= 2)
      return true;
  }
  return false;
}
