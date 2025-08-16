#include "ind/support_resistance.h"
#include "ind/indicators.h"
#include "util/config.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

inline auto& sr_config = config.sr_config;

double Zone::distance(double price) const {
  return contains(price) ? 0.0
                         : std::min(std::abs(price - lo), std::abs(price - hi));
}

bool Zone::is_near(double price) const {
  return distance(price) < price * sr_config.zone_proximity_threshold;
}

bool Zone::is_strong() const {
  return confidence >= sr_config.strong_confidence_threshold;
}

struct Swing {
  size_t idx;
  double price;
  size_t window;
  double width;
};

template <SR sr>
inline Swing to_swing(auto& ind, size_t i) {
  constexpr bool is_support = sr == SR::Support;

  auto val = [&ind](int idx) {
    return is_support ? ind.low(idx) : ind.high(idx);
  };

  double cur = val(i);
  double atr_sum = ind.atr(i);

  auto test = [cur, val](int idx) {
    return is_support ? (val(idx) >= cur) : (val(idx) <= cur);
  };

  size_t j = 1;
  auto N = ind.size();
  while (i >= j && i + j < N && test(i - j) && test(i + j)) {
    atr_sum += ind.atr(i - j) + ind.atr(i + j);
    j++;
  }

  return {i, cur, j - 1, atr_sum / (2 * j - 1)};
}

template <SR sr>
inline Zone to_zone(auto& ind, Swing sp) {
  constexpr bool is_support = sr == SR::Support;

  Zone zone;
  zone.sps.emplace_back(ind.time(sp.idx), ind.price(sp.idx));

  auto width = sp.width;
  auto lo = zone.lo = sp.price - width / 2;
  auto hi = zone.hi = sp.price + width / 2;

  size_t max_inside = sr_config.n_candles_in_zone(ind.interval);
  size_t lookback = sr_config.n_lookback_candles(ind.interval);

  size_t N = ind.size();
  for (size_t i = N - lookback; i < N; ++i) {
    auto prev_close = ind.price(i - 1);
    auto curr_close = ind.price(i);

    auto crossed = is_support ? (prev_close > hi && curr_close <= hi)
                              : (prev_close < lo && curr_close >= lo);

    if (!crossed)
      continue;

    bool clean_exit = false;
    bool broken = false;

    size_t j = 0;
    for (; j <= std::min(N - 1, max_inside + 1); ++j) {
      auto idx = i + j;
      if (idx >= N)
        break;

      double close = ind.price(idx);
      double low = ind.low(idx);
      double high = ind.high(idx);

      if constexpr (is_support) {
        if (low < lo) {
          broken = true;
          break;
        }
        if (close > hi) {
          clean_exit = true;
          break;
        }
      } else {
        if (high > hi) {
          broken = true;
          break;
        }
        if (close < lo) {
          clean_exit = true;
          break;
        }
      }
    }

    if (j != 0 && clean_exit && !broken) {
      zone.hits.emplace_back(i, i + j - 1);
      i += j + 1;
    }
  }

  return zone;
}

inline auto merge_intervals(auto& raw_invs) {
  if (raw_invs.empty())
    return raw_invs;

  std::sort(raw_invs.begin(), raw_invs.end(), [](auto& l, auto& r) {
    return l.l == r.l ? l.r < r.r : l.l < r.l;
  });

  std::vector<Interval> invs;
  invs.push_back(raw_invs[0]);

  for (size_t i = 1; i < raw_invs.size(); i++) {
    auto inv = raw_invs[i];
    auto& prev = invs.back();
    if (inv.l <= prev.r)
      prev.r = std::max(inv.r, prev.r);
    else
      invs.push_back(inv);
  }

  return invs;
}

