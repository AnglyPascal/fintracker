#include "core/portfolio.h"
#include "util/config.h"
#include "util/format.h"
#include "util/gen_html_template.h"
#include "util/times.h"

#include <fstream>
#include <thread>

inline constexpr std::string_view ticker_body_template = R"(
  <div><b>Position:</b> {}</div>
  <div><b>Stop Loss:</b> {}</div>
  <div><b>Forecast:</b> {}</div>
  <div><b>Position size:</b> {}</div>
)";

template <>
inline std::string to_str<FormatTarget::HTML>(const Indicators& ind) {
  auto& sig = ind.signal;

  auto stats_html = []<typename S>(auto& stats, S) {
    std::string html;
    for (const auto& [rtype, stat] : stats) {
      if (rtype == S::none)
        continue;
      auto [_, ret, dd, winrate, _, _] = stat;

      S r{rtype};
      auto ret_str = colored("green", ret);
      auto dd_str = colored("red", dd);

      if (r.cls() == SignalClass::Entry)
        ret_str = std::format("<b>{}</b>", ret_str);
      else {
        winrate = 1 - winrate;
        dd_str = std::format("<b>{}</b>", dd_str);
      }

      html += std::format("<li class=\"{}\">{}: {} / {}, <b>{:.2f}</b></li>",
                          r.cls() == SignalClass::Entry ? "entry" : "exit",
                          to_str(r), ret_str, dd_str, winrate);
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

  auto& sym = ticker.symbol;
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
