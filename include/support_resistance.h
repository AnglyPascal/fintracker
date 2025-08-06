#pragma once

#include <vector>

struct Indicators;

struct Zone {
  double lower_bound;
  double upper_bound;
  int confidence = 0;

  bool contains(double price) const {
    return price >= lower_bound && price <= upper_bound;
  }
};

struct Support {
  std::vector<Zone> zones;
  Support(const Indicators& m, int lookback = 250) noexcept;
  bool is_near(double price) const;
};

struct Resistance {
  std::vector<Zone> zones;
  Resistance(const Indicators& m, int lookback = 250) noexcept;
  bool is_near(double price) const;
};
