#pragma once

#include "util/times.h"

#include <optional>
#include <vector>

struct Indicators;

struct Interval {
  size_t l, r;

  bool operator==(const Interval& other) const {
    return l == other.l && r == other.r;
  }
  auto operator<=>(const Interval& other) const = default;
};

struct SwingPoint {
  LocalTimePoint tp;
  double price;

  bool operator==(const SwingPoint& other) const {
    return tp == other.tp && price == other.price;
  }
  auto operator<=>(const SwingPoint& other) const = default;
};

struct Zone {
  double lo;
  double hi;

  double confidence = 0.5;
  std::vector<Interval> hits;
  std::vector<SwingPoint> sps;

  bool contains(double price) const { return price >= lo && price <= hi; }
  double distance(double price) const;
  bool is_near(double price) const;
  bool is_strong() const;
};

enum class SR {
  Support,
  Resistance,
};

template <SR sr>
struct SupportResistance {
  std::vector<Zone> zones;
  SupportResistance(const Indicators& m) noexcept;

  std::optional<std::reference_wrapper<const Zone>> nearest(double price) const;
};

template struct SupportResistance<SR::Support>;
template struct SupportResistance<SR::Resistance>;

using Support = SupportResistance<SR::Support>;
using Resistance = SupportResistance<SR::Resistance>;
