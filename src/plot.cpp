#include "portfolio.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

inline void trendlines_to_csv(auto& candles,
                              const TrendLines& trend_lines,
                              const std::string& file_name) {
  std::ofstream f(file_name);
  f << "datetime,value\n";

  auto trend_line = trend_lines.top_trends[0];

  auto period = trend_line.period;
  auto n = candles.size();

  for (int i = candles.size() - period; i < candles.size(); i++) {
    f << std::format("{},{:.2f}\n", candles[i].datetime, trend_line.eval(i));
  }
  f.flush();
  f.close();
}

inline bool wait_for_file(const std::string& path,
                          int timeout_seconds = 30,
                          int poll_interval_ms = 500) {
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

  trendlines_to_csv(candles, trends.price, "price_fit.csv");
  trendlines_to_csv(candles, trends.ema21, "ema21_fit.csv");
  trendlines_to_csv(candles, trends.rsi, "rsi_fit.csv");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string cmd = "python3 scripts/plot_metrics.py " + symbol;
  std::system(cmd.c_str());

  auto fname = symbol + ".html";
  if (wait_for_file(fname))
    TG::send_doc(fname, "Charts for " + symbol);
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
