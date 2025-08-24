#include "core/portfolio.h"
#include "gen_html_template.h"
#include "util/format.h"

#include <filesystem>
#include <fstream>
#include <thread>

inline constexpr std::string_view index_subtitle_template = R"(
    <div id="title">Portfolio Overview</div>
    <div id="subtitle">
      <div class="update-block">
        <b>Updated</b>: {}
      </div>
      <div class="update-block">
        <b>Last candle</b>: {}
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
      <div class="flex-between">
        <a href="{0}.html" target="_blank">{4}</a>{5}
      </div>
    </td>
    <td class="price-th" data-label="Price"><div>{6}</div></td>
    <td class="ema-th" data-label="EMA"><div>{7}</div></td>
    <td class="rsi-th" data-label="RSI"><div>{8}</div></td>
    <td class="rsi-th" data-label="MACD"><div>{9}</div></td>
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
          <tr class="meta-tr">
            <td>{}</td>
            <td colspan="3">{}</td> 
          </tr>
          <tr class="combined-signal-tr">{}</tr>
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
inline std::string to_str<FormatTarget::HTML>(
    const Metrics& m,
    const StopLoss& stop_loss,
    const ProfitTarget& profit_target)  //
{
  auto price =
      std::format("<div class=\"price\"><b>{:.2f}</b></div>", m.last_price());

  std::string str = price;
  if (m.has_position())
    str += to_str(stop_loss) + to_str(profit_target);
  return std::format("<div class=\"px-sl-pt\">{}</div>", str);
}

template <>
inline std::string to_str<FormatTarget::HTML>(const std::string& sym,
                                              const Event& ev)  //
{
  return std::format(index_event_template, sym, to_str(ev));
}

inline constexpr std::string_view signal_overview_template = R"(
  <div class="overview {}">
  <ul>
    <li><div class="forecast">{}</div></li>
    <li><div class="position_sizing">{}</div></li>
    <li><div class="stop_loss">{}</div></li>
    <li><div class="profit_target">{}</div></li>
  </ul>
  </div>
)";

inline constexpr std::string_view combined_signal_template = R"(
  <td class="trend">{}<div class="trend-list">{}</div></td>
  <td class="curr_signal signal-1h">{}</td>
  <td class="signal-4h">{}</td>
  <td class="signal-1d">{}</td>
)";

template <>
inline std::string to_str<FormatTarget::HTML>(const CombinedSignal& s,
                                              const Ticker& ticker) {
  auto& ind_1h = ticker.metrics.ind_1h;
  auto& sig_1h = ind_1h.signal;

  auto& ind_4h = ticker.metrics.ind_4h;
  auto& sig_4h = ind_4h.signal;

  auto& ind_1d = ticker.metrics.ind_1d;
  auto& sig_1d = ind_1d.signal;

  return std::format(                              //
      combined_signal_template,                    //
      to_str<FormatTarget::HTML>(s),               //
      to_str<FormatTarget::HTML>(s.filters),       //
      to_str<FormatTarget::HTML>(sig_1h, ind_1h),  //
      to_str<FormatTarget::HTML>(sig_4h, ind_4h),  //
      to_str<FormatTarget::HTML>(sig_1d, ind_1d)   //
  );
}

inline constexpr std::string_view rational_template = R"(
  <div class="rationale {}">
  <ul>
    <li><div class="rationale-sig">{}</div></li>
    <li><div class="rationale-size">{}</div></li>
    <li><div class="rationale-stop">{}</div></li>
    <li><div class="rationale-target">{}</div></li>
  </ul>
  </div>
)";

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Source& src) {
  std::string interesting;
  auto add = [&interesting, src](auto& iter) {
    for (auto& a : iter) {
      if (a.source() != src)
        continue;
      if (a.severity() >= Severity::High && a.score >= 0.75) {
        interesting += " " + to_str<FormatTarget::HTML>(a);
      }
    }
  };

  add(s.reasons);
  add(s.hints);

  return interesting;
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

    body += std::format(                                                      //
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
        to_str<FormatTarget::HTML>(m, ticker.stop_loss,                       //
                                   ticker.profit_target)                      //
    );

    auto overview = std::format(                             //
        signal_overview_template,                            //
        m.has_position() ? "" : "no-position",               //
        to_str<FormatTarget::HTML>(sig.forecast),            //
        to_str<FormatTarget::HTML>(ticker.position_sizing),  //
        to_str<FormatTarget::HTML>(ticker.stop_loss),        //
        to_str<FormatTarget::HTML>(ticker.profit_target)     //
    );

    auto& sl = ticker.stop_loss;
    auto& sizing = ticker.position_sizing;
    auto& target = ticker.profit_target;

    auto rationale = std::format(                        //
        rational_template,                               //
        m.has_position() ? "" : "no-position",           //
        sig.rationale != "" ? sig.rationale : "--",      //
        sizing.rationale != "" ? sizing.rationale : "",  //
        sl.rationale != "" ? sl.rationale : "",          //
        target.rationale != "" ? target.rationale : ""   //
    );

    body += std::format(                         //
        index_signal_template,                   //
        symbol,                                  //
        overview,                                //
        rationale,                               //
        to_str<FormatTarget::HTML>(sig, ticker)  //
    );
  }

  auto last_updated = std::format("{:%a, %b %d, %H:%M}", p.last_updated);
  auto last_candle = std::format("{:%a, %b %d, %H:%M}", p.last_candle_time());
  auto subtitle =
      std::format(index_subtitle_template, last_updated, last_candle);

  return std::format(index_template, subtitle, body);
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
    auto fn = config.replay_en ? "page/public/index_replay.html"
                               : "page/public/index.html";
    std::ofstream f{fn};
    f << to_str<FormatTarget::HTML>(*this);
    f.flush();
    f.close();
  }).detach();
}
