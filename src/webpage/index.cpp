#include "core/portfolio.h"
#include "util/format.h"
#include "util/gen_html_template.h"

#include <filesystem>
#include <fstream>
#include <thread>

inline constexpr std::string_view index_reload =
    R"(<meta http-equiv="refresh" content="1">)";

inline constexpr std::string_view index_subtitle_template = R"(
    <div id="title">Portfolio Overview</div>
    <div id="subtitle">
      <div class="update-block">
        <b>Updated</b>: {}
      </div>
      <div class="update-block">
        <a href="trades.html" target="_blank"><b>Trades</b></a>
      </div>
      <div class="update-block">
        <a href="history.html" target="_blank"><b>History</b></a>
      </div>
      <div class="update-block">
        <b>Earnings:</b> 
          <a href="https://finance.yahoo.com/calendar/earnings/" target="_blank">Yahoo</a>
          <a href="calendar.html">Calendar</a>
      </div>
      <div class="update-block">
        <b>Analyst Updates:</b> 
          <a href="https://www.marketbeat.com/ratings/us/" target="_blank">MarketBeat</a>
      </div>
    </div>
)";

inline constexpr std::string_view index_event_template = R"(
  <a href="https://finance.yahoo.com/quote/{}/analysis/" 
     target="_blank">
     {}
  </a>
)";

inline constexpr std::string_view index_row_template = R"(
  <tr class="info-row {1}" 
      id="row-{0}" 
      style="{2}"
      onclick="toggleSignalDetails(this, '{0}-details') ">
    <td data-label="Signal">{3}</td>
    <td data-label="Symbol">
      <div class="eventful">
        <a href="{0}.html" target="_blank">{4}</a>{5}
      </div>
    </td>
    <td data-label="Price"><div>{6}</div></td>
    <td data-label="EMA"><div>{7}</div></td>
    <td data-label="RSI"><div>{8}</div></td>
    <td data-label="MACD"><div>{9}</div></td>
    <td data-label="Position"><div>{10}</div></td>
    <td data-label="Stop Loss"><div>{11}</div></td>
  </tr>
)";

inline constexpr std::string_view index_signal_td_template = R"(
  <td class="{}">
    {}
  </td>
)";

inline constexpr std::string_view index_signal_template = R"(
  <tr id="{}-details" 
      class="signal-details-row" 
      style="display:none">
    <td colspan="8" class="signal-table">
      <div class="signal-table">
        <table class="signal">
          <tr class="combined-signal-tr">{}</tr>
          <tr class="signal-memory-tr">{}</tr>
        </table>
      </div>
    </td>
  </tr>
)";

inline bool hide(auto& m, auto& type) {
  if (m.has_position())
    return false;
  return (type == Rating::Caution || type == Rating::Mixed ||
          type == Rating::None || type == Rating::Skip);
};

template <>
inline std::string to_str<FormatTarget::HTML>(const Metrics& m,
                                              const StopLoss& stop_loss)  //
{
  auto price = std::format("<b>{:.2f}</b>", m.last_price());
  if (!m.has_position())
    return price;
  return price + ", " + to_str(stop_loss);
}

template <>
inline std::string to_str<FormatTarget::HTML>(const std::string& sym,
                                              const Event& ev)  //
{
  return std::format(index_event_template, sym, to_str(ev));
}

template <>
inline std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  std::vector<std::pair<const std::string*, const Ticker*>> sorted;
  for (auto& [symbol, ticker] : p.tickers)
    sorted.emplace_back(&symbol, &ticker);

  std::sort(sorted.begin(), sorted.end(), [](auto& lhs, auto& rhs) {
    auto lt = lhs.second->signal.type;
    auto rt = rhs.second->signal.type;
    return (lt == rt) ? *lhs.first < *rhs.first : lt < rt;
  });

  for (auto [s, t] : sorted) {
    auto& symbol = *s;
    auto& ticker = *t;

    auto& m = ticker.metrics;
    auto& ind_1h = m.ind_1h;

    auto& sig = ticker.signal;
    auto& sig_1h = ind_1h.signal;

    auto stop_str = sig.stop_hit.str();
    auto str = [&](auto src) {
      return to_str<FormatTarget::HTML>(sig_1h, src);
    };

    body += std::format(
        index_row_template,                                                   //
        symbol,                                                               //
        to_str<FormatTarget::HTML>(sig.type),                                 //
        hide(m, sig.type) ? "display:none;" : "",                             //
        to_str<FormatTarget::HTML>(sig),                                      //
        ticker.si.priority == 3 ? symbol : std::format("<b>{}</b>", symbol),  //
        to_str<FormatTarget::HTML>(symbol, ticker.ev),                        //
        str(Source::Price) + stop_str + str(Source::SR),                      //
        str(Source::EMA),                                                     //
        str(Source::RSI),                                                     //
        str(Source::MACD),                                                    //
        to_str<FormatTarget::HTML>(m.position, m.last_price()),               //
        to_str<FormatTarget::HTML>(m, ticker.stop_loss)                       //
    );

    std::string mem_str = "";
    int n = 1;
    auto& past_sigs = ind_1h.memory.past;
    for (auto it = past_sigs.rbegin(); it != past_sigs.rend(); it++) {
      auto s =
          std::format(index_signal_td_template,  //
                      "old_signal", to_str<FormatTarget::HTML>(*it, ind_1h));

      mem_str += s;
      if (n++ == 4)
        break;
    }

    body += std::format(index_signal_template,                    //
                        symbol,                                   //
                        to_str<FormatTarget::HTML>(sig, ticker),  //
                        mem_str);
  }

  auto datetime = std::format("{:%a, %b %d, %H:%M}", p.last_updated);
  auto subtitle = std::format(index_subtitle_template, datetime);
  auto reload = (config.replay_en && config.debug_en) ? index_reload : "";

  return std::format(index_template, reload, subtitle, body);
}

void Portfolio::write_index() const {
  [[maybe_unused]] auto backup = []() {
    namespace fs = std::filesystem;
    if (fs::exists("page/public/index.html"))
      fs::copy_file(
          "page/public/index.html",
          std::format("page/backup/index_{:%F_%T}.html", now_ny_time()),
          fs::copy_options::overwrite_existing);
  };

  std::thread([=, this]() {
    auto _ = reader_lock();

    // if (!config.replay_en) // TODO
    //   backup();

    auto fn = config.replay_en ? "page/public/index_replay.html"
                               : "page/public/index.html";
    std::ofstream f(fn);
    f << to_str<FormatTarget::HTML>(*this);
    f.flush();
    f.close();
  }).detach();
}
