#include "format.h"
#include "portfolio.h"

#include <spdlog/spdlog.h>
#include <fstream>


inline constexpr std::string csv_fname(const std::string& symbol,
                                       const std::string& time,
                                       const std::string& fn) {
  return std::format("page/src/{}{}_{}.csv", symbol, time, fn);
}

inline constexpr size_t n_days_plot = 90;

LocalTimePoint Indicators::plot(const std::string& sym,
                                const std::string& time) const {
  std::ofstream f(csv_fname(sym, time, "data"));
  f << "datetime,open,close,high,low,volume,ema9,ema21,rsi,macd,signal\n";

  size_t n_candles_per_day = (D_1 + interval - minutes{1}) / interval;
  size_t n = std::min(candles.size(), n_candles_per_day * n_days_plot);

  for (size_t i = candles.size() - n; i < candles.size(); i++)
    f << std::format(
        "{},{:.2f},{:.2f},{:.2f},{:.2f},{},"
        "{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
        candles[i].datetime, candles[i].open, candles[i].close, candles[i].high,
        candles[i].low, candles[i].volume, _ema9.values[i], _ema21.values[i],
        _rsi.values[i], _macd.macd_line[i], _macd.signal_ema.values[i]);

  f.flush();
  f.close();

  auto trends_to_csv = [this, n, &sym, &time](auto& top_trends, auto& name) {
    for (size_t i = 0; i < top_trends.size(); i++) {
      std::ofstream f(csv_fname(sym, time, std::format("{}_fit_{}", name, i)));
      f << "datetime,value\n";

      auto& top_trend = top_trends[i];
      auto m = std::min((size_t)top_trend.period, n);
      for (size_t j = candles.size() - m; j < candles.size(); j++)
        f << std::format("{},{:.2f}\n", candles[j].datetime, top_trend.eval(j));

      f.flush();
      f.close();
    }
  };

  trends_to_csv(trends.price.top_trends, "price");
  trends_to_csv(trends.rsi.top_trends, "rsi");

  std::ofstream sr(csv_fname(sym, time, "support_resistance"));
  sr << "support,lower,upper,confidence\n";
  for (auto [lo, hi, conf] : support.zones)
    sr << std::format("0,{:.2f},{:.2f},{:.2f}\n", lo, hi, conf);
  for (auto [lo, hi, conf] : resistance.zones)
    sr << std::format("1,{:.2f},{:.2f},{:.2f}\n", lo, hi, conf);
  sr.flush();
  sr.close();

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
