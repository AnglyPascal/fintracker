#include "core/replay.h"
#include "ind/candle.h"
#include "util/times.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <glaze/glaze.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>

namespace cereal {
template <class Archive>
void save(Archive& ar, const Candle& c) {
  LocalTimePoint tp = c.time();
  auto secs = tp.time_since_epoch().count();
  ar(secs, c.open, c.high, c.low, c.close, c.volume);
}

template <class Archive>
void load(Archive& ar, Candle& c) {
  std::int64_t secs;
  ar(secs, c.open, c.high, c.low, c.close, c.volume);
  c.datetime = LocalTimePoint{std::chrono::seconds{secs}};
}

template <class Archive>
void serialize(Archive& ar, Timeline& t) {
  ar(t.candles, t.idx);
}
}  // namespace cereal

void write_candles(const std::string& filename, const CandleStore& data) {
  std::ofstream ofs(filename, std::ios::binary);
  cereal::BinaryOutputArchive oarchive(ofs);
  oarchive(data);
}

CandleStore read_candles(const std::string& filename) {
  std::unordered_map<std::string, Timeline> data;
  std::ifstream ifs(filename, std::ios::binary);
  cereal::BinaryInputArchive iarchive(ifs);
  iarchive(data);
  return data;
}

template <>
struct glz::meta<LocalTimePoint> {
  using T = LocalTimePoint;

  static constexpr auto write = [](const T& time_point) {
    return std::format("{:%F %T}", time_point);
  };

  static constexpr auto read = [](T& t, const std::string& str) {
    if (str.size() > 10)
      t = datetime_to_local(str);
    else
      t = date_to_local(str) + hours{9} + minutes{30};
  };

  static constexpr auto value = custom<read, write>;
};

struct APIRes {
  TimeSeriesRes values;
};

TimeSeriesRes read_candles_json(const std::string& str) {
  constexpr auto opts = glz::opts{
      .error_on_unknown_keys = false,
      .quoted_num = true,
  };

  APIRes ts_res;
  auto ec = glz::read<opts>(ts_res, str);
  if (ec) {
    spdlog::error("[td] time_series json error: {}", glz::format_error(ec));
    return {};
  }

  return ts_res.values;
}

struct fx_res_t {
  std::unordered_map<std::string, double> rates;
};

double get_amount_json(double amount,
                       const std::string& currency,
                       const std::string& str) {
  try {
    constexpr auto opts = glz::opts{
        .error_on_unknown_keys = false,
    };

    fx_res_t resp;
    auto ec = glz::read<opts>(resp, str);
    if (ec) {
      spdlog::error("[fx] glaze parse error: {}", glz::format_error(ec, str));
      return amount;
    }

    // Convert: currency -> GBP -> USD
    // If request base is GBP, rates["USD"] gives GBP -> USD
    if (currency == "GBP") {
      auto it = resp.rates.find("USD");
      if (it != resp.rates.end())
        return amount * it->second;
    } else {
      auto it_from = resp.rates.find(currency);
      auto it_usd = resp.rates.find("USD");
      if (it_from != resp.rates.end() && it_usd != resp.rates.end()) {
        double gbp_per_currency = 1.0 / it_from->second;
        return amount * gbp_per_currency * it_usd->second;
      }
    }
  } catch (const std::exception& ex) {
    spdlog::error("[fx] {}->USD error: {}", currency, ex.what());
  }

  return amount;
}
