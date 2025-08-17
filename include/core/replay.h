#pragma once

#include "ind/candle.h"
#include "mt/api.h"
#include "util/symbols.h"
#include "util/times.h"

#include <string>
#include <unordered_map>
#include <vector>

struct Candle;
struct SymbolInfo;
class TD;

struct Timeline {
  std::vector<Candle> candles;
  size_t idx = 0;
};

using CandleStore = std::unordered_map<std::string, Timeline>;

class Replay {
  TD& td;

  CandleStore candles_by_sym;

  size_t n_ticks = 0;
  const size_t calls_per_hour = 4;

 public:
  Replay(TD& td, const Symbols& symbols) noexcept;

  TimeSeriesRes time_series(const std::string& symbol,
                            minutes timeframe = H_1) noexcept;
  RealTimeRes real_time(const std::string& symbol,
                        minutes timeframe = H_1) noexcept;

  void rollback(const std::string& symbol);
  bool has_data() const;

  void roll_fwd() noexcept;
  void roll_bwd();
};
