#include "api.h"
#include "indicators.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <format>
#include <iostream>
#include <unordered_map>

using nlohmann::json;

#ifndef TD_API_KEY_1
#define TD_API_KEY_1 ""
#endif

#ifndef TD_API_KEY_2
#define TD_API_KEY_2 ""
#endif

#ifndef TD_API_KEY_3
#define TD_API_KEY_3 ""
#endif

#ifndef TG_TOKEN
#define TG_TOKEN ""
#endif

#ifndef TG_CHAT_ID
#define TG_CHAT_ID ""
#endif

#ifndef TG_USER
#define TG_USER ""
#endif

int TG::send(const std::string& text) {
  if (text == "")
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/sendMessage", TG_TOKEN);
  cpr::Response r = cpr::Post(cpr::Url{url},
                              cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                                           {"text", text},
                                           {"parse_mode", "Markdown"}});

  if (r.status_code != 200 || r.text == "") {
    std::cerr << "[tg] Error " << r.status_code << ": " << r.text << std::endl
              << text << std::endl;
    return -1;
  }

  auto json = nlohmann::json::parse(r.text);
  return json["result"]["message_id"];
}

int TG::pin_message(int message_id) {
  auto url =
      std::format("https://api.telegram.org/bot{}/pinChatMessage", TG_TOKEN);
  cpr::Response r = cpr::Post(
      cpr::Url{url},
      cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                   {"message_id", std::to_string(message_id)},
                   {"disable_notification", "true"}});  // Optional: silent pin

  if (r.status_code != 200) {
    std::cerr << "[tg] Error " << r.status_code << ": " << r.text << std::endl;
    return -1;
  }

  try {
    auto json = json::parse(r.text);
    if (json.contains("ok") && json["ok"].get<bool>() == true)
      return 0;

    std::cerr << "[tg] Failed to pin message: " << r.text << std::endl;
    return -1;
  } catch (...) {
    std::cerr << "[tg] unexpected error: " << r.text << std::endl;
    return -1;
  }
}

int TG::edit_msg(int message_id, const std::string& text) {
  auto url =
      std::format("https://api.telegram.org/bot{}/editMessageText", TG_TOKEN);

  cpr::Response r = cpr::Post(
      cpr::Url{url}, cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                                  {"message_id", std::to_string(message_id)},
                                  {"text", text},
                                  {"parse_mode", "Markdown"}});

  if (r.status_code != 200) {
    std::cerr << "[tg] Error " << r.status_code << ": " << r.text << std::endl
              << text << std::endl;
    return -1;
  }

  try {
    auto json = json::parse(r.text);
    if (json.contains("ok") && json["ok"].get<bool>() == true)
      return 0;

    std::cerr << "[tg] Failed to edit message: " << r.text << std::endl;
    return -1;
  } catch (...) {
    std::cerr << "[tg] unexpected error: " << r.text << std::endl;
    return -1;
  }
}

