#pragma once

#include "config.h"

#include <vector>

struct Indicators;

struct Zone {
  double lo;
  double hi;
  double confidence = 0.0;

  bool contains(double price) const { return price >= lo && price <= hi; }
};

enum class SR {
  Support,
  Resistance,
};

template <SR sr>
struct SupportResistance {
  std::vector<Zone> zones;
  SupportResistance(const Indicators& m,
                    const SupportResistanceConfig& config) noexcept;
  bool is_near(double price) const;
};

template struct SupportResistance<SR::Support>;
template struct SupportResistance<SR::Resistance>;
