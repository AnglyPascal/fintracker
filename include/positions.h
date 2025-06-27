#pragma once

#include "indicators.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class Action {
  BUY,
  SELL,
};

inline constexpr uint32_t FLOAT_SCALE = 100;
inline constexpr uint32_t COST_SCALE = FLOAT_SCALE * FLOAT_SCALE;

struct Trade {
  std::string date;  // ISO format YYYY-MM-DD
  std::string ticker;
  Action action;  // "BUY" or "SELL"

  int qty;
  int px;
  int fees;  // actual = fees / FLOAT_SCALE^2;

  std::string to_str() const;
};

struct Position {
  int qty = 0;  // actual = qty / FLOAT_SCALE
  int px = 0;
  int cost = 0;

  double quantity() const;
  double price() const;

  double pnl(double price) const;
  double pct(double price) const;

  std::string to_str() const;
};

Position operator-(const Position& lhs, const Position& rhs);

using Trades = std::unordered_map<std::string, std::vector<Trade>>;
using Positions = std::unordered_map<std::string, Position>;

class OpenPositions {
  Trades trades_by_ticker;
  Positions positions;
  double total_pnl = 0;

  time_t positions_file_mtime = 0;
  int positions_file_line = 1;

 private:
  bool has_positions_changed();

 public:
  OpenPositions() noexcept;

  void update();
  void send_updates(const Trades& trades,
                    const Positions& old_positions,
                    const Positions& new_positions) const;
  void send_current_positions(const std::string& ticker) const;

  Position* get_position(const std::string& symbol);
  const Position* get_position(const std::string& symbol) const;

  std::string to_str(bool tg = false) const;
};