std::tuple<bool, std::string, int> TG::receive(int last_update_id) {
  auto url =
      std::format("https://api.telegram.org/bot{}/getUpdates?offset={}&limit=1",
                  TG_TOKEN, last_update_id + 1);
  auto response = cpr::Get(cpr::Url{url});

  if (response.status_code != 200) {
    std::cerr << "[tg] Error: " << response.status_code << "\n";
    return {false, "", last_update_id};
  }

  auto js = json::parse(response.text);

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

int TG::delete_msg(int message_id) {
  auto url =
      std::format("https://api.telegram.org/bot{}/deleteMessage", TG_TOKEN);

  cpr::Response r = cpr::Post(
      cpr::Url{url}, cpr::Payload{{"chat_id", TG_CHAT_ID},
                                  {"message_id", std::to_string(message_id)}});

  if (r.status_code != 200) {
    std::cerr << "Telegram delete error: " << r.status_code << " - " << r.text
              << "\n";
    return -1;
  }

  return 0;
}

int TG::send_doc(const std::string& fname,
                 const std::string& copy_name,
                 const std::string& caption) {
  auto url =
      std::format("https://api.telegram.org/bot{}/sendDocument", TG_TOKEN);

  cpr::Multipart multipart{
      {"chat_id", TG_CHAT_ID},
      {"caption", caption},
      {"document", cpr::File{fname, copy_name}, "text/html"}};

  cpr::Response r = cpr::Post(cpr::Url{url}, multipart);

  if (r.status_code != 200) {
    std::cerr << "Telegram API error: " << r.status_code
              << "Response: " << r.text << "\n";
    return -1;
  }

  auto json = nlohmann::json::parse(r.text);
  return json["result"]["message_id"];
}

inline constexpr minutes get_interval(size_t n_tickers) {
  auto mins = 60 / ((API_TOKENS * Ns) / (n_tickers * 8));
  auto m = minutes(mins);
  constexpr minutes intervals[] = {M_15, M_30, H_1, H_2, H_4, D_1};
  for (auto iv : intervals)
    if (m <= iv)
      return iv;
  return D_1;
}

TD::TD(size_t n_tickers) : interval{get_interval(n_tickers)} {
  constexpr char const* api_keys[] = {TD_API_KEY_1, TD_API_KEY_2, TD_API_KEY_3};
  for (auto key : api_keys)
    keys.emplace_back(key);
}

int TD::try_get_key() {
  TimePoint now = Clock::now();

  for (size_t i = 0; i < keys.size(); i++) {
    auto k = idx;
    idx = (idx + 1) % keys.size();

    auto& api_key = keys[k];
    if (api_key.daily_calls >= API_TOKENS)
      continue;

    auto& timestamps = api_key.call_timestamps;
    while (!timestamps.empty()) {
      auto duration = now - timestamps.front();
      if (duration < minutes(1))
        break;

      timestamps.pop_front();
    }

    if (timestamps.size() < MAX_CALLS_MIN)
      return k;
  }

  return -1;
}

const std::string& TD::get_key() {
  int k = -1;
  while ((k = try_get_key()) == -1)
    std::this_thread::sleep_for(std::chrono::seconds(30));

  auto& api_key = keys[k];
  api_key.daily_calls++;
  api_key.call_timestamps.push_back(Clock::now());

  return api_key.key;
}

inline std::string interval_to_str(minutes interval) {
  const std::unordered_map<size_t, std::string> str_map{
      {M_1.count(), "1min"},   {M_5.count(), "5min"}, {M_15.count(), "15min"},
      {M_30.count(), "30min"}, {H_1.count(), "1h"},   {H_2.count(), "2h"},
      {H_4.count(), "4h"},     {D_1.count(), "1day"},
  };
  auto it = str_map.find(interval.count());
  if (it == str_map.end())
    return "";
  return it->second;
}

TD::Result TD::api_call(const std::string& symbol, size_t output_size) {
  auto api_key = get_key();

  if (output_size > MAX_OUTPUT_SIZE)
    std::cerr << "outputsize exceeds limit" << std::endl;

  auto url = std::format(
      "https://api.twelvedata.com/time_series?symbol={}"
      "&interval={}&outputsize={}&order=asc&apikey={}",
      symbol, interval_to_str(interval), output_size, api_key);

  auto res = cpr::Get(cpr::Url{url});
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  if (res.status_code != 200) {
    std::cerr << "[12] Error: HTTP " << res.status_code << " " << symbol
              << std::endl;
    return {};
  }

  auto j = json::parse(res.text);
  if (!j.contains("values")) {
    std::cerr << "[12] Error: JSON error " << symbol << std::endl;
    return {};
  }

  std::vector<Candle> candles;
  for (const auto& entry : j["values"])
    candles.emplace_back(entry["datetime"].get<std::string>(),
                         std::stod(entry["open"].get<std::string>()),
                         std::stod(entry["high"].get<std::string>()),
                         std::stod(entry["low"].get<std::string>()),
                         std::stod(entry["close"].get<std::string>()),
                         std::stoi(entry["volume"].get<std::string>()));

  return candles;
}

TD::Result TD::time_series(const std::string& symbol, int n_days) {
  size_t output_size =
      n_days * 8 /* hours per day */ * (std::chrono::minutes(60) / interval);
  return api_call(symbol, output_size);
}

Candle TD::real_time(const std::string& symbol) {
  return api_call(symbol, 1).back();
}

bool wait_for_file(const std::string& path,
                   int timeout_seconds,
                   int poll_interval_ms) {
  namespace fs = std::filesystem;

  fs::path file_path(path);
  auto start = std::chrono::steady_clock::now();

  std::optional<fs::file_time_type> last_mod_time;

  while (true) {
    if (fs::exists(file_path)) {
      auto mod_time = fs::last_write_time(file_path);
      if (!last_mod_time || mod_time != *last_mod_time)
        return true;
    }

    if (std::chrono::steady_clock::now() - start >
        std::chrono::seconds(timeout_seconds)) {
      return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
  }
}