inline auto merge_zones(auto& raw_zones, auto max_zone_width) {
  if (raw_zones.empty())
    return raw_zones;

  std::sort(raw_zones.begin(), raw_zones.end(),
            [](auto& l, auto& r) { return l.lo < r.lo; });

  std::vector<Zone> zones;
  zones.emplace_back(std::move(raw_zones[0]));
  double n_merged = 1;

  for (size_t i = 1; i < raw_zones.size(); i++) {
    auto& next = raw_zones[i];
    auto& prev = zones.back();

    bool crossed = next.lo * (1 + 0.002) <= prev.hi;

    if (crossed) {
      prev.hi = std::max(next.hi, prev.hi);
      prev.hits.insert(prev.hits.end(), next.hits.begin(), next.hits.end());
      prev.sps.insert(prev.sps.end(), next.sps.begin(), next.sps.end());

      auto w = n_merged / (n_merged + 1);
      prev.confidence = prev.confidence * w + next.confidence * (1 - w);
      n_merged++;
    } else {
      zones.emplace_back(std::move(next));
      n_merged = 1;
    }
  }

  for (auto& zone : zones) {
    double center = (zone.lo + zone.hi) / 2;
    double current_width = (zone.hi - zone.lo) / center;

    if (current_width > max_zone_width) {
      double half_max = center * max_zone_width / 2;
      zone.lo = center - half_max;
      zone.hi = center + half_max;
    }

    zone.hits = merge_intervals(zone.hits);
    std::sort(zone.sps.begin(), zone.sps.end());

    std::set<SwingPoint> sps{zone.sps.begin(), zone.sps.end()};
    zone.sps.clear();
    zone.sps.insert(zone.sps.end(), sps.begin(), sps.end());
  }

  return zones;
}

inline constexpr auto conf_mult(auto& ind) {
  if (ind.interval == H_1)
    return 1.0;
  if (ind.interval == H_4)
    return 1.5;
  if (ind.interval == D_1)
    return 2.0;
  return 0.0;
}

inline auto calc_conf(auto& ind, auto& zone) {
  auto mid = (zone.lo + zone.hi) / 2;

  auto px = ind.price(-1);
  auto proximity_score = std::exp(-std::abs(px - mid) / px);

  double w2 = 0.0;
  size_t max_inside = sr_config.n_candles_in_zone(ind.interval);
  for (size_t i = 0; i < zone.hits.size(); i++) {
    auto [l, r] = zone.hits[i];

    // candles inside zone
    double w0 = static_cast<double>(r - l + 1) / max_inside;
    double w1 = static_cast<double>(i) / zone.hits.size();

    w2 += w0 * 0.4 + w1 * 0.6;
  }

  double bounce_score = 1 - std::exp(-w2);

  double w = 0.4;
  auto weighted = bounce_score * w + proximity_score * (1 - w);

  return weighted;
}

inline auto filter_zones(auto& raw_zones) {
  std::vector<Zone> zones;
  auto min_conf = sr_config.min_zone_confidence;
  for (auto& zone : raw_zones) {
    if (zone.confidence < min_conf)
      continue;
    if (zone.hits.empty())
      continue;
    zones.emplace_back(std::move(zone));
  }
  return zones;
}

inline auto normalize_zones(auto& zones) {
  double max_conf = zones.front().confidence;
  for (auto& zone : zones)
    zone.confidence /= max_conf;
}

template <SR sr>
inline auto find_zones(auto& ind) {
  std::vector<Swing> swings;

  auto lookback = sr_config.n_lookback_candles(ind.interval);
  auto window = static_cast<size_t>(sr_config.swing_window(ind.interval));

  for (size_t i = ind.size() - lookback; i < ind.size(); ++i) {
    auto s = to_swing<sr>(ind, i);
    if (s.window >= window)
      swings.emplace_back(std::move(s));
  }

  std::vector<Zone> zones;
  if (swings.empty())
    return zones;

  for (auto& swing : swings) {
    auto zone = to_zone<sr>(ind, swing);
    zone.confidence = calc_conf(ind, zone);
    zones.emplace_back(std::move(zone));
  }

  auto max_zone_width = sr_config.zone_width(ind.interval);
  zones = merge_zones(zones, max_zone_width);
  zones = filter_zones(zones);

  std::sort(zones.begin(), zones.end(),
            [](auto& l, auto& r) { return l.confidence > r.confidence; });

  size_t n_zones = std::min(sr_config.n_zones, zones.size());
  zones.resize(n_zones);

  normalize_zones(zones);
  return zones;
}

template <SR sr>
SupportResistance<sr>::SupportResistance(const Indicators& ind) noexcept
    : zones{find_zones<sr>(ind)} {}

template <SR sr>
ZoneOpt SupportResistance<sr>::nearest(double price, bool below) const {
  double min_dist = std::numeric_limits<double>::max();
  size_t min_idx = 0;
  for (size_t i = 0; i < zones.size(); i++) {
    auto& zone = zones[i];
    if (zone.contains(price))
      return zone;

    if (below && zone.lo > price)
      continue;
    else if (!below && zone.hi < price)
      continue;

    auto dist = zone.distance(price);
    if (dist < min_dist) {
      min_dist = dist;
      min_idx = i;
    }
  }
  if (min_dist < std::numeric_limits<double>::max())
    return zones[min_idx];
  return std::nullopt;
}
