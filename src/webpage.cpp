#include "format.h"
#include "html_template.h"
#include "portfolio.h"
#include "times.h"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <latch>
#include <ranges>
#include <thread>

inline constexpr std::string csv_fname(const std::string& symbol,
                                       const std::string& fn) {
  return std::format("page/src/{}_{}.csv", symbol, fn);
}

void Indicators::plot(const std::string& sym) const {
  std::ofstream f(csv_fname(sym, "data"));
  f << "datetime,open,close,high,low,volume,ema9,ema21,rsi,macd,signal\n";

  for (size_t i = 0; i < candles.size(); i++)
    f << std::format(
        "{},{:.2f},{:.2f},{:.2f},{:.2f},{},"
        "{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
        candles[i].datetime, candles[i].open, candles[i].close, candles[i].high,
        candles[i].low, candles[i].volume, ema9.values[i], ema21.values[i],
        rsi.values[i], macd.macd_line[i], macd.signal_ema.values[i]);

  f.flush();
  f.close();

  auto trends_to_csv = [this, &sym](auto& top_trends, auto& name) {
    for (size_t i = 0; i < top_trends.size(); i++) {
      std::ofstream f(csv_fname(sym, std::format("{}_fit_{}", name, i)));
      f << "datetime,value\n";

      auto& top_trend = top_trends[i];
      for (size_t j = candles.size() - top_trend.period; j < candles.size();
           j++)
        f << std::format("{},{:.2f}\n", candles[j].datetime, top_trend.eval(j));

      f.flush();
      f.close();
    }
  };

  trends_to_csv(trends.price.top_trends, "price");
  trends_to_csv(trends.rsi.top_trends, "rsi");
}

void Metrics::plot(const std::string& sym) const {
  indicators_1h.plot(sym);
}

void Portfolio::write_plot_data(const std::string& symbol) const {
  if (!config.plot_en)
    return;

  auto it = tickers.find(symbol);
  if (it == tickers.end())
    return;

  auto _ = reader_lock();
  spdlog::trace("plotting {}", symbol.c_str());

  std::ofstream f(csv_fname(symbol, "trades"));
  f << "datetime,name,action,qty,price,total\n";

  auto& all_trades = get_trades();
  if (auto it = all_trades.find(symbol); it != all_trades.end()) {
    for (auto& trade : it->second)
      f << to_str(trade) << std::endl;
  }
  f.flush();
  f.close();

  it->second.metrics.plot(symbol);
}

void Portfolio::plot(const std::string& symbol) const {
  if (symbol != "")
    return notify_plot_daemon({symbol});

  auto syms =
      tickers | std::ranges::views::keys | std::ranges::to<std::vector>();
  notify_plot_daemon(syms);
}

/******************************
 * Webpage related formatting *
 ******************************/

template <>
std::string to_str<FormatTarget::HTML>(const Hint& h) {
  if (h.severity >= Severity::Urgent)
    return std::format("<b>{}</b>", to_str(h.type));
  return to_str(h.type);
}

template <>
std::string to_str<FormatTarget::HTML>(const Reason& r) {
  if (r.severity >= Severity::Urgent)
    return std::format("<b>{}</b>", to_str(r.type));
  return to_str(r.type);
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s) {
  if (s.type != SignalType::Entry)
    return to_str(s.type);
  auto& conf = s.confirmations;
  return std::format("{}: ({})", to_str(s.type),
                     join(conf.begin(), conf.end()));
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Source& src) {
  std::string interesting;
  auto add = [&interesting, src](auto& iter) {
    for (auto& a : iter) {
      if (a.src != src)
        continue;
      if (a.severity >= Severity::Medium) {
        interesting += " " + to_str<FormatTarget::HTML>(a);
      }
    }
  };

  add(s.entry_reasons);
  add(s.entry_hints);
  add(s.exit_reasons);
  add(s.exit_hints);

  return interesting;
}

template <>
std::string to_str<FormatTarget::SignalExit>(const Signal& s) {
  return index_reason_list("Exit Reasons", s.exit_reasons) +
         index_reason_list("Exit Hints", s.exit_hints);
}

