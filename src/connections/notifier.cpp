#include "notifier.h"
#include "api.h"
#include "config.h"
#include "format.h"
#include "portfolio.h"
#include "times.h"

#include <spdlog/spdlog.h>

#include <sys/wait.h>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef POSITIONS_FILE
#define POSITIONS_FILE ""
#endif

template <Commands command>
inline void Notifier::handle_command(std::istream& is) {
  auto& tg = portfolio.tg;

  std::string symbol;
  is >> symbol;

  if constexpr (command == Commands::PING) {
    tg.send(std::format("```json\nping: \"{}\"\nlast updated: \"{}\"```",  //
                        now_ny_time(), portfolio.last_updated));
  }

  if constexpr (command == Commands::STATUS) {
    std::string str;
    {
      auto _ = portfolio.reader_lock();

      if (symbol == "")
        str = to_str<FormatTarget::Telegram>(portfolio);
      else if (auto ticker = portfolio.get_ticker(symbol); ticker != nullptr)
        str = to_str<FormatTarget::Telegram>(*ticker);
    }

    std::string load =
        symbol == "" ? std::format("{}\n\n{}",  //
                                   tunnel_url,
                                   to_str<FormatTarget::Telegram>(HASKELL, str))
                     : std::format("{}/{}.html\n\n{}",  //
                                   tunnel_url, symbol,
                                   to_str<FormatTarget::Telegram>(ELIXIR, str));

    tg.send(load);
  }

  if constexpr (command == Commands::TRADES) {
    auto& mut_portfolio = const_cast<Portfolio&>(portfolio);
    mut_portfolio.update_trades();
    tg.send(tunnel_url + "/trades.html");
  }

  if constexpr (command == Commands::POSITIONS) {
    std::string str;
    {
      auto _ = portfolio.reader_lock();
      str =
          to_str<FormatTarget::Telegram>(portfolio.get_positions(), portfolio);
    }
    tg.send(to_str<FormatTarget::Telegram>(HASKELL, str));
  }

  else {
    std::string qty_str, px_str, total_str;
    is >> qty_str >> px_str >> total_str;

    if (!portfolio.is_tracking(symbol)) {
      spdlog::error("[tg] buy/sell for untracked symbol: {}", symbol.c_str());
      return;
    }

    if (qty_str == "" || px_str == "" || total_str == "") {
      spdlog::error("[tg] empty qty, px or total string");
      return;
    }

    try {
      Trade trade{to_str(now_ny_time()),
                  symbol,
                  command == Commands::BUY ? Action::BUY : Action::SELL,
                  std::stod(qty_str),
                  std::stod(px_str),
                  std::stod(total_str)};
      auto pos_pnl = portfolio.add_trade(trade);
      auto str = to_str<FormatTarget::Telegram>(trade, pos_pnl);
      auto msg = to_str<FormatTarget::Telegram>(DIFF, str);
      tg.send(msg);

      std::ofstream file(POSITIONS_FILE, std::ios::app);
      if (file)
        file << std::format("{},{},{},{},{},{}\n",                      //
                            to_str(now_ny_time()), symbol,              //
                            command == Commands::BUY ? "BUY" : "SELL",  //
                            qty_str, px_str, total_str);
    } catch (const std::invalid_argument& e) {
      spdlog::error("[tg] invalid argument to buy/sell: {} {} @ {}, {}",
                    symbol.c_str(), qty_str.c_str(), px_str.c_str(),
                    total_str.c_str());
    } catch (...) {
      spdlog::error("[tg] wrong format for buy/sell");
    }
  }
}

