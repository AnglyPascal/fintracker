#include "core/portfolio.h"
#include "mt/sleeper.h"
#include "mt/thread_pool.h"
#include "util/config.h"
#include "util/format.h"
#include "util/raw_mode.h"
#include "util/times.h"

#include <spdlog/spdlog.h>
#include <iostream>

#include <thread>

Portfolio::Portfolio() noexcept
    : Endpoint{PORTFOLIO_ID},
      symbols{},

      // APIs
      td{symbols.size()},
      rp{td, symbols},

      // components
      positions{},
      calendar{symbols},

      // timing
      update_interval{td.interval},
      last_updated{}  //
{
  auto func = [this](SymbolInfo&& si) {
    if (sleeper.should_shutdown())
      return false;

    auto symbol = si.symbol;

    auto candles = time_series(symbol, H_1);
    if (candles.empty()) {
      spdlog::error("[init] ({}) no candles", symbol.c_str());
      return true;
    }

    Ticker ticker{si, calendar.next_event(symbol),  //
                  std::move(candles), H_1, positions.get_position(symbol)};

    {
      auto _ = writer_lock();
      tickers.try_emplace(symbol, std::move(ticker));
    }

    write_plot_data(symbol);
    spdlog::info("[init] ({})", symbol.c_str());

    return true;
  };

  Timer timer;
  {
    thread_pool<SymbolInfo> pool{config.n_concurrency, func, symbols.arr};
  }
  auto ms = timer.diff_ms();

  if (sleeper.should_shutdown() || tickers.empty()) {
    sleeper.request_shutdown();
    return;
  }

  last_updated = now_ny_time();
  write_page();

  server = std::thread{[this] { iter(); }};

  spdlog::info("[init] at {} took {:.2f}ms",
               std::format("{}", last_updated).c_str(), ms);
}

void Portfolio::add_candle() {
  auto func = [&](SymbolInfo&& si) {
    if (sleeper.should_shutdown())
      return false;

    auto& symbol = si.symbol;

    auto it = tickers.find(symbol);
    if (it == tickers.end())
      return true;

    auto [_, next] = real_time(symbol, H_1);
    if (next.time() == LocalTimePoint{}) {
      spdlog::error("[push_back] ({}) invalid candle: {}", symbol.c_str(),
                    to_str(next).c_str());
      return true;
    }

    it->second.push_back(next, positions.get_position(symbol));
    write_plot_data(symbol);

    return true;
  };

  Timer timer;
  {
    thread_pool<SymbolInfo> pool{config.n_concurrency, func, symbols.arr};
  }
  auto ms = timer.diff_ms();

  if (sleeper.should_shutdown())
    return;

  rp.roll_fwd();

  last_updated = now_ny_time();
  write_page();

  send_to_broker(id, "update");

  spdlog::info("[update] at {} took {:.2f}ms",
               std::format("{}", last_updated).c_str(), ms);
}

void Portfolio::rollback() {
  if (!config.replay_en)
    return;

  {
    auto _ = writer_lock();
    for (auto& [symbol, ticker] : tickers) {
      ticker.rollback();
      rp.rollback(symbol);
      write_plot_data(symbol);
    }
  }

  if (!tickers.empty())
    last_updated = tickers.begin()->second.metrics.last_updated();

  write_page();
  spdlog::info("[rollback] to " + std::format("{}", last_updated));
}

std::pair<const Position*, double> Portfolio::add_trade(const Trade& trade) {
  std::pair<const Position*, double> res;
  {
    auto _ = writer_lock();
    res.second = positions.add_trade(trade);

    auto& ticker = tickers.at(trade.ticker);
    res.first = positions.get_position(trade.ticker);
    ticker.update_position(res.first);
  }

  write_plot_data(trade.ticker);

  spdlog::info("[trades] added at " + std::format("{}", now_ny_time()));
  return res;
}

void Portfolio::update_trades() {
  {
    auto _ = writer_lock();
    positions.update_trades();
    for (auto& [symbol, ticker] : tickers)
      ticker.update_position(positions.get_position(symbol));
  }

  for (auto& [symbol, _] : tickers)
    write_plot_data(symbol);

  spdlog::info("[trades] updated at" + std::format("{}", now_ny_time()));
}

void Portfolio::run() {
  std::cerr << "[start] construction done" << std::endl;

  if (config.replay_en)
    return run_replay();

  while (!sleeper.should_shutdown()) {
    config.update();

    auto [open, remaining] = market_status(update_interval);
    if (!open || !sleeper.sleep_for(remaining)) {
      stop();
      break;
    }

    add_candle();
  }
}

void Portfolio::run_replay() {
  if (!config.replay_paused) {
    while (!sleeper.should_shutdown() && rp.has_data()) {
      if (!sleeper.sleep_for(config.update_interval())) {
        stop();
        break;
      }
      add_candle();
    }
    return;
  }

  RawMode _;

  while (!sleeper.should_shutdown() && rp.has_data()) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
      printf("\b \b");
      fflush(stdout);

      if (ch == 'q') {
        sleeper.request_shutdown();
        break;
      }

      if (ch == 'l')
        add_candle();

      if (ch == 'h')
        rollback();
    }
  }
}

Portfolio::~Portfolio() noexcept {
  stop();
  sleeper.request_shutdown();
  if (server.joinable())
    server.join();
  std::cout << "[exit] portfolio" << std::endl;
}
