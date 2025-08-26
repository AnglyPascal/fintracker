#include "core/replay.h"
#include "mt/sleeper.h"
#include "mt/thread_pool.h"
#include "util/config.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace fs = std::filesystem;

void write_candles(const std::string& filename, const CandleStore& data);
CandleStore read_candles(const std::string& filename);

Replay::Replay(TD& td, const Symbols& symbols) noexcept
    : td{td},
      calls_per_hour{static_cast<size_t>(H_1 / td.interval)}  //
{
  if (!config.replay_en)
    return;

  auto candles_fname = "private/replay_candles.bin";

  if (!config.replay_clear && fs::exists(candles_fname)) {
    candles_by_sym = read_candles(candles_fname);
    if (candles_by_sym.empty())
      spdlog::error("[replay] error reading from {}", candles_fname);
    else
      spdlog::info("[replay] read from file");
    return;
  }

  std::mutex mtx;

  auto func = [&, this](SymbolInfo&& si, minutes interval = H_1) {
    if (sleeper.should_shutdown())
      return false;

    auto& symbol = si.symbol;

    auto candles = td.time_series(symbol, interval);
    if (candles.empty()) {
      spdlog::error("[init] ({}) no candles", symbol.c_str());
      return true;
    }

    auto date = candles.back().day();
    auto idx = candles.size() - 1;
    while (candles[idx].day() == date)
      idx--;
    idx++;

    {
      std::lock_guard _{mtx};
      candles_by_sym.try_emplace(symbol, std::move(candles), idx);
    }

    spdlog::info("[replay] fetched ({})", symbol.c_str());

    return true;
  };

  {
    thread_pool<SymbolInfo> pool{config.n_concurrency, func, symbols.arr};
    func(SymbolInfo{symbols.spy}, D_1);
  }

  write_candles(candles_fname, candles_by_sym);
  spdlog::info("[replay] constructed with intervals {}", calls_per_hour);
}

TimeSeriesRes Replay::time_series(const std::string& symbol,
                                  minutes) noexcept  //
{
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
  for (auto& [_, tl] : candles_by_sym) {
    tl.idx++;
  }
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
  //   it->second.ids--;
  // }
}

bool Replay::has_data() const {
  if (candles_by_sym.empty())
    return false;
  auto& tl = candles_by_sym.begin()->second;
  return tl.idx < tl.candles.size();
}
