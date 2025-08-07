#include "support_resistance.h"
#include "indicators.h"

#include <algorithm>

template <SR sr>
inline bool is_swing(auto& ind, int i, int window = 3) {
  auto val = [&ind](int idx) {
    return sr == SR::Support ? ind.low(idx) : ind.high(idx);
  };

  double cur = val(i);

  auto test = [cur, val](int idx) {
    return sr == SR::Support ? (val(idx) <= cur) : (val(idx) >= cur);
  };

  for (int j = 1; j <= window; ++j) {
    if (test(i - j) || test(i + j))
      return false;
  }
  return true;
}

template <SR sr>
auto count_bounces(auto& ind, double lower, double upper, auto& config) {
  constexpr bool is_support = sr == SR::Support;

  double conf = 0;
  size_t max_inside = config.n_candles_in_zone(ind.interval);

  auto N = ind.candles.size();
  for (size_t i = 3; i < N; ++i) {
    double prev_close = ind.price(i - 1);
    double curr_close = ind.price(i);

    bool crossed = is_support ? (prev_close > upper && curr_close <= upper)
                              : (prev_close < lower && curr_close >= lower);

    if (!crossed)
      continue;

    bool clean_exit = false;
    bool broken = false;
    size_t j = 0;
    for (; j < max_inside; ++j) {
      auto idx = i + j;
      if (idx >= N)
        break;

      double close = ind.price(idx);
      double next_close = ind.price(idx + 1);
      double low = ind.low(idx);
      double high = ind.high(idx);

      if constexpr (is_support) {
        if (low < lower) {
          broken = true;
          break;
        }
        if (close < upper && next_close > upper) {
          clean_exit = true;
          break;
        }
      } else {
        if (high > upper) {
          broken = true;
          break;
        }
        if (close > lower && next_close < lower) {
          clean_exit = true;
          break;
        }
      }
    }

    if (clean_exit && !broken) {
      conf += (double)i * 2 / N;
      i += j + 1;
    }
  }

  return conf;
}

auto merge_zones(auto raw) {
  if (raw.empty())
    return raw;

  std::sort(raw.begin(), raw.end(),
            [](auto& l, auto& r) { return l.lo < r.lo; });

  std::vector<Zone> zones;
  zones.push_back(raw[0]);

  for (size_t i = 1; i < raw.size(); i++) {
    auto next = raw[i];
    auto& prev = zones.back();
    if (next.lo <= prev.hi) {
      prev.hi = std::max(next.hi, prev.hi);
      prev.confidence += next.confidence;
    } else {
      zones.push_back(next);
    }
  }

  return zones;
}

double get_adaptive_zone_width(auto interval) {
  if (interval == H_1)
    return 0.004;  // 0.4%
  if (interval == H_4)
    return 0.008;  // 0.8%
  if (interval == D_1)
    return 0.015;  // 1.5%
  return 0.004;
}

double get_atr_based_zone_width(auto& ind, int lookback_periods = 14) {
    double avg_atr = 0;
    int count = 0;
    
    for (int i = -1; i >= -lookback_periods; --i) {
        avg_atr += ind.atr(i);
        count++;
    }
    avg_atr /= count;
    
    double atr_multiplier = 0.5; // Adjust this based on testing
    return (avg_atr / ind.price(-1)) * atr_multiplier;
}

double confidence_multiplier(auto& ind) {
  if (ind.interval == H_1)
    return 1.0; 
  if (ind.interval == H_4) 
    return 3.0;
  if (ind.interval == D_1)
    return 6.0;
  return 0;
}

template <SR sr>
inline auto find_zones(auto& ind, auto& config) {
  std::vector<double> swings;

  auto lookback =
      config.lookback_days * ((D_1 + ind.interval - minutes{1}) / ind.interval);
  if (ind.interval == H_4)
    lookback *= 1.5;
  if (ind.interval == D_1)
    lookback *= 2.5;

  auto window = config.swing_window(ind.interval);

  for (int i = -lookback; i < -window - 1; ++i) {
    if (is_swing<sr>(ind, i, window))
      swings.push_back(ind.low(i));
  }

  std::vector<Zone> zones;
  if (swings.empty())
    return zones;

  auto width = get_atr_based_zone_width(ind, lookback);
  auto min_conf = config.min_zone_confidence;
  auto conf_mult = confidence_multiplier(ind);

  for (auto lvl : swings) {
    auto lo = lvl * (1 - width / 2);
    auto hi = lvl * (1 + width / 2);
    auto conf = count_bounces<sr>(ind, lo, hi, config);
    conf *= conf_mult;
    if (conf >= min_conf)
      zones.emplace_back(lo, hi, conf);
  }

  zones = merge_zones(zones);

  std::sort(zones.begin(), zones.end(),
            [](auto& l, auto& r) { return l.confidence > r.confidence; });

  zones.resize(std::min((size_t)config.n_zones, zones.size()));

  return zones;
}

template <SR sr>
SupportResistance<sr>::SupportResistance(
    const Indicators& ind,
    const SupportResistanceConfig& config) noexcept
    : zones{find_zones<sr>(ind, config)} {}

template <SR sr>
bool SupportResistance<sr>::is_near(double price) const {
  for (const auto& zone : zones) {
    if (zone.contains(price))
      return true;
  }
  return false;
}
