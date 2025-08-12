#include "portfolio.h"
#include "config.h"
#include "format.h"
#include "raw_mode.h"
#include "times.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <latch>
#include <semaphore>
#include <thread>

inline auto read_symbols() {
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

void Ticker::calculate_signal() {
  stop_loss = StopLoss(metrics);
  signal = gen_signal(-1);
  position_sizing = PositionSizing(metrics, signal, stop_loss);

  spdlog::trace("[stop] {}: {} price={:.2f} atr_stop={:.2f} final={:.2f}",
                symbol, stop_loss.is_trailing, metrics.last_price(),
                stop_loss.atr_stop, stop_loss.final_stop);
}

Portfolio::Portfolio() noexcept
    : symbols{read_symbols()},

      // APIs
      tg{config.tg_en},
      td{symbols.size()},
      rp{td, symbols, config.replay_en},

      // components
      positions{},
      calendar{},

      // timing
      last_updated{},
      update_interval{td.interval}  //
{
  config.sizing_config.capital_usd = td.to_usd(
      config.sizing_config.capital, config.sizing_config.capital_currency);

  std::counting_semaphore<max_concurrency> sem(
      std::thread::hardware_concurrency() / 2);
  std::latch done(symbols.size());

  Timer timer;

  for (auto& [symbol, priority] : symbols) {
    sem.acquire();
    std::jthread([&]() {
      try {
        auto candles = time_series(symbol, H_1);
        if (candles.empty()) {
          spdlog::error("[init] ({}) no candles fetched", symbol.c_str());

          sem.release();
          done.count_down();
          return;
        }

        Ticker ticker(symbol, priority,         //
                      std::move(candles), H_1,  //
                      positions.get_position(symbol),
                      calendar.next_event(symbol));
        {
          auto _ = writer_lock();
          tickers.emplace(symbol, std::move(ticker));
        }

        write_plot_data(symbol);
        spdlog::info("[init] ({})", symbol.c_str());
      } catch (const std::exception& ex) {
        spdlog::error("[init] ({}) error: {}", symbol.c_str(), ex.what());
      }

      sem.release();
      done.count_down();
    }).detach();
  }

  done.wait();
  spdlog::info("[init] took {:.2f}ms", timer.diff_ms());

  if (!tickers.empty())
    last_updated = tickers.begin()->second.metrics.last_updated();

  write_page();

  spdlog::info("[init] at {}", std::format("{}", last_updated).c_str());
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
      try {
        Candle candle = real_time(symbol, H_1);

        if (candle.time() == LocalTimePoint{}) {
          spdlog::error("[add] ({}) invalid candle: {}", symbol.c_str(),
                        to_str(candle).c_str());
          sem.release();
          done.count_down();
          return;
        }
        ticker.add(candle, positions.get_position(symbol));
        write_plot_data(symbol);
      } catch (const std::exception& ex) {
        spdlog::warn("[add] ({}) failed: {}", symbol.c_str(), ex.what());
      }

      sem.release();
      done.count_down();
    }).detach();
  }

  done.wait();
  spdlog::info("[add] took {:.2f}ms", timer.diff_ms());

  if (!tickers.empty())
    last_updated = tickers.begin()->second.metrics.last_updated();

  write_page();
  spdlog::info("[update] at " + std::format("{}", last_updated));
}

void Portfolio::add_candle_sync() {
  Timer timer;
  for (auto& [symbol, ticker] : tickers) {
    auto candle = real_time(symbol, H_1);
    {
      auto _ = writer_lock();
      ticker.add(candle, positions.get_position(symbol));
    }
    write_plot_data(symbol);
  }
  spdlog::info("[add sync] completed in {:.2f} ms", timer.diff_ms());

  if (!tickers.empty())
    last_updated = tickers.begin()->second.metrics.last_updated();

  write_page();
  spdlog::info("[update] at " + std::format("{}", last_updated));
}

void Portfolio::rollback() {
  if (!config.replay_en)
    return;

  {
    auto _ = writer_lock();
    for (auto& [symbol, ticker] : tickers) {
      auto candle = ticker.rollback();
      rp.rollback(symbol, candle);
      write_plot_data(symbol);
    }
  }

  if (!tickers.empty())
    last_updated = tickers.begin()->second.metrics.last_updated();

  write_page();
  spdlog::info("[rollback] to " + std::format("{}", last_updated));
}

std::pair<const Position*, double> Portfolio::add_trade(
    const Trade& trade) const {
  const Position* pos;
  double pnl;

  {
    auto portfolio = const_cast<Portfolio*>(this);

    auto _ = portfolio->writer_lock();
    pnl = portfolio->positions.add_trade(trade);

    auto& ticker = portfolio->tickers.at(trade.ticker);
    pos = positions.get_position(trade.ticker);
    ticker.update_position(pos);
  }
  spdlog::info("[add trade] at " + std::format("{}", last_updated));

  write_plot_data(trade.ticker);
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
    config.update();

    auto [open, remaining] = market_status(update_interval);
    if (!open)
      break;

    sleep(remaining + minutes(4));
    add_candle();
  }

  spdlog::info("[close]");
  while (true)
    sleep(1);
}

void Portfolio::update_trades() {
  {
    auto _ = writer_lock();

    positions.update_trades();
    for (auto& [symbol, ticker] : tickers) {
      ticker.update_position(positions.get_position(symbol));
    }
  }

  for (auto& [symbol, _] : tickers)
    write_plot_data(symbol);
  write_page();

  spdlog::info("[trades] updated at" + std::format("{}", now_ny_time()));
}

void Portfolio::run_replay() {
  if (config.continuous_en) {
    while (rp.has_data()) {
      spdlog::info("[replay] adding data");
      add_candle();
      sleep(config.speed);
    }
    spdlog::info("[replay] done");
    return;
  }

  RawMode _;

  while (rp.has_data()) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
      printf("\b \b");
      fflush(stdout);

      if (ch == 'q')
        break;

      if (ch == 'l')
        add_candle();

      if (ch == 'h')
        rollback();
    }
  }
}
