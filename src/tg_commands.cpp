#include "tg_commands.h"

#include "api.h"
#include "indicators.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef POSITIONS_FILE
#define POSITIONS_FILE ""
#endif

inline std::string current_date() {
  return std::format("{:%Y-%m-%d}", std::chrono::system_clock::now());
}

enum class Commands {
  BUY,
  SELL,
  STATUS,
  TRADES,
  POSITIONS,
};

template <Commands command>
void handle_command(Portfolio& portfolio, std::istream& is) {
  std::string symbol;
  is >> symbol;

  if constexpr (command == Commands::STATUS) {
    portfolio.status(symbol);
  } else if constexpr (command == Commands::TRADES) {
    std::string since;
    is >> since;
    TG::send(std::format("/trades not implemented: {} {}\n", symbol, since));
  } else if constexpr (command == Commands::POSITIONS) {
    portfolio.send_current_positions(symbol);
  }
}

template <Commands command>
  requires(command == Commands::BUY || command == Commands::SELL)
void handle_command(Portfolio&, std::istream& is) {
  std::string symbol, qty_str, px_str, fees_str;
  is >> symbol >> qty_str >> px_str >> fees_str;

  if (qty_str == "" || px_str == "")
    return TG::send("Command not valid");

  double fees = fees_str != "" ? std::stod(fees_str) : 0.0;

  auto date = current_date();
  auto str = std::format("{},{},{},{:.2f},{:.2f},{:.2f}\n", date, symbol,
                         command == Commands::BUY ? "BUY" : "SELL",
                         std::stod(qty_str), std::stod(px_str), fees);

  // std::cout << str << std::endl;
  std::ofstream file(POSITIONS_FILE, std::ios::app);
  if (file)
    file << str;
}

inline void handle_command(Portfolio& portfolio, const std::string& line) {
  std::istringstream is{line};

  std::string command;
  is >> command;
  if (command == "/buy")
    handle_command<Commands::BUY>(portfolio, is);
  else if (command == "/sell")
    handle_command<Commands::SELL>(portfolio, is);
  else if (command == "/trades")
    handle_command<Commands::TRADES>(portfolio, is);
  else if (command == "/status")
    handle_command<Commands::STATUS>(portfolio, is);
  else if (command == "/positions")
    handle_command<Commands::POSITIONS>(portfolio, is);
}

bool tg_response(Portfolio& portfolio) {
  auto last_update_id = 0;
  while (true) {
    auto [valid, line, id] = TG::receive(last_update_id);
    if (valid)
      handle_command(portfolio, line);
    last_update_id = id;
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

