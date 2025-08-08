#include "format.h"
#include "portfolio.h"

#include <fstream>
#include <iostream>
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
        <button id="btn1h" class="active">1H</button>
        <button id="btn4h">4H</button>
        <button id="btn1d">1D</button>
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
    <ul>
      {}
    </ul>
  </div>
</div>
)";

constexpr std::string_view history_ticker_template = R"(
  <li>
    <div class="symbol-entry">
      <div class="symbol">
        {}
      </div>
      <div class="symbol-details">
        {}
        <div class="symbol-forecast"> {} </div>
      </div>
    </div>
  </li>
)";

template <>
inline std::string to_str<FormatTarget::HTML>(const Tickers& tickers,
                                              const minutes& interval,
                                              const int& idx) {
  if (tickers.empty())
    return "";

  auto& m = tickers.rbegin()->second.metrics;
  auto time = m.get_indicators(interval).time(idx);

  std::string entry, watchlist, caution;
  for (auto& [symbol, ticker] : tickers) {
    auto& m = ticker.metrics;
    auto& ind = m.get_indicators(interval);
    auto sig = ind.get_signal(idx);
    auto str = std::format(history_ticker_template, symbol,
                           to_str<FormatTarget::HTML>(sig, ind),
                           to_str<FormatTarget::HTML>(ind.gen_forecast(idx)));

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
        break;
    }
  }

  auto make_td = [](auto& str) { str = std::format(history_td_template, str); };
  make_td(entry);
  make_td(watchlist);
  make_td(caution);

  // std::cout << entry << " " << watchlist << " " << caution << std::end;

  return std::format(history_tr_template, time,  //
                     entry, watchlist, caution);
}

template <>
inline std::string to_str<FormatTarget::HTML>(const Tickers& tickers,
                                              const minutes& interval) {
  if (tickers.empty())
    return "";

  auto& m = tickers.rbegin()->second.metrics;
  auto memory_length = m.get_indicators(interval).memory.memory_length;

  std::string tbl_body = "";
  for (int i = -1; i >= -1 - memory_length; i--) {
    tbl_body += to_str<FormatTarget::HTML>(tickers, interval, i);
  }

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
