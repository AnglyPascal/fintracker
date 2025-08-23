#include "core/portfolio.h"
#include "util/format.h"

#include <fstream>
#include <thread>

inline constexpr std::string_view ticker_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>{}</title>
    <link rel="stylesheet" href="css/ticker.css">
  </head>
  <body>
    <div id="title">
      <div id="title_text">{}</div>
      <div class="button-container">
        <button id="btn1h" class="active">1H</button>
        <button id="btn4h">4H</button>
        <button id="btn1d">1D</button>
      </div>
    </div>

    <div id="body">
      <div id="chart"></div>
      <div id="initial">{}</div>
      <div id="content"></div>
    </div>

    <script> let ticker = '{}'; </script>
    <script type="module">
    import {{ loadContent }} from "./js/plot.js";
    </script>
  </body>
</html>
)";

inline constexpr std::string_view indicators_template = R"(
<div class="indicators">
  <table>
    <tr> 
      <td class="stats curr-signal">{}</td>
      <td class="stats">
        <div>
          <b>Reason</b>
        </div>
        <table class="stats-tbl reason-tbl">{}</table>
      </td>
      <td class="stats">
        <div>
          <b>Hint</b>
        </div>
        <table class="stats-tbl hints-tbl">{}</table>
      </td>
    </tr>
  </table>

  <h3>Recent Signals</h3>
  <table class="memory-table">
    {}
  </table>
</div>

)";

inline constexpr std::string_view ticker_body_template = R"(
  <div><b>Position:</b> {}</div>
  <div>{}</div>
  <div>{}</div>
  <div><b>Position size:</b> {}</div>
)";

template <>
inline std::string to_str<FormatTarget::HTML>(const Indicators& ind) {
  auto& sig = ind.signal;

  constexpr std::string_view stats_row_templ = R"(
        <tr>
          <td>
            <div class="eventful">
              <div class="{}_signal">{}</div>
              <div class="eventful"> 
                <div class="stats_imp"><b>{}</b></div>
                <div><b>{}</b></div>
              </div>
            </div>
          </td> 
          <td><b>{}</b></td> 
          <td>{:.2f}</td> 
          <td>{} | {}</td> 
          <td>{}</td> 
        </tr>
      )";

  const std::string header = R"(
        <tr class="stats_table_hd">
          <th>
            <div class="eventful">
              <div>sig</div>
              <div class="eventful"> 
                <div class="stats_imp">imp</div>
                <div>wr</div>
              </div>
            </div>
          </th> 
          <th>pnl</th> 
          <th>vol</th> 
          <th>avg_p | avg_l</th> 
          <th>days</th> 
        </tr>
  )";

  auto stats_html = [stats_row_templ, header, &ind]<typename S>(auto& stats,
                                                                S) {
    auto html = header;
    for (const auto& [rtype, stat] : stats) {
      if (rtype == S::none)
        continue;

      S r{rtype};
      auto pnl_str = colored(stat.avg_pnl > 0 ? "green" : "red", stat.avg_pnl);

      auto holding_candles = stat.avg_winning_holding_period;
      auto holding_days = holding_candles / candles_per_day(ind.interval);

      html += std::format(
          stats_row_templ,
          r.cls() == SignalClass::Entry ? "entry" : "exit",  //
          to_str(r), colored("blue", stat.imp),
          colored("border", stat.win_rate), pnl_str, stat.pnl_volatility,
          colored("green", stat.avg_profit), colored("red", stat.avg_loss),
          holding_days  //
      );
    }
    return html;
  };

  // Recent signals memory
  std::string mem_row, mem_html;
  auto& past = ind.memory.past;
  int i = 0;
  for (auto it = past.rbegin(); it != past.rend(); ++it) {
    i++;
    mem_row += std::format("<td class=\"signal-td\">{}</td>",
                           to_str<FormatTarget::HTML>(*it, ind));

    if (i % 4 == 0) {
      mem_html += std::format("<tr>{}</tr>", mem_row);
      mem_row = "";
    }
  }

  return std::format(indicators_template,
                     to_str<FormatTarget::HTML>(sig, ind),    //
                     stats_html(ind.stats.reason, Reason{}),  //
                     stats_html(ind.stats.hint, Hint{}),      //
                     mem_html                                 //
  );
}

template <>
inline std::string to_str<FormatTarget::HTML>(const Ticker& ticker) {
  auto& m = ticker.metrics;
  auto& forecast = ticker.signal.forecast;

  auto body =
      std::format(ticker_body_template,
                  to_str<FormatTarget::HTML>(m.position, m.last_price()),  //
                  to_str<FormatTarget::HTML>(ticker.stop_loss),            //
                  to_str<FormatTarget::HTML>(forecast),                    //
                  to_str<FormatTarget::HTML>(ticker.position_sizing)       //
      );

  auto& sym = ticker.si.symbol;
  return std::format(ticker_template,
                     sym,       //
                     sym,       //
                     body, sym  //
  );
}

void Portfolio::write_tickers() const {
  std::thread([this]() {
    auto _ = reader_lock();

    for (auto& [symbol, ticker] : tickers) {
      std::ofstream f(std::format("page/public/{}.html", symbol));
      f << to_str<FormatTarget::HTML>(ticker);
      f.flush();
      f.flush();

      auto print_ind = [&](auto& ind, auto& time) {
        std::ofstream f(std::format("page/public/{}_{}.html", symbol, time));
        f << to_str<FormatTarget::HTML>(ind);
        f.flush();
        f.flush();
      };

      print_ind(ticker.metrics.ind_1h, "1h");
      print_ind(ticker.metrics.ind_4h, "4h");
      print_ind(ticker.metrics.ind_1d, "1d");
    }
  }).detach();
}
