#include "util/symbols.h"

#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

Symbols::Symbols() noexcept : spy{true, "SPY", 4, "benchmark etf"} {
  std::ifstream file("private/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string add, symbol, tier_str, sector;

    if (std::getline(ss, add, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str, ',') && std::getline(ss, sector, ',')) {
      auto ch = add[0];
      arr.emplace_back(ch, symbol, std::stoi(tier_str), sector);
    }
  }

  spdlog::info("[init] {} symbols", arr.size());
}
