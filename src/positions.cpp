#include "positions.h"
#include "api.h"

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
  int diff_px = diff_qty != 0 ? std::round(double(diff_cost) / diff_qty) : 0;
  return {diff_qty, diff_px, diff_cost};
}

double Position::pnl(double price) const {
  auto value = price * qty / FLOAT_SCALE;
  return value - (double)(cost) / COST_SCALE;
}

double Position::pct(double price) const {
  auto abs_cost = std::abs((double)(cost)) / COST_SCALE;
  return (cost != 0) ? pnl(price) / abs_cost * 100.0 : 0.0;
}

double Position::price() const {
  return (double)px / FLOAT_SCALE;
}

OpenPositions::OpenPositions() noexcept {
  update();
}

bool OpenPositions::has_positions_changed() {
  if (!std::filesystem::exists(POSITIONS_FILE))
    return false;

  auto ftime = std::filesystem::last_write_time(POSITIONS_FILE);
  auto systime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
  auto cftime = std::chrono::system_clock::to_time_t(systime);

  bool changed = cftime != positions_file_mtime;
  positions_file_mtime = cftime;
  return changed;
}

inline std::pair<Position, int> position_from_trades(
    const std::vector<Trade>& trades) {
  int net_qty = 0;
  int total_cost = 0;
  int pnl = 0;

  for (const Trade& t : trades) {
    int mult = t.action == Action::BUY ? 1 : -1;
    net_qty += mult * t.qty;
    total_cost += t.fees + mult * t.qty * t.px;

    if (net_qty == 0) {
      pnl -= double(total_cost) / COST_SCALE;
      total_cost = 0;
    }
  }

  int px = std::round(double(total_cost) / net_qty);
  return {Position{net_qty, px, total_cost}, pnl};
}

void OpenPositions::update() {
  if (!has_positions_changed())
    return;

  std::ifstream file(POSITIONS_FILE);
  if (!file.is_open()) {
    std::cerr << "Could not open positions file.\n";
    return;
  }

  std::string line;
  for (int i = 0; i < positions_file_line; i++)
    getline(file, line);  // skip header

  bool first_run = positions_file_line == 1;
  Trades new_trades_by_ticker;

  while (getline(file, line)) {
    positions_file_line++;

    std::istringstream ss(line);
    std::string date, ticker, action_str, qty_str, price_str, fees_str;

    getline(ss, date, ',');
    getline(ss, ticker, ',');
    getline(ss, action_str, ',');
    getline(ss, qty_str, ',');
    getline(ss, price_str, ',');
    getline(ss, fees_str, ',');

    auto action = action_str == "BUY" ? Action::BUY : Action::SELL;
    int qty = std::round(std::stod(qty_str) * FLOAT_SCALE);
    int px = std::round(std::stod(price_str) * FLOAT_SCALE);
    int fees = std::round(std::stod(fees_str) * FLOAT_SCALE * FLOAT_SCALE);

    auto& trade = trades_by_ticker[ticker].emplace_back(date, ticker, action,
                                                        qty, px, fees);
    new_trades_by_ticker[ticker].emplace_back(trade);
  }

  Positions new_positions;

  for (const auto& [ticker, trades] : trades_by_ticker) {
    auto [pos, pnl] = position_from_trades(trades);
    if (pos.qty > 0)
      new_positions.try_emplace(ticker, pos);
  }

  if (!first_run)
    send_updates(new_trades_by_ticker, positions, new_positions);
  std::swap(positions, new_positions);
}

inline std::string position_for_tg_str(const std::string& symbol,
                                       const Position& pos,
                                       double pnl) {
  auto [qty, px, cost] = pos;

  double total = (double)(cost) / COST_SCALE;
  double gain = (total != 0) ? pnl / std::abs(total) * 100.0 : 0.0;
  auto pnl_str = std::format(" | PnL: {:+.2f} ({:+.2f}%)\n", pnl, gain);

  return std::format("- {:<7}{}{}{}\n", symbol, (qty > 0 ? " +" : " "),
                     pos.to_str(), (qty > 0 ? "" : pnl_str));
}

void OpenPositions::send_updates(const Trades& new_trades,
                                 const Positions& old_positions,
                                 const Positions& new_positions) const {
  std::ostringstream msg;
  msg << "🔄 *Position Update*\n\n";

  for (auto& [symbol, trades] : new_trades) {
    auto [diff, pnl] = position_from_trades(trades);
    if (diff.qty < 0) {
      auto& pos = old_positions.at(symbol);
      if (pos.qty + diff.qty == 0)
        pnl -= diff.cost + pos.cost;
    }
    msg << position_for_tg_str(symbol, diff, (double)pnl / COST_SCALE);
  }

  if (!new_trades.empty())
    TG::send(msg.str());
}

void OpenPositions::send_current_positions(const std::string& ticker) const {
  std::ostringstream msg;
  msg << "🗿 *Open Positions*\n\n";

  if (ticker == "") {
    for (auto [symbol, position] : positions)
      msg << position_for_tg_str(symbol, position, 0);
  } else {
    auto it = positions.find(ticker);
    if (it != positions.end())
      msg << position_for_tg_str(ticker, it->second, 0);
  }

  if (!positions.empty())
    TG::send(msg.str());
}

Position* OpenPositions::get_position(const std::string& symbol) {
  auto it = positions.find(symbol);
  return it == positions.end() ? nullptr : &it->second;
}

const Position* OpenPositions::get_position(const std::string& symbol) const {
  auto it = positions.find(symbol);
  return it == positions.end() ? nullptr : &it->second;
}
