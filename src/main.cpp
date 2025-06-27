#include "api.h"
#include "portfolio.h"
#include "tg_commands.h"

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

inline auto read_symbols() {
  std::vector<SymbolInfo> symbols;
  std::ifstream file("data/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string add, symbol, tier_str;

    if (std::getline(ss, add, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str)) {
      if (add == "+")
        symbols.push_back({symbol, std::stoi(tier_str)});
    }
  }

  return symbols;
}

int main() {
  Portfolio portfolio{read_symbols()};
  std::thread t{tg_response, std::ref(portfolio)};
  portfolio.run();
  t.join();
  return 0;
}

