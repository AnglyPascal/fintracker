#include "core/portfolio.h"
#include "util/format.h"

#include <fstream>
#include <iostream>
#include <set>
#include <thread>

constexpr std::string_view history_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>History</title>
    <link rel="stylesheet" href="css/history.css">
  </head>

  <body>
    <div id="title">
      <div id="title_text">History</div>
      <div class="button-container">
        <button class="time" id="btn1h" class="active">1H</button>
        <button class="time" id="btn4h">4H</button>
        <button class="time" id="btn1d">1D</button>
      </div>
    </div>

    <div id="body">
      <div id="content-1h">{}</div>
      <div id="content-4h">{}</div>
      <div id="content-1d">{}</div>
    </div>

    <script src="js/history.js"></script>
  </body>
</html>
)";

constexpr std::string_view history_tbl_template = R"(
<div class="history-table">
  <table>
  <tr class="history-header">
    <th class="history-th-time"></th>
    <th class="history-th {}">Entry</th>
    <th class="history-th {}">Watchlist</th>
    <th class="history-th {}">Caution</th>
  </tr>
    {}
  </table>
</div>
)";

constexpr std::string_view history_tr_template = R"(
<tr class="history-tr">
  <td class="history-tr-time">{}</td>
  <td class="history-td history-entry">{}</td>
  <td class="history-td history-watchlist">{}</td>
  <td class="history-td history-caution">{}</td>
</tr>
)";

constexpr std::string_view history_td_template = R"(
<div class="history-td-div">
  <div class="ticker-list">
      {}
  </div>
</div>
)";

constexpr std::string_view history_ticker_template = R"(
    <div class="symbol-entry">
      <button class="symbol sym_{0}">
        {0}
      </button>
      <div class="symbol-details">
        {1}
        <div class="symbol-forecast"> {2} </div>
      </div>
    </div>
)";

inline std::pair<std::string, std::set<std::string>>
table_row(const Tickers& tickers, const minutes& interval, const int& idx) {
  if (tickers.empty())
    return {};

  auto& m = tickers.rbegin()->second.metrics;
  auto time = m.get_indicators(interval).time(idx);

  std::set<std::string> symbols;

  std::string entry, watchlist, caution;
  for (auto& [symbol, ticker] : tickers) {
    auto& m = ticker.metrics;
    auto& ind = m.get_indicators(interval);
    auto sig = ind.get_signal(idx);
    auto str = std::format(history_ticker_template, symbol,
                           to_str<FormatTarget::HTML>(sig, ind),
                           to_str<FormatTarget::HTML>(Forecast{m, idx}));

    symbols.insert(symbol);

    switch (sig.type) {
      case Rating::Entry:
        entry += str;
        break;
      case Rating::Watchlist:
        watchlist += str;
        break;
      case Rating::Exit:
      case Rating::Caution:
      case Rating::HoldCautiously:
        caution += str;
        break;
      default:
        symbols.erase(symbol);
        break;
    }
  }

  auto make_td = [](auto& str) { str = std::format(history_td_template, str); };
  make_td(entry);
  make_td(watchlist);
  make_td(caution);

  return {std::format(history_tr_template, time,  //
                      entry, watchlist, caution),
          symbols};
}

template <>
inline std::string to_str<FormatTarget::HTML>(const Tickers& tickers,
                                              const minutes& interval) {
  if (tickers.empty())
    return "";

  auto& m = tickers.rbegin()->second.metrics;
  auto memory_length = m.get_indicators(interval).memory.memory_length;

  std::set<std::string> symbols;

  std::string tbl_body = "";
  for (int i = -1; i >= -1 - memory_length; i--) {
    auto [row, syms] = table_row(tickers, interval, i);
    tbl_body += row;
    symbols.insert(syms.begin(), syms.end());
  }

  // std::string buttons = "";
  // for (auto& symbol : symbols)
  //   buttons += std::format(history_ticker_button, symbol);
  // buttons = std::format(history_ticker_button_group, buttons);

  return std::format(history_tbl_template,
                     to_str<FormatTarget::HTML>(Rating::Entry),
                     to_str<FormatTarget::HTML>(Rating::Watchlist),
                     to_str<FormatTarget::HTML>(Rating::Caution), tbl_body);
}

void Portfolio::write_history() const {
  std::thread([this]() {
    auto _ = reader_lock();

    auto tbl_1h = to_str<FormatTarget::HTML>(tickers, H_1);
    auto tbl_4h = to_str<FormatTarget::HTML>(tickers, H_4);
    auto tbl_1d = to_str<FormatTarget::HTML>(tickers, D_1);

    auto history = std::format(history_template, tbl_1h, tbl_4h, tbl_1d);

    std::ofstream f(std::format("page/public/history.html"));
    f << history;
    f.flush();
    f.close();
  }).detach();
}
