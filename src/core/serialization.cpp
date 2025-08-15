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
struct glz::meta<Candle> {
  using T = Candle;

  static constexpr auto read_x = [](T& candle, const std::string& str) {
    candle.datetime = datetime_to_local(str);
  };

  static constexpr auto write_x = [](auto& candle) -> auto& {
    return std::format("{:%F %T}", candle.datetime);
  };

  static constexpr auto value = object(  //
      "datetime",
      custom<read_x, write_x>,
      "open",
      &T::open,
      "high",
      &T::high,
      "low",
      &T::low,
      "close",
      &T::close,
      "volume",
      &T::volume  //
  );
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
  if (ec)
    spdlog::error("[td] time_series json error: {}", glz::format_error(ec));

  return ts_res.values;
}

struct curr_res_t {
  double amount = -1;
};

double get_amount_json(const std::string& currency, const std::string& str) {
  curr_res_t curr_res;

  constexpr auto opts = glz::opts{
      .error_on_unknown_keys = false,
      .partial_read = true,
  };

  auto ec = glz::read<opts>(curr_res, str);
  if (ec)
    spdlog::error("[td] {}/USD json error: {}", currency,
                  glz::format_error(ec).c_str());

  return curr_res.amount;
}
