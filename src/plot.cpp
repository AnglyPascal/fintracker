#include "portfolio.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

inline void trendlines_to_csv(auto& candles,
                              const TrendLine& trend_line,
                              const std::string& file_name) {
  std::ofstream f(file_name);
  f << "datetime,value\n";

  auto period = trend_line.period;
  auto n = candles.size();

  for (int i = candles.size() - period; i < candles.size(); i++) {
    f << std::format("{},{:.2f}\n", candles[i].datetime, trend_line.eval(i));
  }
  f.flush();
  f.close();
}

void Indicators::plot(const std::string& symbol) const {
  // Extract the base signals
  std::vector<double> price_series, ema21_series, rsi_series;
  for (const auto& c : candles)
    price_series.push_back(c.close);
  ema21_series = ema21.values;
  rsi_series = rsi.values;

  // Create trend lines

  std::ofstream f("data.csv");
  f << "datetime,price,ema9,ema21,rsi,macd,signal\n";

  for (size_t i = 0; i < candles.size(); i++)
    f << std::format("{},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
                     candles[i].datetime, candles[i].close, ema9.values[i],
                     ema21.values[i], rsi.values[i], macd.macd_line[i],
                     macd.signal_line[i]);

  f.flush();
  f.close();

  if (!trends.price.top_trends.empty()) {
    auto& top_trends = trends.price.top_trends;
    for (int i = 0; i < top_trends.size(); i++)
      trendlines_to_csv(candles, top_trends[i],
                        std::format("price_fit_{}.csv", i));
  }

  // trendlines_to_csv(candles, trends.ema21, "ema21_fit.csv");
  if (!trends.rsi.top_trends.empty()) {
    auto& top_trends = trends.rsi.top_trends;
    for (int i = 0; i < top_trends.size(); i++)
      trendlines_to_csv(candles, top_trends[i],
                        std::format("rsi_fit_{}.csv", i));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string cmd = "python3 scripts/plot_metrics.py " + symbol;
  std::system(cmd.c_str());
}

void Metrics::plot() const {
  indicators_1h.plot(symbol);
}

void Portfolio::plot_all() const {
  auto td = std::thread(
      [](auto* portfolio) {
        for (auto& [symbol, ticker] : portfolio->tickers)
          ticker.metrics.plot();
      },
      this);
  td.detach();
}
