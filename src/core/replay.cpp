#include "core/replay.h"
#include "concurrency/api.h"
#include "core/portfolio.h"
#include "util/config.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void write_candles(const std::string& filename, const CandleStore& data);
CandleStore read_candles(const std::string& filename);

Replay::Replay(TD& td, const Symbols& symbols) noexcept
    : td{td}, calls_per_hour{static_cast<size_t>(H_1 / td.interval)} {
  if (!config.replay_en)
    return;

  auto candles_fname = "data/replay_candles.bin";

  if (!config.replay_clear && fs::exists(candles_fname)) {
    candles_by_sym = read_candles(candles_fname);
    if (candles_by_sym.empty())
      spdlog::error("[replay] error reading from {}", candles_fname);
    else
      spdlog::info("[replay] read from file");
    return;
  }

  for (auto& [symbol, _] : symbols) {
    auto candles = td.time_series(symbol, H_1);
    if (candles.empty()) {
      spdlog::error("[replay] no candles fetched for {}", symbol.c_str());
      continue;
    }

    auto date = candles.back().day();
    auto idx = candles.size() - 1;
    while (candles[idx].day() == date)
      idx--;
    idx++;

    candles_by_sym.try_emplace(symbol, std::move(candles), idx);
  }

  write_candles(candles_fname, candles_by_sym);
  spdlog::info("[replay] constructed with intervals {}", calls_per_hour);
}

TimeSeriesRes Replay::time_series(const std::string& symbol, minutes) noexcept {
  auto it = candles_by_sym.find(symbol);
  if (it == candles_by_sym.end()) {
    spdlog::warn("[replay] no time series for {}", symbol.c_str());
    return {};
  }

  auto& [candles, idx] = it->second;
  constexpr size_t min_sz = 300;
  if (candles.size() < min_sz) {
    spdlog::warn("[replay] time series not long enough for {}", symbol.c_str());
    return {};
  }

  return TimeSeriesRes{candles.begin(), candles.begin() + idx};
}

RealTimeRes Replay::real_time(const std::string& symbol, minutes) noexcept {
  auto it = candles_by_sym.find(symbol);
  if (it == candles_by_sym.end()) {
    spdlog::warn("[replay] no time series for {}", symbol.c_str());
    return {};
  }
  auto& [candles, idx] = it->second;
  return {candles[idx - 1], candles[idx]};
}

void Replay::roll_fwd() noexcept {
  if (!config.replay_en)
    return;
  if (++n_ticks < calls_per_hour)
    return;
  n_ticks = 0;
  for (auto& [_, tl] : candles_by_sym)
    tl.idx++;
}

// FIXME fix
void Replay::rollback(const std::string& symbol) {
  auto it = candles_by_sym.find(symbol);
  if (it == candles_by_sym.end()) {
    spdlog::warn("[replay] no time series for {}", symbol.c_str());
    return;
  }

  // if (time_idx == 0) {
  //   time_idx = calls_per_hour - 1;
  //   it->second.idx--;
  // }
}

bool Replay::has_data() const {
  if (candles_by_sym.empty())
    return false;
  auto& tl = candles_by_sym.begin()->second;
  return tl.idx < tl.candles.size();
}
