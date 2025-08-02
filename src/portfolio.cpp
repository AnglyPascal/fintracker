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
               const Position* position) noexcept
    : symbol{symbol},
      priority{priority},
      last_polled{Clock::now()},
      metrics{std::move(candles), update_interval, position}  //
{
  signal = gen_signal(-1);
}

inline std::vector<SymbolInfo> read_symbols() {
  std::vector<SymbolInfo> symbols;
  std::ifstream file("private/tickers.csv");
  std::string line;

  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string add, symbol, tier_str, sector;

    if (std::getline(ss, add, ',') && std::getline(ss, symbol, ',') &&
        std::getline(ss, tier_str, ',') && std::getline(ss, sector, ',')) {
      if (add == "+")
        symbols.push_back({symbol, std::stoi(tier_str)});
    }
  }

  return symbols;
}

LocalTimePoint Portfolio::last_updated() const {
  return _last_updated;
}

Portfolio::Portfolio(Config config) noexcept
    : config{config},
      symbols{::read_symbols()},
      tg{config.tg_en},
      td{symbols.size()},
      rp{td, symbols, config.replay_en},
      positions{},
      calendar{},
      _last_updated{},
      update_interval{td.interval}  //
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
        if (candles.empty()) {
          spdlog::error("[init] no candles fetched for {}", symbol.c_str());

          sem.release();
          done.count_down();
          return;
        }

        auto position = positions.get_position(symbol);
        // FIXME
        // auto [add, reason] = filter(candles, update_interval);

        Ticker ticker(symbol, priority, std::move(candles), update_interval,
                      position);
        {
          auto _ = writer_lock();
          tickers.emplace(symbol, std::move(ticker));
        }

        write_plot_data(symbol);
      } catch (const std::exception& ex) {
        spdlog::error("[init] error {}: {}", symbol.c_str(), ex.what());
      }

      sem.release();
      done.count_down();
    }).detach();  // safe because latch ensures completion
  }

  done.wait();
  spdlog::info("[init] took {:.2f}ms", timer.diff_ms());

  if (!tickers.empty())
    _last_updated = tickers.begin()->second.metrics.last_updated();

  std::thread([] {
    std::string cmd = "python3 scripts/plot_metrics.py " + plot_daemon_port;
    std::system(cmd.c_str());
  }).detach();
  plot();
  write_page();

  spdlog::info("[init] at {}", std::format("{}", last_updated()).c_str());
}

void Portfolio::add_candle() {
  spdlog::info("[add] at {}", std::format("{}", now_ny_time()).c_str());

  std::counting_semaphore<max_concurrency> sem(
      std::thread::hardware_concurrency() / 2);
  std::latch done(tickers.size());

  Timer timer;

  for (auto& [symbol, ticker] : tickers) {
    sem.acquire();
    std::jthread([&]() {
      spdlog::trace("[add] start {}", symbol.c_str());
      try {
        Candle candle = real_time(symbol);

        if (candle.time() == LocalTimePoint{}) {
          spdlog::error("[add] invalid candle {}: {}", symbol.c_str(),
                        to_str(candle).c_str());
          sem.release();
          done.count_down();
          return;
        }

        spdlog::trace("[add] {}: {}", symbol.c_str(), to_str(candle).c_str());

        {
          auto _ = writer_lock();
          ticker.metrics.add(candle, positions.get_position(symbol));
          ticker.signal = ticker.gen_signal();
        }

        write_plot_data(symbol);

        spdlog::trace("[add] end {}", symbol.c_str());
      } catch (const std::exception& ex) {
        spdlog::warn("[add] failed. {}: {}", symbol.c_str(), ex.what());
      }

      sem.release();
      done.count_down();
    }).detach();
  }

  done.wait();
  spdlog::info("[add] took {:.2f}ms", timer.diff_ms());

  if (!tickers.empty())
    _last_updated = tickers.begin()->second.metrics.last_updated();

  plot();
  write_page();
  spdlog::info("[update] at {}", std::format("{}", last_updated()).c_str());
}

void Portfolio::add_candle_sync() {
  Timer timer;
  for (auto& [symbol, ticker] : tickers) {
    auto candle = real_time(symbol);
    {
      auto _ = writer_lock();
      ticker.metrics.add(candle, positions.get_position(symbol));
      ticker.signal = ticker.gen_signal();
    }
    write_plot_data(symbol);
  }
  spdlog::info("[add_candle_sync] completed in {:.2f} ms", timer.diff_ms());

  if (!tickers.empty())
    _last_updated = tickers.begin()->second.metrics.last_updated();

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
      ticker.signal = ticker.gen_signal();
      write_plot_data(symbol);
    }
  }

  if (!tickers.empty())
    _last_updated = tickers.begin()->second.metrics.last_updated();

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
    pos = positions.get_position(trade.ticker);
    metrics.update_position(pos);

    if (!tickers.empty())
      portfolio->_last_updated = tickers.begin()->second.metrics.last_updated();
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

inline auto sleep(double mins) {
  return std::this_thread::sleep_for(seconds((int)(mins * 60)));
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

void Portfolio::update_trades() {
  auto _ = writer_lock();

  positions.update_trades();
  for (auto& [symbol, ticker] : tickers)
    ticker.metrics.update_position(positions.get_position(symbol));
}

void Portfolio::run_replay() {
  if (config.continuous_en) {
    while (rp.has_data()) {
      std::cout << "adding data" << std::endl;
      add_candle();
      sleep(config.speed);
    }
    std::cout << "done" << std::endl;
    return;
  }

  {
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
  }

  std::cout << "exit" << std::endl;
}
