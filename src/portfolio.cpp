#include "portfolio.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

inline bool is_us_market_open_now() {
  using namespace std::chrono;

  auto now = system_clock::now();

  zoned_time eastern_time{locate_zone("America/New_York"), now};
  auto local = eastern_time.get_local_time();
  auto tod = floor<minutes>(local.time_since_epoch() % days{1});  // time of day

  auto total_minutes = duration_cast<minutes>(tod).count();

  int open_minutes = 9 * 60 + 30;
  int close_minutes = 16 * 60;

  return total_minutes >= open_minutes && total_minutes < close_minutes;
}

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

Portfolio::Portfolio() noexcept
    : symbols{Portfolio::read_symbols()},
      td{symbols.size()},
      positions{},
      last_updated{Clock::now()},   //
      update_interval{td.interval}  //
{
#ifndef NDEBUG
  std::cout << "Update interval " << update_interval << std::endl;
#endif

  for (auto& [symbol, priority] : symbols) {
    auto candles = td.time_series(symbol);

    // if filters reject this ticker, don't add it
    auto add = filter(candles, update_interval);
    if (!add) {
#ifndef NDEBUG
      std::cout << "Skipping " << symbol << std::endl;
#endif
      continue;
    }

    tickers.try_emplace(symbol,  //
                        symbol, priority, std::move(candles), update_interval,
                        positions.get_position(symbol));
  }

  write_page();

#ifndef NDEBUG
  std::cout << "Init at " << current_datetime() << std::endl;
#endif
}

void Portfolio::add_candle() {
  {
    auto lock = writer_lock();
    last_updated = Clock::now();

    for (auto& [symbol, ticker] : tickers) {
      auto candle = td.real_time(symbol);

      ticker.metrics.add(candle, positions.get_position(symbol));
      ticker.signal = Signal{ticker.metrics};
    }
  }

  write_page();

#ifndef NDEBUG
  std::cout << "Updated at " << current_datetime() << std::endl;
#endif
}

void Portfolio::add_trade(const Trade& trade) const {
  auto portfolio = const_cast<Portfolio*>(this);
  auto lock = portfolio->writer_lock();
  portfolio->positions.add_trade(trade);

  auto& metrics = portfolio->tickers.at(trade.ticker).metrics;
  metrics.position = positions.get_position(trade.ticker);
}

inline auto sleep(auto mins) {
  return std::this_thread::sleep_for(std::chrono::minutes(mins));
}

void Portfolio::run() {
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

