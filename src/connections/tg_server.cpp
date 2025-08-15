#include "config.h"
#include "servers.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <format>

using nlohmann::json;

inline auto& TG_TOKEN = config.api_config.tg_token;
inline auto& TG_CHAT_ID = config.api_config.tg_chat_id;
inline auto& TG_USER = config.api_config.tg_user;

inline int send_str(const std::string& text) {
  spdlog::debug("[tg] send: {}...", text.substr(0, 20).c_str());
  if (!config.tg_en || text == "")
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/sendMessage", TG_TOKEN);
  auto r = cpr::Post(cpr::Url{url},
                     cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                                  {"text", text},
                                  {"parse_mode", "Markdown"}});

  if (r.status_code != 200 || r.text == "") {
    spdlog::error("[tg] error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  auto json = nlohmann::json::parse(r.text);
  return json["result"]["message_id"];
}

inline void pin_message(int message_id) {
  spdlog::debug("[tg] pin message: {}", message_id);
  if (!config.tg_en)
    return;

  auto url =
      std::format("https://api.telegram.org/bot{}/pinChatMessage", TG_TOKEN);
  cpr::Response r = cpr::Post(
      cpr::Url{url}, cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                                  {"message_id", std::to_string(message_id)},
                                  {"disable_notification", "true"}});

  if (r.status_code != 200)
    spdlog::error("[tg] error {}: {}", r.status_code, r.text.c_str());

  try {
    auto json = json::parse(r.text);
    if (json.contains("ok") && json["ok"].get<bool>() == true)
      return;
    spdlog::error("[tg] Failed to pin message: {}", r.text.c_str());
  } catch (...) {
    spdlog::error("[tg] Unexpected error: {}", r.text.c_str());
  }
}

inline std::tuple<bool, std::string, int> listen(int last_update_id) {
  if (!config.tg_en)
    return {false, "", -1};

  auto url =
      std::format("https://api.telegram.org/bot{}/getUpdates?offset={}&limit=1",
                  TG_TOKEN, last_update_id + 1);
  auto r = cpr::Get(cpr::Url{url});

  if (r.status_code != 200) {
    spdlog::error("[tg] error {}", r.status_code);
    return {false, "", last_update_id};
  }

  auto js = json::parse(r.text);

  for (const auto& update : js["result"]) {
    int update_id = update["update_id"];
    last_update_id = update_id;

    if (!update.contains("message"))
      continue;

    const auto& msg = update["message"];
    if (!msg.contains("text"))
      continue;

    auto user = msg["from"]["username"].get<std::string>();
    if (user != std::string(TG_USER))
      continue;

    return {true, msg["text"], last_update_id};
  }

  return {false, "", last_update_id};
}

inline void delete_msg(int message_id) {
  spdlog::debug("[tg] delete msg: {}", message_id);
  if (!config.tg_en)
    return;

  auto url =
      std::format("https://api.telegram.org/bot{}/deleteMessage", TG_TOKEN);

  cpr::Response r = cpr::Post(
      cpr::Url{url}, cpr::Payload{{"chat_id", TG_CHAT_ID},
                                  {"message_id", std::to_string(message_id)}});

  if (r.status_code != 200)
    spdlog::error("[tg] error {}: {}", r.status_code, r.text.c_str());
}

inline int send_doc(const std::string& fname,
                    const std::string& copy_name,
                    const std::string& caption) {
  spdlog::debug("[tg] send doc: {} to {}", fname.c_str(), copy_name.c_str());

  if (!config.tg_en)
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/sendDocument", TG_TOKEN);

  cpr::Multipart multipart{
      {"chat_id", TG_CHAT_ID},
      {"caption", caption},
      {"document", cpr::File{fname, copy_name}, "text/html"}};

  cpr::Response r = cpr::Post(cpr::Url{url}, multipart);

  if (r.status_code != 200) {
    spdlog::error("[tg] error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  auto json = nlohmann::json::parse(r.text);
  return json["result"]["message_id"];
}

Message TGServer::parse(const std::string& line) const {
  std::istringstream is{line};
  std::string command;
  is >> command;

  if (command == "/ping") {
    return {TG_ID, PORTFOLIO_ID, "ping"};
  } else if (command == "/status") {
    std::string sym;
    is >> sym;
    return {TG_ID, PORTFOLIO_ID, "status", {{"sym", sym}}};
  } else if (command == "/trades") {
    std::string sym;
    is >> sym;
    if (sym == "")
      return {TG_ID, PORTFOLIO_ID, "update_trades"};
    else
      return {TG_ID, PORTFOLIO_ID, "report_trades", {{"sym", sym}}};
  }

  return {TG_ID, PORTFOLIO_ID, ""};
}

void TGServer::t_receive_f() {
  while (!is_stopped()) {
    auto [valid, line, id] = listen(last_update_id);
    if (valid) {
      auto msg = parse(line);
      if (msg.cmd != "") {
        send_to_broker(std::move(msg));
      }
    }

    last_update_id = id;
    msg_q.sleep_for(seconds{5});
  }
}

void TGServer::t_send_f() {
  while (!is_stopped()) {
    auto msg_opt = msg_q.pop();
    if (!msg_opt)
      break;

    auto msg = *msg_opt;
    if (msg.cmd == "send") {
      auto it = msg.params.find("str");
      if (it != msg.params.end())
        send_str(it->second);
    }
  }
}

TGServer::TGServer() noexcept : Endpoint{TG_ID} {
  t_receive = std::thread{[this]() { t_receive_f(); }};
  t_send = std::thread{[this]() { t_send_f(); }};
}

// FIXME: handle exit
TGServer::~TGServer() noexcept {
  stop();
  if (t_receive.joinable())
    t_receive.join();
  if (t_send.joinable())
    t_send.join();
  std::cout << "[exit] tg_server" << std::endl;
}
