#include "portfolio.h"

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

inline void Portfolio::handle_command(const Message& msg) {
  auto command = msg.cmd;

  if (command == "update_trades") {
    update_trades();
    send_to_broker(TG_ID, "send", {{"str", tunnel_url + "/trades.html"}});
    return;
  }

  if (command == "status") {
    std::string str, symbol;
    {
      auto _ = reader_lock();

      auto sym_it = msg.params.find("sym");
      if (sym_it == msg.params.end())
        str = to_str<FormatTarget::Telegram>(*this);
      else if (auto ticker = get_ticker(sym_it->second); ticker != nullptr) {
        str = to_str<FormatTarget::Telegram>(*ticker);
        symbol = sym_it->second;
      }
    }

    auto load = symbol == ""
                    ? std::format("{}\n\n{}", tunnel_url,
                                  to_str<FormatTarget::Telegram>(HASKELL, str))
                    : std::format("{}/{}.html\n\n{}", tunnel_url, symbol,
                                  to_str<FormatTarget::Telegram>(ELIXIR, str));

    send_to_broker(TG_ID, "send", {{"str", load}});
    return;
  }

  if (command == "ping") {
    auto str = std::format("```json\nping: \"{}\"\nlast updated: \"{}\"```",  //
                           now_ny_time(), last_updated);
    send_to_broker(TG_ID, "send", {{"str", str}});
    return;
  }

  if (command == "positions") {
    std::string str;
    {
      auto _ = reader_lock();
      str = to_str<FormatTarget::Telegram>(get_positions(), *this);
      str = to_str<FormatTarget::Telegram>(HASKELL, str);
    }
    send_to_broker(TG_ID, "send", {{"str", str}});
    return;
  }

  if (command == "tunnel_url") {
    auto it = msg.params.find("url");
    if (it != msg.params.end()) {
      tunnel_url = it->second;
      send_to_broker(TG_ID, "send", {{"str", tunnel_url}});
    }
    return;
  }
}

void Portfolio::iter() {
  auto str = to_str<FormatTarget::Telegram>(
      HASKELL, to_str<FormatTarget::Telegram>(*this));

  send_to_broker(TG_ID, "send", {{"str", str}});

  while (!is_stopped()) {
    auto msg_opt = msg_q.pop();
    if (!msg_opt)
      break;
    handle_command(*msg_opt);
  }
}
