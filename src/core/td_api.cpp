#include "api.h"
#include "candle.h"
#include "config.h"
#include "times.h"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <unordered_map>

using nlohmann::json;
namespace fs = std::filesystem;

inline constexpr int API_TOKENS = 800;
inline constexpr int MAX_CALLS_MIN = 8;
inline auto N_APIs = config.api_config.td_api_keys.size();

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
