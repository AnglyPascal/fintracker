#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct Candle;
struct SymbolInfo;
class TD;

class Replay {
  TD& td;

  std::unordered_map<std::string, std::vector<Candle>> prev_day_candles;
  std::unordered_map<std::string, std::vector<Candle>> curr_day_candles_rev;

 public:
  const bool enabled = false;

 public:
  Replay(TD& td,
           const std::vector<SymbolInfo>& symbols,
           bool bt_enabled = false);

  std::vector<Candle> time_series(const std::string& symbol, int n_days = 90);
  Candle real_time(const std::string& symbol);

  void rollback(const std::string& symbol, const Candle& candle);

  bool has_data() const;
};
