#include "positions.h"
#include "api.h"     // FIXME: remove this, move it to notifier
#include "format.h"  // same

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef POSITIONS_FILE
#define POSITIONS_FILE ""
#endif

Position operator-(const Position& lhs, const Position& rhs) {
  auto diff_qty = lhs.qty - rhs.qty;
  auto diff_cost = lhs.cost - rhs.cost;
  auto diff_px = diff_qty != 0 ? std::round(double(diff_cost) / diff_qty) : 0;
  return {diff_qty, diff_px, diff_cost, 0.0};
}

double Position::pnl(double price) const {
  return price * qty - cost;
}

double Position::pct(double price) const {
  return (cost != 0) ? pnl(price) / std::abs(cost) * 100 : 0.0;
}

inline std::pair<Position, int> position_from_trades(
    const std::vector<Trade>& trades) {
  double net_qty = 0;
  double total_cost = 0;
  double pnl = 0;

  for (const Trade& t : trades) {
    int mult = t.action == Action::BUY ? 1 : -1;
    net_qty += mult * t.qty;
    total_cost += t.fees + mult * t.qty * t.px;

    if (net_qty == 0) {
      pnl -= total_cost;
      total_cost = 0;
    }
  }

  auto px = total_cost / net_qty;
  return {Position{net_qty, px, total_cost, 0.0}, pnl};
}

OpenPositions::OpenPositions() noexcept {
  std::ifstream file(POSITIONS_FILE);
  if (!file.is_open()) {
    std::cerr << "Could not open positions file.\n";
    return;
  }

  std::string line;
  getline(file, line);  // skip header

  while (getline(file, line)) {
    std::istringstream ss(line);
    std::string date, ticker, action_str, qty_str, price_str, fees_str;

    getline(ss, date, ',');
    getline(ss, ticker, ',');
    getline(ss, action_str, ',');
    getline(ss, qty_str, ',');
    getline(ss, price_str, ',');
    getline(ss, fees_str, ',');

    auto action = action_str == "BUY" ? Action::BUY : Action::SELL;
    auto qty = std::stod(qty_str);
    auto px = std::stod(price_str);
    auto fees = std::stod(fees_str);

    trades_by_ticker[ticker].emplace_back(date, ticker, action, qty, px, fees);
  }

  for (const auto& [ticker, trades] : trades_by_ticker) {
    auto [pos, pnl] = position_from_trades(trades);
    if (pos.qty > 0)
      positions.try_emplace(ticker, pos);
  }
}

void OpenPositions::add_trade(const Trade& trade) {
  auto& ticker = trade.ticker;
  trades_by_ticker[ticker].emplace_back(trade);

  auto [net_pos, pnl] = position_from_trades(trades_by_ticker.at(ticker));
  if (net_pos.qty > 0)
    positions[ticker] = net_pos;

  if (trade.action == Action::BUY)
    TG::send(std::format("➕ Bought: {} {} @ {}", ticker, trade.qty, trade.px));
  else if (net_pos.qty > 0)
    TG::send(std::format("➖ Sold: {} {} @ {}", ticker, trade.qty, trade.px));
  else
    TG::send(std::format("✔️ Closed: {} {} @ {}, {}", ticker, trade.qty,
                         trade.px, pnl));
}

const Position* OpenPositions::get_position(const std::string& symbol) const {
  auto it = positions.find(symbol);
  return it == positions.end() ? nullptr : &it->second;
}

