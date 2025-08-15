#include "core/portfolio.h"

#include <glaze/glaze.hpp>

Symbols::Symbols() noexcept {
  std::ifstream file("private/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string push_back, symbol, tier_str, sector;

    if (std::getline(ss, push_back, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str, ',') && std::getline(ss, sector, ',')) {
      if (push_back == "+")
        arr.emplace_back(symbol, std::stoi(tier_str));
    }
  }
}
