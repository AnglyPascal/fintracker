#include "notifier.h"
#include "api.h"
#include "format.h"
#include "portfolio.h"

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
  PLOT,
};

template <Commands command>
void handle_command(const Portfolio& portfolio, std::istream& is) {
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
    TG::send(str);
  }

  else if constexpr (command == Commands::TRADES) {
    // FIXME: need to send an html, not the text
    std::string str;
    {
      auto lock = portfolio.reader_lock();
      str = to_str<FormatTarget::HTML>(portfolio.get_trades());
    }
    TG::send(str);
  }

  else if constexpr (command == Commands::POSITIONS) {
    std::string str;
    {
      auto lock = portfolio.reader_lock();
      str =
          to_str<FormatTarget::Telegram>(portfolio.get_positions(), portfolio);
    }
    TG::send(str);
  }

  else if constexpr (command == Commands::PLOT) {
    auto fname = std::format("page/{}.html", symbol);
    if (wait_for_file(fname))
      TG::send_doc(fname, "Charts for " + symbol);
  }
}

template <Commands command>
  requires(command == Commands::BUY || command == Commands::SELL)
void handle_command(const Portfolio& portfolio, std::istream& is) {
  std::string symbol, qty_str, px_str, fees_str;
  is >> symbol >> qty_str >> px_str >> fees_str;

  if (!portfolio.is_tracking(symbol)) {
    TG::send("Symbol is not being tracked");
    return;
  }

  if (qty_str == "" || px_str == "") {
    TG::send("Command not valid");
    return;
  }

  if (fees_str == "")
    fees_str = "0.0";

  Trade trade{current_ny_time(),
              symbol,
              command == Commands::BUY ? Action::BUY : Action::SELL,
              std::stod(qty_str),
              std::stod(px_str),
              std::stod(fees_str)};

  portfolio.add_trade(trade);

  auto str = std::format("{},{},{},{},{},{}\n",                      //
                         current_ny_time(), symbol,                  //
                         command == Commands::BUY ? "BUY" : "SELL",  //
                         qty_str, px_str, fees_str);

  std::ofstream file(POSITIONS_FILE, std::ios::app);
  if (file)
    file << str;
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

inline bool is_interesting(const Ticker& ticker, const Signal& prev_signal) {
  if (ticker.has_position())
    return true;
  return ticker.signal.is_interesting() || prev_signal.is_interesting();
}

void Notifier::iter(Notifier* notifier) {
  auto& portfolio = notifier->portfolio;

  auto last_update_id = 0;
  while (true) {
    std::string msg;
    {
      auto lock = portfolio.reader_lock();

      if (notifier->last_updated != portfolio.last_updated) {
        auto& prev_signals = notifier->prev_signals;

        for (auto& [symbol, ticker] : portfolio.tickers) {
          auto prev_signal = prev_signals.at(symbol);
          prev_signals.try_emplace(symbol, ticker.signal);

          if (prev_signal.type == ticker.signal.type)
            continue;

          if (!is_interesting(ticker, prev_signal))
            continue;

          msg += std::format(
              "*{}*: {}\n", symbol,
              to_str<FormatTarget::Telegram>(prev_signal, ticker.signal));
        }
        notifier->last_updated = portfolio.last_updated;
      }
    }
    if (msg != "")
      TG::send(msg);

    auto [valid, line, id] = TG::receive(last_update_id);
    if (valid)
      handle_command(portfolio, line);
    last_update_id = id;
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

std::string Notifier::init_update() const {
  auto lock = portfolio.reader_lock();

  std::string exit, caution, entry, watch;
  for (auto& [symbol, signal] : prev_signals) {
    if (signal.type == SignalType::Exit)
      exit += symbol + ", ";
    else if (signal.type == SignalType::Entry)
      entry += symbol + ", ";
    else if (signal.type == SignalType::HoldCautiously)
      caution += symbol + ", ";
    else if (signal.type == SignalType::Watchlist)
      watch += symbol + ", ";
  }

  std::string signal_str;

  if (exit != "") {
    exit.pop_back();
    exit.pop_back();
    signal_str += "🚨 Exit: " + exit + "\n\n";
  }

  if (caution != "") {
    caution.pop_back();
    caution.pop_back();
    signal_str += "⚠️ Hold cautiously: " + caution + "\n\n";
  }

  if (entry != "") {
    entry.pop_back();
    entry.pop_back();
    signal_str += "✅ Entry: " + entry + "\n\n";
  }

  if (watch != "") {
    watch.pop_back();
    watch.pop_back();
    signal_str += "👀 Watchlist: " + watch + "\n\n";
  }

  if (signal_str == "")
    signal_str = "😴 Nothing to report\n";
  signal_str += "\n";

  auto positions =
      to_str<FormatTarget::Telegram>(portfolio.get_positions(), portfolio);

  std::string header = "📊 *Status Update*\n\n";
  return header + signal_str + positions;
}

Notifier::Notifier(const Portfolio& portfolio) noexcept
    : portfolio{portfolio},
      last_updated{portfolio.last_updated}  //
{
  for (auto& [symbol, ticker] : portfolio.tickers)
    prev_signals.emplace(symbol, ticker.signal);

  auto msg = init_update();
  TG::send(msg);

  if (wait_for_file("page/index.html", 60)) {
    auto msg_id = TG::send_doc("page/index.html", "portfolio.html", "");
    if (msg_id != -1)
      TG::pin_message(msg_id);
  } else {
    TG::send("Teport not yet created");
  }

  td = std::thread{Notifier::iter, this};
}

Notifier::~Notifier() noexcept {
  td.join();
}