inline void Notifier::handle_command(const std::string& line) {
  std::istringstream is{line};

  std::string command;
  is >> command;

  if (command == "/buy")
    return handle_command<Commands::BUY>(is);

  if (command == "/sell")
    return handle_command<Commands::SELL>(is);

  if (command == "/trades")
    return handle_command<Commands::TRADES>(is);

  if (command == "/status")
    return handle_command<Commands::STATUS>(is);

  if (command == "/ping")
    return handle_command<Commands::PING>(is);

  if (command == "/positions")
    return handle_command<Commands::POSITIONS>(is);

  spdlog::error("[tg] wrong command: {}", line.c_str());
}

void Notifier::iter(Notifier* notifier) {
  auto& portfolio = notifier->portfolio;

  auto last_update_id = 0;
  while (true) {
    std::string alert, status;
    {
      auto _ = portfolio.reader_lock();
      if (notifier->last_updated != portfolio.last_updated) {
        auto& prev_signals = notifier->prev_signals;

        auto diff = to_str<FormatTarget::Alert>(portfolio, prev_signals);
        alert = to_str<FormatTarget::Telegram>(DIFF, diff);

        prev_signals.clear();
        for (auto& [symbol, ticker] : portfolio.tickers)
          prev_signals.emplace(symbol, ticker.metrics.ind_1h.signal);

        auto str = to_str<FormatTarget::Telegram>(portfolio);
        status = to_str<FormatTarget::Telegram>(HASKELL, str);

        notifier->last_updated = portfolio.last_updated;
      }
    }
    if (alert != "")
      notifier->tg.send(status + '\n' + alert);

    auto [valid, line, id] = notifier->tg.receive(last_update_id);
    if (valid)
      notifier->handle_command(line);
    last_update_id = id;
    std::this_thread::sleep_for(seconds(5));
  }
}

pid_t tunnel_pid = -1;  // To track the child process

inline std::string start_tunnel() {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    spdlog::error("[tunnel] pipe creation failed");
    return "";
  }

  tunnel_pid = fork();
  if (tunnel_pid == -1) {
    spdlog::error("[tunnel] fork failed");
    return "";
  }

  if (tunnel_pid == 0) {
    // Child process
    // close(pipefd[0]);                // Close read end
    dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
    close(pipefd[1]);

    execlp("python3", "python3", "scripts/start_tunnel.py", nullptr);
    _exit(1);  // If exec fails
  }

  // Parent process
  // close(pipefd[1]);  // Close write end
  std::array<char, 256> buffer;
  std::string result;

  ssize_t len = read(pipefd[0], buffer.data(), buffer.size());
  if (len > 0) {
    result.assign(buffer.data(), len);
    result.erase(result.find_last_not_of("\r\n") + 1);
    spdlog::info("[tunnel] tunnel url: {}", result.c_str());
  } else {
    spdlog::error("[tunnel] no output received tunnel script.");
  }
  close(pipefd[0]);

  return result;
}

inline void cleanup() {
  if (tunnel_pid <= 0)
    return;
  spdlog::info("[tunnel] kill tunnel pid {}", tunnel_pid);
  kill(tunnel_pid, SIGTERM);
  waitpid(tunnel_pid, nullptr, 0);
}

Notifier::Notifier(const Portfolio& portfolio)
    : portfolio{portfolio},
      tg{portfolio.tg},
      last_updated{portfolio.last_updated}  //
{
  for (auto& [symbol, ticker] : portfolio.tickers)
    prev_signals.emplace(symbol, ticker.metrics.ind_1h.signal);

  auto str = to_str<FormatTarget::Telegram>(portfolio);
  auto msg = to_str<FormatTarget::Telegram>(HASKELL, str);
  tg.send(msg);

  signal(SIGINT, [](int) {
    cleanup();
    exit(0);
  });
  signal(SIGTERM, [](int) {
    cleanup();
    exit(0);
  });

  tunnel_url = start_tunnel();
  auto msg_id = tg.send(tunnel_url);
  tg.pin_message(msg_id);

  td = std::thread{Notifier::iter, this};
}

Notifier::~Notifier() {
  if (td.joinable())
    td.join();
  cleanup();
}
