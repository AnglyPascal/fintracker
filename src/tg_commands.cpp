#include "tg_commands.h"

#include "api.h"
#include "format.h"
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

inline std::string current_ny_time() {
  using namespace std::chrono;
  auto now = floor<seconds>(system_clock::now());
  zoned_time ny_time{"America/New_York", now};
  return std::format("{:%Y-%m-%d %H:%M:%S}", ny_time);
}

enum class Commands {
  BUY,
  SELL,
  STATUS,
  TRADES,
  POSITIONS,
  PLOTS,
};

template <Commands command>
void handle_command(Portfolio& portfolio, std::istream& is) {
  std::string symbol;
  is >> symbol;

  if constexpr (command == Commands::STATUS) {
    portfolio.status(symbol);
  } else if constexpr (command == Commands::TRADES) {
    TG::send(to_str(portfolio.get_trades()));
  } else if constexpr (command == Commands::POSITIONS) {
    portfolio.send_current_positions(symbol);
  } else if constexpr (command == Commands::PLOTS) {
    auto fname = symbol + ".html";
    if (wait_for_file(fname))
      TG::send_doc(fname, "Charts for " + symbol);
  }
}

template <Commands command>
  requires(command == Commands::BUY || command == Commands::SELL)
void handle_command(Portfolio&, std::istream& is) {
  std::string symbol, qty_str, px_str, fees_str;
  is >> symbol >> qty_str >> px_str >> fees_str;

  if (qty_str == "" || px_str == "") {
    TG::send("Command not valid");
    return;
  }

  if (fees_str == "")
    fees_str = "0.0";

  auto str = std::format("{},{},{},{},{},{}\n",                      //
                         current_ny_time(), symbol,                  //
                         command == Commands::BUY ? "BUY" : "SELL",  //
                         qty_str, px_str, fees_str);

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
  else if (command == "/plot")
    handle_command<Commands::PLOTS>(portfolio, is);
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

