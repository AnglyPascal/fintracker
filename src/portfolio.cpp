#include "portfolio.h"
#include "format.h"
#include "raw_mode.h"
#include "times.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <future>
#include <iostream>
#include <latch>
#include <semaphore>
#include <thread>

Ticker::Ticker(const std::string& symbol,
               int priority,
               std::vector<Candle>&& candles,
               minutes update_interval,
               const Position* position,
               const std::string& long_term_trend) noexcept
    : symbol{symbol},
      priority{priority},
      last_polled{Clock::now()},
      metrics{std::move(candles), update_interval, position},
      signal{metrics},
      long_term_trend{long_term_trend}  //
{}

inline std::vector<SymbolInfo> read_symbols() {
  std::vector<SymbolInfo> symbols;
  std::ifstream file("data/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string add, symbol, tier_str, sector;

    if (std::getline(ss, add, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str) && std::getline(ss, sector, ',')) {
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

inline uint32_t intervals_passed() {
  auto now = now_ny_time();
  auto time_of_day = now - floor<days>(now);

  auto market_open = hours{9} + minutes{30};

  if (time_of_day <= market_open)
    return 0;

  auto elapsed = time_of_day - market_open;
  return elapsed / minutes{15};
}

Portfolio::Portfolio(Config config) noexcept
    : config{config},
      symbols{read_symbols()},
      tg{config.tg_en},
      td{symbols.size()},
      rp{td, symbols, config.replay_en},
      positions{},
      calendar{},
      update_interval{td.interval},
      intervals_passed{::intervals_passed()}  //
{
  std::counting_semaphore<max_concurrency> sem(
      std::thread::hardware_concurrency() / 2);
  std::latch done(symbols.size());

  Timer timer;

  for (auto& [symbol, priority] : symbols) {
    sem.acquire();
    std::jthread([&, priority]() {
      try {
        auto candles = time_series(symbol);
        auto position = positions.get_position(symbol);
        auto [add, reason] = filter(candles, update_interval);

        if (position == nullptr && !add) {
          spdlog::info("[skip] {}: {}", symbol.c_str(), reason.c_str());
        } else {
          spdlog::debug("[add] {}: {}", symbol.c_str(), reason.c_str());

          Ticker ticker(symbol, priority, std::move(candles), update_interval,
                        position, reason);

          {
            auto _ = writer_lock();
            tickers.emplace(symbol, std::move(ticker));
          }

          write_plot_data(symbol);
        }
      } catch (const std::exception& ex) {
        spdlog::error("[init] error {}: {}", symbol.c_str(), ex.what());
      }

      sem.release();
      done.count_down();
    }).detach();  // safe because latch ensures completion
  }

  done.wait();
  spdlog::info("[init] took {:.2f}ms", timer.diff_ms());

  std::thread([] { std::system("python3 scripts/plot_metrics.py"); }).detach();
  plot();
  write_page();

  spdlog::info("[init] at {}", std::format("{}", last_updated()).c_str());
}

void Portfolio::add_candle() {
  {
    spdlog::info("[add_candle] at {}",
                 std::format("{}", now_ny_time()).c_str());

    std::counting_semaphore<max_concurrency> sem(
        std::thread::hardware_concurrency() / 2);
    std::latch done(tickers.size());

    Timer timer;

    for (auto& [symbol, ticker] : tickers) {
      sem.acquire();
      std::jthread([&]() {
        spdlog::trace("[add_candle] start {}", symbol.c_str());
        try {
          Candle candle = real_time(symbol);
          spdlog::trace("[add_candle] {}: {}", symbol.c_str(),
                        to_str(candle).c_str());

          {
            auto _ = writer_lock();
            ticker.metrics.add(candle, positions.get_position(symbol));
            ticker.signal = Signal{ticker.metrics};
          }

          write_plot_data(symbol);

          spdlog::trace("[add_candle] end {}", symbol.c_str());
        } catch (const std::exception& ex) {
          spdlog::warn("[add_candle] failed. {}: {}", symbol.c_str(),
                       ex.what());
        }

        sem.release();
        done.count_down();
      }).detach();
    }

    done.wait();
    spdlog::info("[add_candle] took {:.2f}ms", timer.diff_ms());

    intervals_passed++;
  }

  plot();
  write_page();
  spdlog::info("[update] at {}", std::format("{}", last_updated()).c_str());
}

void Portfolio::add_candle_sync() {
  {
    Timer timer;
    for (auto& [symbol, ticker] : tickers) {
      auto candle = real_time(symbol);
      {
        auto _ = writer_lock();
        ticker.metrics.add(candle, positions.get_position(symbol));
        ticker.signal = Signal{ticker.metrics};
      }
      write_plot_data(symbol);
    }
    spdlog::info("[add_candle_sync] completed in {:.2f} ms", timer.diff_ms());

    intervals_passed++;
  }

  plot();
  write_page();
  spdlog::info("[update] at " + std::format("{}", last_updated()));
}

void Portfolio::rollback() {
  if (!config.replay_en)
    return;

  {
    auto _ = writer_lock();
    for (auto& [symbol, ticker] : tickers) {
      auto candle = ticker.metrics.pop_back();
      rp.rollback(symbol, candle);
      ticker.signal = Signal{ticker.metrics};
      write_plot_data(symbol);
    }
    intervals_passed--;
  }

  plot();
  write_page();
  spdlog::info("[rollback] to " + std::format("{}", last_updated()));
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
  spdlog::info("[add_trade] at " + std::format("{}", last_updated()));

  write_plot_data(trade.ticker);
  plot(trade.ticker);
  write_page();

  return {pos, pnl};
}

inline auto sleep(auto mins) {
  return std::this_thread::sleep_for(minutes(mins));
}

void Portfolio::run() {
  if (rp.enabled)
    return run_replay();

  while (true) {
    auto [open, remaining] = market_status();
    if (!open)
      break;

    sleep(remaining + minutes(3));
    if (td.latest_datetime() == last_updated())
      sleep(2);

    add_candle();
  }

  spdlog::info("[close]");

  while (true)
    sleep(1);
}

void Portfolio::run_replay() {
  if (config.continuous_en) {
    while (rp.has_data()) {
      add_candle();
      sleep(1);
    }
    return;
  }

  RawMode _;

  while (rp.has_data()) {
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

  std::puts("[exit]");
}

