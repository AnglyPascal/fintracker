#pragma once

#include <cmath>

constexpr auto sigmoid(auto x, auto c, auto x0) {
  return 1 / (1 + std::exp(-c * (x - x0)));
}

constexpr auto tanh(auto x, auto c) {
  return std::tanh(x * c);
}
