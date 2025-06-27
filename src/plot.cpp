#include "portfolio.h"

#include <cstdlib>
#include <fstream>
#include <thread>

void Indicators::plot(const std::string& symbol) const {
  const size_t step = 8;  // 4h = 8 * 30m candles

  {
    std::ofstream f("data.csv");
    f << "datetime,price,ema9,ema21,rsi,macd,signal\n";
    for (size_t i = 0; i < candles.size(); ++i) {
      f << std::format("{},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",  //
                       candles[i].datetime, candles[i].close,             //
                       ema9.values[i], ema21.values[i],                   //
                       rsi.values[i],                                     //
                       macd.macd_line[i], macd.signal_line[i]);
    }
    f.flush();
    f.close();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string cmd = "python3 scripts/plot_metrics.py " + symbol;
  std::system(cmd.c_str());
}

void Metrics::plot() const {
  indicators_1h.plot(symbol);
}

void Portfolio::plot(const std::string& ticker) const {
  auto it = tickers.find(ticker);
  if (it == tickers.end())
    return;
  it->second.metrics.plot();
}
