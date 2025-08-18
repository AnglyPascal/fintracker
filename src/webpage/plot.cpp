#include "core/portfolio.h"
#include "util/format.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <glaze/glaze.hpp>
#include <iostream>

inline constexpr std::string csv_fname(const std::string& symbol,
                                       const std::string& time,
                                       const std::string& fn) {
  return std::format("page/src/{}{}_{}.csv", symbol, time, fn);
}

inline constexpr std::string json_fname(const std::string& symbol,
                                        const std::string& time,
                                        const std::string& fn) {
  return std::format("page/src/{}{}_{}.json", symbol, time, fn);
}

inline constexpr size_t n_days_plot = 90;

struct sr_t {
  std::vector<Zone> support;
  std::vector<Zone> resistance;
};

template <>
struct glz::meta<LocalTimePoint> {
  static constexpr auto value = [](const auto& tp) {
    return std::format("{:%F %T}", tp);
  };
};

template <>
struct glz::meta<Zone> {
  using T = Zone;
  static constexpr auto value = object(  //
      &T::hi,
      &T::lo,
      &T::conf,
      &T::sps  //
  );
};

inline void plot_sr(auto path, auto& support, auto& resistance) {
  sr_t sr{support.zones, resistance.zones};
  constexpr auto opts = glz::opts{.prettify = true};
  std::string buffer;
  auto ec = glz::write_file_json<opts>(sr, path, buffer);
  if (ec)
    spdlog::error("[plot] error writing sr json: {}", path.c_str());
}

LocalTimePoint Indicators::plot(const std::string& sym,
                                const std::string& time) const {
  std::ofstream f(csv_fname(sym, time, "data"));
  f << "datetime,open,close,high,low,volume,ema9,ema21,rsi,macd,signal\n";

  size_t n_candles_per_day = (D_1 + interval - minutes{1}) / interval;
  size_t n = std::min(candles.size(), n_candles_per_day * n_days_plot);

  for (size_t i = candles.size() - n; i < candles.size(); i++)
    f << std::format(
        "{:%F %T},{:.2f},{:.2f},{:.2f},{:.2f},{},"
        "{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
        candles[i].time(), candles[i].open, candles[i].close, candles[i].high,
        candles[i].low, candles[i].volume, _ema9.values[i], _ema21.values[i],
        _rsi.values[i], _macd.macd_line[i], _macd.signal_ema.values[i]);

  f.flush();
  f.close();

  std::ofstream ff(csv_fname(sym, time, "trends"));
  ff << "plot,d0,v0,d1,v1\n";

  auto trends_to_csv = [&](auto& top_trends, auto& name) {
    for (size_t i = 0; i < top_trends.size(); i++) {
      auto& top_trend = top_trends[i];
      auto start = candles.size() - std::min(top_trend.period, n);
      auto end = candles.size() - 1;

      ff << std::format("{},{:%F %T},{:.2f},{:%F %T},{:.2f}\n",  //
                        name,                                    //
                        candles[start].time(), top_trend.eval(start),
                        candles[end].time(), top_trend.eval(end));
    }
  };

  trends_to_csv(trends.price.top_trends, "price");
  trends_to_csv(trends.rsi.top_trends, "rsi");

  ff.flush();
  ff.close();

  plot_sr(json_fname(sym, time, "support_resistance"), support, resistance);

  return candles[candles.size() - n].time();
}

LocalTimePoint Metrics::plot(const std::string& sym) const {
  auto start_1h = ind_1h.plot(sym, "_1h");
  auto start_4h = ind_4h.plot(sym, "_4h");
  auto start_1d = ind_1d.plot(sym, "_1d");
  return std::max({start_1h, start_4h, start_1d});
}

template <>
std::string to_str(const Trade& t) {
  return std::format(                              //
      "{},{},{},{:.2f},{:.2f},{:.2f},{},{}",       //
      closest_nyse_aligned_time(t.date),           //
      t.ticker,                                    //
      (t.action == Action::BUY ? "BUY" : "SELL"),  //
      t.qty,                                       //
      t.px,                                        //
      t.total,                                     //
      t.remark,                                    //
      t.rating                                     //
  );
}

void Portfolio::write_plot_data(const std::string& symbol) const {
  if (!config.plot_en)
    return;

  auto it = tickers.find(symbol);
  if (it == tickers.end())
    return;

  spdlog::trace("plotting {}", symbol.c_str());

  auto start_time = it->second.metrics.plot(symbol);

  std::ofstream f(csv_fname(symbol, "", "trades"));
  f << "datetime,name,action,qty,price,total,remark,rating\n";

  auto& all_trades = get_trades();
  if (auto it = all_trades.find(symbol); it != all_trades.end()) {
    for (auto& trade : it->second) {
      if (trade.time() > start_time)
        f << to_str(trade) << std::endl;
    }
  }
  f.flush();
  f.close();
}
