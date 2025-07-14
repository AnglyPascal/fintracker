#include "notifier.h"
#include "api.h"
#include "format.h"
#include "portfolio.h"
#include "times.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef POSITIONS_FILE
#define POSITIONS_FILE ""
#endif

enum class Commands {
  BUY,
  SELL,
  STATUS,
  TRADES,
  POSITIONS,
  PLOT,
};

template <Commands command>
void handle_command(const Portfolio& portfolio, std::istream& is) {
  auto& tg = portfolio.tg;

  std::string symbol;
  is >> symbol;

  if constexpr (command == Commands::STATUS) {
    std::string str;
    {
      auto lock = portfolio.reader_lock();

      if (symbol == "")
        str = to_str<FormatTarget::Telegram>(portfolio);
      else if (auto ticker = portfolio.get_ticker(symbol); ticker != nullptr)
        str = to_str<FormatTarget::Telegram>(*ticker);
    }

    if (symbol == "")
      tg.send(to_str<FormatTarget::Telegram>(HASKELL, str));
    else
      tg.send(to_str<FormatTarget::Telegram>(TEXT, str));
  }

  else if constexpr (command == Commands::TRADES) {
    // FIXME: need to send an html, not the text
    std::string str;
    {
      auto lock = portfolio.reader_lock();
      str = to_str<FormatTarget::HTML>(portfolio.get_trades());
    }
    tg.send(str);
  }

  else if constexpr (command == Commands::POSITIONS) {
    std::string str;
    {
      auto lock = portfolio.reader_lock();
      str =
          to_str<FormatTarget::Telegram>(portfolio.get_positions(), portfolio);
    }
    tg.send(to_str<FormatTarget::Telegram>(HASKELL, str));
  }

  else if constexpr (command == Commands::PLOT) {
    auto fname = std::format("page/{}.html", symbol);
    if (wait_for_file(fname))
      tg.send_doc(fname, "Charts for " + symbol);
  }
}

template <Commands command>
  requires(command == Commands::BUY || command == Commands::SELL)
void handle_command(const Portfolio& portfolio, std::istream& is) {
  auto& tg = portfolio.tg;

  std::string symbol, qty_str, px_str, fees_str;
  is >> symbol >> qty_str >> px_str >> fees_str;

  if (!portfolio.is_tracking(symbol)) {
    tg.send("Symbol is not being tracked");
    return;
  }

  if (qty_str == "" || px_str == "") {
    tg.send("Command not valid");
    return;
  }

  if (fees_str == "")
    fees_str = "0.0";

  Trade trade{to_str(now_ny_time()),
              symbol,
              command == Commands::BUY ? Action::BUY : Action::SELL,
              std::stod(qty_str),
              std::stod(px_str),
              std::stod(fees_str)};

  auto pos_pnl = portfolio.add_trade(trade);
  auto str = to_str<FormatTarget::Telegram>(trade, pos_pnl);
  auto msg = to_str<FormatTarget::Telegram>(DIFF, str);
  tg.send(msg);

  std::ofstream file(POSITIONS_FILE, std::ios::app);
  if (file)
    file << std::format("{},{},{},{},{},{}\n",                      //
                        to_str(now_ny_time()), symbol,              //
                        command == Commands::BUY ? "BUY" : "SELL",  //
                        qty_str, px_str, fees_str);
}

inline void handle_command(const Portfolio& portfolio,
                           const std::string& line) {
  std::istringstream is{line};

  std::string command;
  is >> command;

  if (command == "/buy")
    return handle_command<Commands::BUY>(portfolio, is);

  if (command == "/sell")
    return handle_command<Commands::SELL>(portfolio, is);

  if (command == "/trades")
    return handle_command<Commands::TRADES>(portfolio, is);

  if (command == "/status")
    return handle_command<Commands::STATUS>(portfolio, is);

  if (command == "/positions")
    return handle_command<Commands::POSITIONS>(portfolio, is);

  if (command == "/plot")
    return handle_command<Commands::PLOT>(portfolio, is);
}

void Notifier::iter(Notifier* notifier) {
  auto& portfolio = notifier->portfolio;

  auto last_update_id = 0;
  while (true) {
    std::string msg;
    {
      auto lock = portfolio.reader_lock();
      if (notifier->last_updated != portfolio.last_updated()) {
        auto& prev_signals = notifier->prev_signals;
        auto diff = to_str<FormatTarget::Alert>(portfolio, prev_signals);
        msg = to_str<FormatTarget::Telegram>(DIFF, diff);

        prev_signals.clear();
        for (auto& [symbol, ticker] : portfolio.tickers)
          prev_signals.emplace(symbol, ticker.signal);

        notifier->last_updated = portfolio.last_updated();
      }
    }
    if (msg != "")
      notifier->tg.send(msg);

    auto [valid, line, id] = notifier->tg.receive(last_update_id);
    if (valid)
      handle_command(portfolio, line);
    last_update_id = id;
    std::this_thread::sleep_for(seconds(5));
  }
}

Notifier::Notifier(const Portfolio& portfolio) noexcept
    : portfolio{portfolio},
      tg{portfolio.tg},
      last_updated{portfolio.last_updated()}  //
{
  for (auto& [symbol, ticker] : portfolio.tickers)
    prev_signals.emplace(symbol, ticker.signal);

  auto str = to_str<FormatTarget::Telegram>(portfolio);
  auto msg = to_str<FormatTarget::Telegram>(HASKELL, str);

  if (wait_for_file("page/index.html")) {
    auto msg_id = tg.send_doc("page/index.html", "portfolio.html", "");
    if (msg_id != -1)
      tg.pin_message(msg_id);
  } else {
    tg.send("Report not yet created");
  }
  tg.send(msg);

  td = std::thread{Notifier::iter, this};
}

Notifier::~Notifier() noexcept {
  td.join();
}