template <>
std::string to_str<FormatTarget::SignalEntry>(const Signal& s) {
  return index_reason_list("Entry Reasons", s.entry_reasons) +
         index_reason_list("Entry Hints", s.entry_hints);
}

template <>
std::string to_str<FormatTarget::HTML>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("sw={:.2f}, atr={:.2f}", sl.swing_low, sl.atr_stop);
}

template <>
std::string to_str(const Event& ev) {
  if (ev.type == '\0')
    return "";

  auto diff_date = duration_cast<days>(ev.ny_date - now_ny_time()).count();
  return std::format("{}{:+}", ev.type, diff_date);
}

template <>
std::string to_str<FormatTarget::HTML>(const Position* const& pos,
                                       const double& price) {
  if (pos == nullptr || pos->qty == 0)
    return "";
  return std::format("{} ({:+.2f}%)", to_str(*pos), pos->pct(price));
}

inline bool hide(auto& m, auto& type) {
  if (m.has_position())
    return false;
  return (type == SignalType::Caution || type == SignalType::Mixed ||
          type == SignalType::None);
};

template <>
std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  for (auto& [symbol, ticker] : p.tickers) {
    auto& m = ticker.metrics;
    auto& sig = ticker.signal;

    auto row_class = index_row_class(sig.type);

    auto pos_str = to_str<FormatTarget::HTML>(m.position, m.last_price());
    auto stop_loss_str =
        m.has_position() ? std::format("<b>{:.2f}</b>, {}", m.last_price(),
                                       to_str<FormatTarget::HTML>(m.stop_loss))
                         : "";

    auto event = p.calendar.next_event(symbol);
    auto event_str = std::format(index_event_template, symbol, to_str(event));

    auto str = [&](auto src) { return to_str<FormatTarget::HTML>(sig, src); };
    body += std::format(
        index_row_template,  //
        row_class, symbol, hide(m, sig.type) ? "display:none;" : "", symbol,
        to_str<FormatTarget::HTML>(sig),  //
        symbol, symbol, event_str,        //
        str(Source::Price) + str(Source::Stop), str(Source::EMA),
        str(Source::RSI), str(Source::MACD),  //
        pos_str, stop_loss_str                //
    );

    body += std::format(index_signal_template,  //
                        symbol,                 //
                        to_str<FormatTarget::SignalEntry>(sig),
                        to_str<FormatTarget::SignalExit>(sig));
  }

  auto datetime = std::format("{:%b %d, %H:%M}", p.last_updated());
  auto subtitle = std::format(index_subtitle_template, datetime);
  auto reload = (p.config.replay_en) ? index_reload : "";

  return std::format(index_template, reload, subtitle, body);
}

template <>
std::string to_str<FormatTarget::HTML>(const Trades& all_trades) {
  std::vector<Trade> trades;
  for (auto& [_, t] : all_trades)
    trades.insert(trades.end(), t.begin(), t.end());

  std::sort(trades.begin(), trades.end(),
            [](auto& lhs, auto& rhs) { return lhs.date < rhs.date; });

  std::string body;

  for (auto& trade : trades) {
    auto& [date, symbol, action, qty, px, fees] = trade;
    body += std::format(trades_row_template,
                        action == Action::BUY ? "buy" : "sell",  //
                        date, symbol, symbol,                    //
                        action == Action::BUY ? "Buy" : "Sell",  //
                        qty, px, fees);
  }

  return std::format(trades_template, body);
}

void Portfolio::write_page() const {
  std::thread([this]() {
    auto _ = reader_lock();

    namespace fs = std::filesystem;
    if (fs::exists("page/index.html"))
      fs::copy_file(
          "page/index.html",
          std::format("page/backup/index_{:%F_%T}.html", now_ny_time()),
          fs::copy_options::overwrite_existing);

    std::ofstream index("page/index.html");
    index << to_str<FormatTarget::HTML>(*this);
    index.close();

    std::ofstream trades("page/trades.html");
    trades << to_str<FormatTarget::HTML>(get_trades());
    trades.close();
  }).detach();
}
