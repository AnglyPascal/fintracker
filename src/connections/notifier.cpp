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

inline void Notifier::handle_command(const Message& msg) {
  auto command = msg.cmd;

  if (command == "update_trades") {
    auto& mut_portfolio = const_cast<Portfolio&>(portfolio);
    mut_portfolio.update_trades();
    send(TG_ID, "send", {{"str", tunnel_url + "/trades.html"}});
    return;
  }

  if (command == "status") {
    std::string str, symbol;
    {
      auto _ = portfolio.reader_lock();

      auto sym_it = msg.params.find("sym");
      if (sym_it == msg.params.end())
        str = to_str<FormatTarget::Telegram>(portfolio);
      else if (auto ticker = portfolio.get_ticker(sym_it->second);
               ticker != nullptr) {
        str = to_str<FormatTarget::Telegram>(*ticker);
        symbol = sym_it->second;
      }
    }

    auto load = symbol == ""
                    ? std::format("{}\n\n{}", tunnel_url,
                                  to_str<FormatTarget::Telegram>(HASKELL, str))
                    : std::format("{}/{}.html\n\n{}", tunnel_url, symbol,
                                  to_str<FormatTarget::Telegram>(ELIXIR, str));

    send(TG_ID, "send", {{"str", load}});
    return;
  }

  if (command == "ping") {
    auto str = std::format("```json\nping: \"{}\"\nlast updated: \"{}\"```",  //
                           now_ny_time(), portfolio.last_updated);
    send(TG_ID, "send", {{"str", str}});
    return;
  }

  if (command == "positions") {
    std::string str;
    {
      auto _ = portfolio.reader_lock();
      str =
          to_str<FormatTarget::Telegram>(portfolio.get_positions(), portfolio);
      str = to_str<FormatTarget::Telegram>(HASKELL, str);
    }
    send(TG_ID, "send", {{"str", str}});
    return;
  }

  if (command == "tunnel_url") {
    auto it = msg.params.find("url");
    if (it != msg.params.end()) {
      tunnel_url = it->second;
      send(TG_ID, "send", {{"str", tunnel_url}});
    }
  }

  if (msg.id == TG_ID)
    spdlog::error("[tg] wrong command: {}", command.c_str());
  else if (msg.id == NPM_ID)
    spdlog::error("[npm] wrong command: {}", command.c_str());
}

void Notifier::iter() {
  auto str = to_str<FormatTarget::Telegram>(
      HASKELL, to_str<FormatTarget::Telegram>(portfolio));

  send(TG_ID, "send", {{"str", str}});

  while (!is_stopped()) {
    auto msg_opt = msg_q.pop();
    if (!msg_opt)
      break;
    handle_command(*msg_opt);
  }

  // auto& portfolio = notifier->portfolio;

  // while (!notifier->cfl.is_ready())
  //   ;
  // auto url = notifier->tunnel_url();
  // if (url != "") {
  //   auto msg_id = tg.send(notifier->tunnel_url());
  //   tg.pin_message(msg_id);
  // }

  // auto last_update_id = 0;
  // while (!portfolio.is_killed()) {
  //   std::string alert, status;
  //   {
  //     auto _ = portfolio.reader_lock();
  //     if (last_updated != portfolio.last_updated) {
  //       auto diff = to_str<FormatTarget::Alert>(portfolio, prev_signals);
  //       alert = to_str<FormatTarget::Telegram>(DIFF, diff);

  //       prev_signals.clear();
  //       for (auto& [symbol, ticker] : portfolio.tickers)
  //         prev_signals.emplace(symbol, ticker.metrics.ind_1h.signal);

  //       auto str = to_str<FormatTarget::Telegram>(portfolio);
  //       status = to_str<FormatTarget::Telegram>(HASKELL, str);
  //

  //       last_updated = portfolio.last_updated;
  //     }
  //   }
  //   if (alert != "")
  //     send(Message{TG_ID, "send", {{"str", status + '\n' + alert}}});

  //   // auto [valid, line, id] = to.receive(last_update_id);
  //   // if (valid)
  //   //   notifier->handle_command(line);
  //   // last_update_id = id;

  //   portfolio.wait_for(seconds{5});
  // }
}

Notifier::Notifier(const Portfolio& portfolio)
    : Endpoint{NOTIFIER_ID},
      portfolio{portfolio},
      last_updated{portfolio.last_updated}  //
{
  for (auto& [symbol, ticker] : portfolio.tickers)
    prev_signals.emplace(symbol, ticker.metrics.ind_1h.signal);
  td = std::thread{[this] { iter(); }};
}

Notifier::~Notifier() {
  stop();
  if (td.joinable())
    td.join();
  std::cout << "[exit] notifier" << std::endl;
}
