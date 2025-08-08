#include "replay.h"
#include "api.h"
#include "portfolio.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using nlohmann::json;
namespace fs = std::filesystem;

inline void write_candles(const std::string& fname, auto& src) {
  std::ofstream f{fname};

  f << "symbol,datetime,open,high,low,close,volume\n";
  for (auto& [symbol, candles] : src) {
    for (auto& [datetime, open, high, low, close, volume] : candles) {
      f << std::format("{},{},{:.2f},{:.2f},{:.2f},{:.2f},{}\n", symbol,
                       datetime, open, high, low, close, volume);
    }
  }
}

inline void read_candles(const std::string& fname, auto& dest) {
  std::ifstream f{fname};
  std::string line;

  std::getline(f, line);

  while (std::getline(f, line)) {
    std::istringstream ss(line);
    std::string symbol, datetime, open, high, low, close, volume;

    if (std::getline(ss, symbol, ',') && std::getline(ss, datetime, ',') &&
        std::getline(ss, open, ',') && std::getline(ss, high, ',') &&
        std::getline(ss, low, ',') && std::getline(ss, close, ',') &&
        std::getline(ss, volume, ',')) {
      dest[symbol].emplace_back(datetime, std::stod(open), std::stod(high),
                                std::stod(low), std::stod(close),
                                std::stoi(volume));
    }
  }
}

Replay::Replay(TD& td, const std::vector<SymbolInfo>& symbols, bool rp_enabled)
    : td{td}, enabled{rp_enabled} {
  if (!enabled)
    return;

  auto prev_day_fname = "data/prev_day_candles.csv";
  auto curr_day_fname = "data/curr_day_candles.csv";

  if (fs::exists(prev_day_fname) && fs::exists(curr_day_fname)) {
    read_candles(prev_day_fname, prev_day_candles);
    read_candles(curr_day_fname, curr_day_candles_rev);
    return;
  }

  for (auto& [symbol, _] : symbols) {
    auto candles = td.time_series(symbol, H_1);
    if (candles.empty())
      continue;

    std::vector<Candle> curr_day;

    auto date = candles.back().datetime.substr(0, 10);
    while (!candles.empty() && candles.back().datetime.substr(0, 10) == date) {
      curr_day.push_back(candles.back());
      candles.pop_back();
    }

    prev_day_candles.try_emplace(symbol, std::move(candles));
    curr_day_candles_rev.try_emplace(symbol, std::move(curr_day));
  }

  write_candles(prev_day_fname, prev_day_candles);
  write_candles(curr_day_fname, curr_day_candles_rev);
}

std::vector<Candle> Replay::time_series(const std::string& symbol, minutes) {
  auto it = prev_day_candles.find(symbol);
  if (it == prev_day_candles.end()) {
    spdlog::warn("[replay] no time series for {}", symbol.c_str());
    return {};
  }

  size_t min_sz = 300;
  if (it->second.size() < min_sz) {
    spdlog::warn("[replay] no time series for {}", symbol.c_str());
    return {};
  }

  auto vec = std::move(it->second);
  prev_day_candles.erase(symbol);
  return vec;
}

Candle Replay::real_time(const std::string& symbol, minutes) {
  auto it = curr_day_candles_rev.find(symbol);
  if (it == curr_day_candles_rev.end() || it->second.empty()) {
    spdlog::warn("[replay] no candle for {}", symbol.c_str());
    return {};
  }

  auto candle = it->second.back();
  it->second.pop_back();
  return candle;
}

void Replay::rollback(const std::string& symbol, const Candle& candle) {
  curr_day_candles_rev[symbol].push_back(candle);
}

bool Replay::has_data() const {
  return !curr_day_candles_rev.empty();
}
