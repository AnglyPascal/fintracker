#include "portfolio.h"
#include "api.h"
#include "format.h"

#include <cassert>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
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
      signal{metrics},
      position{position}  //
{}

bool Ticker::has_position() const {
  return position != nullptr && position->qty != 0;
}

int Portfolio::get_priority(const Ticker& ticker) const {
  if (positions.get_position(ticker.symbol) != nullptr)
    return 1;
  return ticker.priority;
}

Portfolio::Portfolio(const std::vector<SymbolInfo>& symbols) noexcept
    : td{symbols.size()},
      positions{},
      update_interval{td.interval},
      last_updated{Clock::now()}  //
{
  for (auto& [symbol, priority] : symbols) {
    auto candles = td.time_series(symbol);
    const Position* position = positions.get_position(symbol);
    tickers.try_emplace(symbol, symbol, priority, std::move(candles),
                        update_interval, position);
  }
}

void Portfolio::update() {
  for (auto& [symbol, ticker] : tickers) {
    auto candle = td.real_time(symbol);
    ticker.position = positions.get_position(symbol);
    ticker.metrics.add(candle, ticker.position);
    ticker.signal = Signal{ticker.metrics};
  }
}

void Portfolio::send_tg_update() const {
  write_html();
  if (last_tg_update_msg_id != -1)
    TG::delete_msg(last_tg_update_msg_id);

  last_tg_update_msg_id = TG::send_doc("portfolio.html", "Portfolio overview");
  if (last_tg_update_msg_id != -1)
    TG::pin_message(last_tg_update_msg_id);
}

void Portfolio::send_tg_alert() const {
  std::ostringstream msg;
  msg << "📊 *Market Update*\n\n";

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Exit)
      msg << to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::HoldCautiously)
      msg << to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Entry)
      msg << to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Watchlist)
      msg << to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Caution)
      msg << to_str<FormatTarget::Alert>(ticker);

  TG::send(msg.str());
}

void Portfolio::send_updates() const {
  console_update();

  if (count % 2 == 0)
    send_tg_update();

  send_tg_alert();

  count++;
}

void Portfolio::status(const std::string& symbol) const {
  if (symbol == "")
    return send_tg_update();

  auto it = tickers.find(symbol);
  if (it == tickers.end())
    TG::send(std::format("ticker {} is not being tracked\n", symbol));
  else
    TG::send(std::format("{}", to_str<FormatTarget::Telegram>(it->second)));
}

void Portfolio::run() {
  send_updates();
  auto sleep = [](double mins) {
    std::this_thread::sleep_for(std::chrono::seconds(unsigned(mins * 60)));
  };

  while (true) {
    positions.update();
    if (!is_us_market_open_now()) {
      sleep(5);
      continue;
    }

    auto now = Clock::now();
    if (now < last_updated + minutes(update_interval)) {
      sleep(0.5);
      continue;
    }

    update();
    send_updates();

    last_updated = now;
    debug();

    sleep(1);
  }
}

void Portfolio::debug() const {}

void Portfolio::write_html() const {
  std::ofstream file("portfolio.html");
  file << to_str<FormatTarget::HTML>(*this);
  file.close();
}
