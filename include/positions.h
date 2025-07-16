#pragma once

#include "indicators.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class Action {
  BUY,
  SELL,
};

struct Trade {
  std::string date;  // ISO format YYYY-MM-DD
  std::string ticker;
  Action action;  // "BUY" or "SELL"

  double qty;
  double px;
  double total;
};

struct Position {
  double qty = 0.0;
  double px = 0.0;
  double total = 0.0;

  mutable double max_price_seen = 0.0;

  double pnl(double price) const;
  double pct(double price) const;
};

Position operator-(const Position& lhs, const Position& rhs);

using Trades = std::unordered_map<std::string, std::vector<Trade>>;
using Positions = std::unordered_map<std::string, Position>;

class TG;

class OpenPositions {
  Trades trades_by_ticker;
  Positions positions;
  double total_pnl = 0;

 public:
  OpenPositions() noexcept;
  double add_trade(const Trade& trade);

  void send_updates(const Trades& trades, const Positions& old_positions) const;
  void send_current_positions(const std::string& ticker) const;

  const Position* get_position(const std::string& symbol) const;

  const Positions& get_positions() const { return positions; };
  const Trades& get_trades() const { return trades_by_ticker; }
};
