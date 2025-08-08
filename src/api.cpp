#include "api.h"
#include "config.h"
#include "indicators.h"
#include "times.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <zmq.hpp>

#include <format>
#include <unordered_map>

using nlohmann::json;
namespace fs = std::filesystem;

/******************
 ** Telegram API **
 ******************/

inline const auto& TG_TOKEN = config.api_config.tg_token;
inline const auto& TG_CHAT_ID = config.api_config.tg_chat_id;
inline const auto& TG_USER = config.api_config.tg_user;

int TG::send(const std::string& text) const {
  spdlog::debug("[tg] send: {}...", text.substr(0, 20).c_str());

  if (!enabled)
    return -1;

  if (text == "")
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/sendMessage", TG_TOKEN);
  cpr::Response r = cpr::Post(cpr::Url{url},
                              cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                                           {"text", text},
                                           {"parse_mode", "Markdown"}});

  if (r.status_code != 200 || r.text == "") {
    spdlog::error("[tg] Error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  auto json = nlohmann::json::parse(r.text);
  return json["result"]["message_id"];
}

int TG::pin_message(int message_id) const {
  spdlog::debug("[tg] pin message: {}", message_id);

  if (!enabled)
    return -1;

  if (!enabled)
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/pinChatMessage", TG_TOKEN);
  cpr::Response r = cpr::Post(
      cpr::Url{url},
      cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                   {"message_id", std::to_string(message_id)},
                   {"disable_notification", "true"}});  // Optional: silent pin

  if (r.status_code != 200) {
    spdlog::error("[tg] Error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  try {
    auto json = json::parse(r.text);
    if (json.contains("ok") && json["ok"].get<bool>() == true)
      return 0;

    spdlog::error("[tg] Failed to pin message: {}", r.text.c_str());
    return -1;
  } catch (...) {
    spdlog::error("[tg] Unexpected error: {}", r.text.c_str());
    return -1;
  }
}

int TG::edit_msg(int message_id, const std::string& text) const {
  spdlog::debug("[tg] edit msg: {} {}...", message_id,
                text.substr(0, 20).c_str());

  if (!enabled)
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/editMessageText", TG_TOKEN);

  cpr::Response r = cpr::Post(
      cpr::Url{url}, cpr::Payload{{"chat_id", std::string(TG_CHAT_ID)},
                                  {"message_id", std::to_string(message_id)},
                                  {"text", text},
                                  {"parse_mode", "Markdown"}});

  if (r.status_code != 200) {
    spdlog::error("[tg] Error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  try {
    auto json = json::parse(r.text);
    if (json.contains("ok") && json["ok"].get<bool>() == true)
      return 0;

    spdlog::error("[tg] Failed to pin message: {}", r.text.c_str());
    return -1;
  } catch (...) {
    spdlog::error("[tg] Unexpected error: {}", r.text.c_str());
    return -1;
  }
}

std::tuple<bool, std::string, int> TG::receive(int last_update_id) const {
  if (!enabled)
    return {false, "", -1};

  auto url =
      std::format("https://api.telegram.org/bot{}/getUpdates?offset={}&limit=1",
                  TG_TOKEN, last_update_id + 1);
  auto r = cpr::Get(cpr::Url{url});

  if (r.status_code != 200) {
    spdlog::error("[tg] Error {}", r.status_code);
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

int TG::delete_msg(int message_id) const {
  spdlog::debug("[tg] delete msg: {}", message_id);

  if (!enabled)
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/deleteMessage", TG_TOKEN);

  cpr::Response r = cpr::Post(
      cpr::Url{url}, cpr::Payload{{"chat_id", TG_CHAT_ID},
                                  {"message_id", std::to_string(message_id)}});

  if (r.status_code != 200) {
    spdlog::error("[tg] Error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  return 0;
}

int TG::send_doc(const std::string& fname,
                 const std::string& copy_name,
                 const std::string& caption) const {
  spdlog::debug("[tg] send doc: {} to {}", fname.c_str(), copy_name.c_str());

  if (!enabled)
    return -1;

  auto url =
      std::format("https://api.telegram.org/bot{}/sendDocument", TG_TOKEN);

  cpr::Multipart multipart{
      {"chat_id", TG_CHAT_ID},
      {"caption", caption},
      {"document", cpr::File{fname, copy_name}, "text/html"}};

  cpr::Response r = cpr::Post(cpr::Url{url}, multipart);

  if (r.status_code != 200) {
    spdlog::error("[tg] Error {}: {}", r.status_code, r.text.c_str());
    return -1;
  }

  auto json = nlohmann::json::parse(r.text);
  return json["result"]["message_id"];
}

/********************
 ** TwelveData API **
 ********************/

inline constexpr int API_TOKENS = 800;
inline constexpr int MAX_CALLS_MIN = 8;
inline constexpr int N_APIs = 5;

inline constexpr minutes get_interval(size_t n_tickers) {
  auto mins = 60 / ((API_TOKENS * N_APIs) / (n_tickers * 8));
  auto m = minutes(mins);
  constexpr minutes intervals[] = {M_15, M_30, H_1, H_2, H_4, D_1};
  for (auto iv : intervals)
    if (m <= iv)
      return iv;
  return D_1;
}

TD::TD(size_t n_tickers) : interval{get_interval(n_tickers)} {
  for (auto key : config.api_config.td_api_keys)
    keys.emplace_back(key);
  spdlog::info("[td] initiated with {} api keys", keys.size());
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
  auto _ = std::lock_guard{mtx};

  int k = -1;
  while ((k = try_get_key()) == -1)
    std::this_thread::sleep_for(seconds(30));

  spdlog::trace("[api_key] {}", k);
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

double TD::to_usd(double amount, const std::string& currency) {
  if (currency == "USD")
    return amount;

  auto symbol = currency + "/USD";
  auto api_key = get_key();

  cpr::Parameters params{{"symbol", symbol},
                         {"amount", std::to_string(amount)},
                         {"apikey", api_key}};

  auto res = cpr::Get(
      cpr::Url{"https://api.twelvedata.com/currency_conversion"}, params);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (res.status_code != 200) {
    spdlog::error("[td] HTTP error {} while converting {} to USD",
                  res.status_code, currency);
    return -1;
  }

  auto j = json::parse(res.text, nullptr, false);
  if (j.is_discarded() || !j.contains("amount")) {
    spdlog::error("[td] Invalid JSON or missing 'converted' for {} -> USD",
                  currency);
    return -1;
  }

  try {
    return j["amount"].get<double>();
  } catch (const std::exception& e) {
    spdlog::error("[td] Exception parsing 'converted' value: {}", e.what());
    return amount;
  }
}

TD::Result TD::api_call(const std::string& symbol,
                        minutes timeframe,
                        size_t output_size) {
  auto api_key = get_key();

  if (output_size > MAX_OUTPUT_SIZE)
    spdlog::error("[td] outputsize exceeds limit");

  cpr::Parameters params{{"symbol", symbol},
                         {"interval", interval_to_str(timeframe)},
                         {"outputsize", std::to_string(output_size)},
                         {"order", "asc"},
                         {"apikey", api_key}};

  auto res =
      cpr::Get(cpr::Url{"https://api.twelvedata.com/time_series"}, params);

  std::this_thread::sleep_for(milliseconds(1500));

  if (res.status_code != 200) {
    spdlog::error("[td] Error: HTTP {}, {}", res.status_code, symbol.c_str());
    return {};
  }

  auto j = json::parse(res.text);
  if (!j.contains("values")) {
    spdlog::error("[td] Error: JSON error, {}", symbol.c_str());
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

TD::Result TD::time_series(const std::string& symbol, minutes timeframe) {
  return api_call(symbol, timeframe);
}

Candle TD::real_time(const std::string& symbol, minutes timeframe) {
  return api_call(symbol, timeframe, 1).back();
}

LocalTimePoint TD::latest_datetime() {
  return real_time("NVDA", interval).time();
}

bool wait_for_file(const std::string& path,
                   seconds freshness,
                   seconds timeout,
                   milliseconds poll_interval) {
  fs::path file_path(path);
  auto deadline = Clock::now() + timeout;

  while (Clock::now() < deadline) {
    if (fs::exists(file_path)) {
      auto mod_time = fs::last_write_time(file_path);
      auto sys_mod_time = clock_cast<SysClock>(mod_time);
      auto now = SysClock::now();

      if (now - sys_mod_time <= freshness)
        return true;
    }

    std::this_thread::sleep_for(poll_interval);
  }

  return false;
}

