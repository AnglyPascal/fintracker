#include "positions.h"

#include "format.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef POSITIONS_FILE
#define POSITIONS_FILE ""
#endif

Position operator-(const Position& lhs, const Position& rhs) {
  auto diff_qty = lhs.qty - rhs.qty;
  auto diff_cost = lhs.total - rhs.total;
  auto diff_px = diff_qty != 0 ? std::round(double(diff_cost) / diff_qty) : 0;
  return {diff_qty, diff_px, diff_cost, 0.0};
}

double Position::pnl(double price) const {
  return (price - px) * qty;
}

double Position::pct(double price) const {
  auto cost = px * qty;
  return (cost != 0) ? pnl(price) / std::abs(cost) * 100 : 0.0;
}

inline auto net_position(const std::vector<Trade>& trades) {
  double qty = 0;
  double total = 0;
  double cost = 0;
  double pnl = 0;

  for (auto& t : trades) {
    int mult = t.action == Action::BUY ? 1 : -1;
    qty += mult * t.qty;
    total += mult * t.total;
    cost += mult * t.qty * t.px;

    if (qty == 0) {
      pnl -= total;
      total = 0;
      cost = 0;
    }
  }

  auto px = cost / qty;
  return std::make_pair(Position{qty, px, total, 0.0}, pnl);
}

OpenPositions::OpenPositions() noexcept {
  std::system("python3 scripts/clean_trades.py");

  std::ifstream file(POSITIONS_FILE);
  if (!file.is_open()) {
    std::cerr << "Could not open positions file.\n";
    return;
  }

  std::string line;
  getline(file, line);  // skip header

  while (getline(file, line)) {
    std::istringstream ss(line);
    std::string date, ticker, action_str, qty_str, price_str, total_str;

    getline(ss, date, ',');
    getline(ss, ticker, ',');
    getline(ss, action_str, ',');
    getline(ss, qty_str, ',');
    getline(ss, price_str, ',');
    getline(ss, total_str, ',');

    auto action = action_str == "BUY" ? Action::BUY : Action::SELL;
    auto qty = std::stod(qty_str);
    auto px = std::stod(price_str);
    auto total = std::stod(total_str);

    trades_by_ticker[ticker].emplace_back(date, ticker, action, qty, px, total);
  }

  for (const auto& [ticker, trades] : trades_by_ticker) {
    auto [pos, pnl] = net_position(trades);
    if (std::round(pos.total) > 0)
      positions.try_emplace(ticker, pos);
    else
      total_pnl += pnl;
  }
}

double OpenPositions::add_trade(const Trade& trade) {
  auto& symbol = trade.ticker;
  trades_by_ticker[symbol].emplace_back(trade);

  auto [net_pos, pnl] = net_position(trades_by_ticker.at(symbol));

  if (net_pos.qty == 0) {
    if (auto it = positions.find(symbol); it != positions.end())
      pnl = trade.total - it->second.total;
    positions.erase(symbol);
  } else {
    positions[symbol] = net_pos;
  }

  return pnl;
}

const Position* OpenPositions::get_position(const std::string& symbol) const {
  auto it = positions.find(symbol);
  return it == positions.end() ? nullptr : &it->second;
}

