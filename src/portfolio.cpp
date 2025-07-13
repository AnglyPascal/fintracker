#include "portfolio.h"
#include "raw_mode.h"
#include "times.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

Ticker::Ticker(const std::string& symbol,
               int priority,
               std::vector<Candle>&& candles,
               minutes update_interval,
               const Position* position) noexcept
    : symbol{symbol},
      priority{priority},
      last_polled{Clock::now()},
      metrics{this->symbol, std::move(candles), update_interval, position},
      signal{metrics}  //
{}

bool Ticker::has_position() const {
  return metrics.position != nullptr && metrics.position->qty != 0;
}

std::vector<SymbolInfo> Portfolio::read_symbols() {
  std::vector<SymbolInfo> symbols;
  std::ifstream file("data/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string add, symbol, tier_str;

    if (std::getline(ss, add, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str)) {
      if (add == "+")
        symbols.push_back({symbol, std::stoi(tier_str)});
    }
  }

  return symbols;
}

LocalTimePoint Portfolio::last_updated() const {
  return tickers.empty() ? LocalTimePoint{}
                         : tickers.begin()->second.metrics.last_updated();
}

Portfolio::Portfolio(Config config) noexcept
    : config{config},
      symbols{Portfolio::read_symbols()},
      tg{config.tg_en},
      td{symbols.size()},
      bt{td, symbols, config.backtest_en},
      positions{},
      calendar{},
      update_interval{td.interval}  //
{
  spdlog::info("Update interval: " + std::format("{}", update_interval));

  for (auto& [symbol, priority] : symbols) {
    auto candles = time_series(symbol);
    auto position = positions.get_position(symbol);

    // if filters reject this ticker, don't add it
    auto add = filter(candles, update_interval);
    if (position == nullptr && !add) {
      spdlog::info("Skipping " + symbol);
      continue;
    }

    tickers.try_emplace(symbol, symbol, priority, std::move(candles),
                        update_interval, position);
  }

  write_page();
  spdlog::info("Init at " + std::format("{}", last_updated()));
}

void Portfolio::add_candle() {
  {
    auto _ = writer_lock();

    for (auto& [symbol, ticker] : tickers) {
      auto candle = real_time(symbol);

      auto added = ticker.metrics.add(candle, positions.get_position(symbol));
      if (added)
        ticker.signal = Signal{ticker.metrics};
    }
  }

  write_page();
  spdlog::info("Updated at " + std::format("{}", last_updated()));
}

void Portfolio::rollback() {
  if (!config.backtest_en)
    return;

  {
    auto _ = writer_lock();

    for (auto& [symbol, ticker] : tickers) {
      auto candle = ticker.metrics.pop_back();
      bt.rollback(symbol, candle);
      ticker.signal = Signal{ticker.metrics};
    }
  }

  write_page();
  spdlog::info("Rolled back to " + std::format("{}", last_updated()));
}

std::pair<const Position*, double> Portfolio::add_trade(
    const Trade& trade) const {
  const Position* pos;
  double pnl;

  {
    auto portfolio = const_cast<Portfolio*>(this);

    auto _ = portfolio->writer_lock();
    pnl = portfolio->positions.add_trade(trade);

    auto& metrics = portfolio->tickers.at(trade.ticker).metrics;
    pos = metrics.position = positions.get_position(trade.ticker);
  }

  write_page(trade.ticker);
  spdlog::info("Added trade at " + std::format("{}", last_updated()));

  return {pos, pnl};
}

inline auto sleep(auto mins) {
  return std::this_thread::sleep_for(minutes(mins));
}

void Portfolio::run() {
  if (bt.enabled)
    return run_backtest();

  sleep(update_interval);
  while (!is_us_market_open_now())
    sleep(1);

  while (is_us_market_open_now()) {
    add_candle();
    sleep(update_interval);
  }

  while (true) {
    // TODO: give daily report?
    sleep(1);
  }
}

void Portfolio::run_backtest() {
  RawMode _;

  while (true) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
      std::cout << "\b \b" << std::flush;

      if (ch == 'q')
        break;

      if (ch == 'l')
        add_candle();

      if (ch == 'h')
        rollback();
    }
  }

  std::puts("Exited.");
}

