#include "core/portfolio.h"
#include "util/format.h"

#include <fstream>
#include <string_view>
#include <thread>

inline constexpr std::string_view positions_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Positions</title>
    <link rel="stylesheet" href="css/ticker.css">
  </head>
  <body>
    <table> 
      <tr class="header">
        <th>Symbol</th>
        <th>Date</th>
        <th>Qty</th>
        <th>Px</th>
      </tr>
      {}
    <table>
    <script src="js/history.js"></script>
  </body>
</html>
)";

inline constexpr std::string_view positions_row_template = R"(
  <tr>
    <td>{}</td>
    <td>{:%a, %b %d, %H:%M}</td>
    <td>{}</td>
    <td>{}</td>
  </tr>
)";

inline constexpr std::string_view positions_signal_template = R"(
<table>
  <tr>
    <td colspan="4" class="risk">{}</td>
  </tr>
  <tr>
    {}
  </tr>
</table>
)";

template <>
std::string to_str<FormatTarget::HTML>(const Tickers& tickers,
                                       const Indicators& spy,
                                       const OpenPositions& positions) {
  std::string body = "";
  for (auto& [symbol, pos] : positions.get_positions()) {
    auto it = tickers.find(symbol);
    if (it == tickers.end())
      continue;
    auto& ticker = it->second;

    body +=
        std::format(positions_row_template, symbol, pos.tp, pos.qty, pos.px);

    auto& m = ticker.metrics;
    CombinedSignal combined_signal{m, ticker.ev, pos.tp};
    auto sig_1h = m.ind_1h.get_signal(pos.tp);
    auto sig_4h = m.ind_4h.get_signal(pos.tp);
    auto sig_1d = m.ind_1d.get_signal(pos.tp);
    Risk risk{m, spy, pos.tp, combined_signal, positions, ticker.ev};

    body += std::format(                                     //
        positions_signal_template,                           //
        risk.overall_rationale,                              //
        to_str<FormatTarget::HTML>(combined_signal, ticker)  //
    );
  }

  return std::format(positions_template, body);
}

void Portfolio::write_positions() const {
  std::thread([this]() {
    auto _ = reader_lock();
    std::ofstream f("page/public/positions.html");
    f << to_str<FormatTarget::HTML>(tickers, spy, positions);
    f.flush();
    f.flush();
  }).detach();
}
