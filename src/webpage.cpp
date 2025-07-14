#include "format.h"
#include "html_template.h"
#include "portfolio.h"
#include "times.h"

#include <filesystem>
#include <fstream>
#include <thread>

inline void trendlines_to_csv(auto& candles, auto& trend_line, auto fname) {
  std::ofstream f(fname);

  f << "datetime,value\n";
  for (size_t i = candles.size() - trend_line.period; i < candles.size(); i++)
    f << std::format("{},{:.2f}\n", candles[i].datetime, trend_line.eval(i));

  f.flush();
  f.close();
}

void Indicators::plot(const std::string& symbol) const {
  std::ofstream f("page/data.csv");
  f << "datetime,open,close,high,low,volume,ema9,ema21,rsi,macd,signal\n";

  for (size_t i = 0; i < candles.size(); i++)
    f << std::format(
        "{},{:.2f},{:.2f},{:.2f},{:.2f},{},"
        "{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
        candles[i].datetime, candles[i].open, candles[i].close, candles[i].high,
        candles[i].low, candles[i].volume, ema9.values[i], ema21.values[i],
        rsi.values[i], macd.macd_line[i], macd.signal_line[i]);

  f.flush();
  f.close();

  if (!trends.price.top_trends.empty()) {
    auto& top_trends = trends.price.top_trends;
    for (size_t i = 0; i < top_trends.size(); i++)
      trendlines_to_csv(candles, top_trends[i],
                        std::format("page/price_fit_{}.csv", i));
  }

  if (!trends.rsi.top_trends.empty()) {
    auto& top_trends = trends.rsi.top_trends;
    for (size_t i = 0; i < top_trends.size(); i++)
      trendlines_to_csv(candles, top_trends[i],
                        std::format("page/rsi_fit_{}.csv", i));
  }

  std::this_thread::sleep_for(milliseconds(100));

  std::string cmd = "python3 scripts/plot_metrics.py " + symbol;
  std::system(cmd.c_str());
}

void Metrics::plot() const {
  indicators_1h.plot(symbol);
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
  auto& conf = s.confirmations;
  auto confirmation = s.type == SignalType::Entry
                          ? std::format(" ({})", join(conf.begin(), conf.end()))
                          : "";

  std::string interesting;
  auto add = [&interesting](auto& iter) {
    for (auto& a : iter)
      if (a.severity >= Severity::High) {
        interesting += " " + to_str(a);
      }
  };

  if (s.type == SignalType::Entry || s.type == SignalType::Watchlist ||
      s.type == SignalType::Mixed) {
    add(s.entry_reasons);
    add(s.entry_hints);
  }

  if (s.type == SignalType::Exit || s.type == SignalType::HoldCautiously ||
      s.type == SignalType::Mixed) {
    add(s.exit_reasons);
    add(s.exit_hints);
  }

  if (interesting == "")
    return to_str(s.type) + confirmation;

  return to_str(s.type) + ": " + interesting + confirmation;
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Source& src) {
  std::string interesting;
  auto add = [&interesting, &src](auto& iter) {
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
  std::ostringstream out;

  if (!s.exit_reasons.empty()) {
    out << "<div><b>Exit Reasons:</b><ul>";
    for (const auto& r : s.exit_reasons)
      out << std::format("<li>{}</li>", to_str(r));
    out << "</ul></div>";
  }

  if (!s.exit_hints.empty()) {
    out << "<div><b>Exit Hints:</b><ul>";
    for (const auto& r : s.exit_hints)
      out << std::format("<li>{}</li>", to_str(r));
    out << "</ul></div>";
  }

  return out.str();
}

template <>
std::string to_str<FormatTarget::SignalEntry>(const Signal& s) {
  std::ostringstream out;

  if (!s.entry_reasons.empty()) {
    out << "<div><b>Entry Reasons:</b><ul>";
    for (const auto& r : s.entry_reasons)
      out << std::format("<li>{}</li>", to_str(r));
    out << "</ul></div>";
  }

  if (!s.entry_hints.empty()) {
    out << "<div><b>Entry Hints:</b><ul>";
    for (const auto& r : s.entry_hints)
      out << std::format("<li>{}</li>", to_str(r));
    out << "</ul></div>";
  }

  return out.str();
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
  return std::format("{}, {:+.2f} ({:+.2f}%)", to_str(*pos), pos->pnl(price),
                     pos->pct(price));
}

template <>
std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  for (auto& [symbol, ticker] : p.tickers) {
    auto& m = ticker.metrics;

    auto row_class = html_row_class(ticker.signal.type);

    auto ema = to_str<FormatTarget::HTML>(ticker.signal, Source::EMA);
    auto rsi = to_str<FormatTarget::HTML>(ticker.signal, Source::RSI);
    auto macd = to_str<FormatTarget::HTML>(ticker.signal, Source::MACD);
    auto price = to_str<FormatTarget::HTML>(ticker.signal, Source::Price);
    auto stop = to_str<FormatTarget::HTML>(ticker.signal, Source::Stop);

    bool has_position = ticker.metrics.has_position();
    auto pos_str = to_str<FormatTarget::HTML>(m.position, m.last_price());
    auto stop_loss_str =
        has_position ? std::format("<b>{:.2f}</b>, {}", m.last_price(),
                                   to_str<FormatTarget::HTML>(m.stop_loss))
                     : "";

    auto hide = [&]() {
      if (m.has_position())
        return false;
      auto type = ticker.signal.type;
      return (type == SignalType::Caution || type == SignalType::Mixed ||
              type == SignalType::None);
    };

    auto event = p.calendar.next_event(symbol);
    auto event_str = std::format(html_event_template, symbol, to_str(event));

    body +=
        std::format(html_row_template,  //
                    row_class, symbol, hide() ? "display:none;" : "", symbol,
                    to_str(ticker.signal.type),  //
                    symbol, symbol, event_str,   //
                    price + stop, ema, rsi, macd, pos_str, stop_loss_str);

    body += std::format(html_signal_template,  //
                        symbol,                //
                        to_str<FormatTarget::SignalEntry>(ticker.signal),
                        to_str<FormatTarget::SignalExit>(ticker.signal));
  }

  auto datetime = std::format("{:%b %d, %H:%M}", p.last_updated());
  auto subtitle = std::format(html_subtitle_template, datetime);
  auto reload = (p.config.backtest_en) ? html_reload : "";

  return std::format(html_template, reload, subtitle, body);
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

void Portfolio::plot(const std::string& symbol) const {
  if (!config.plot_en)
    return;

  auto print_trades = [](auto& all_trades, auto& sym) {
    std::ofstream f("page/trades.csv");
    f << "datetime,name,action,qty,price,fees\n";
    if (auto it = all_trades.find(sym); it != all_trades.end()) {
      for (auto& trade : it->second)
        f << to_str(trade) << std::endl;
    }
    f.flush();
    f.close();
  };

  auto& all_trades = get_trades();

  if (symbol != "") {
    print_trades(all_trades, symbol);
    if (auto it = tickers.find(symbol); it != tickers.end())
      it->second.metrics.plot();
  } else {
    for (auto& [sym, ticker] : tickers) {
      print_trades(all_trades, sym);
      ticker.metrics.plot();
    }
  }
}

void Portfolio::write_page(const std::string& symbol) const {
  auto td = std::thread(
      [](auto portfolio, auto symbol) {
        auto _ = portfolio->reader_lock();

        {
          namespace fs = std::filesystem;
          if (fs::exists("page/index.html")) {
            auto datetime = std::format("{:%F_%T}", now_ny_time());
            auto new_fname = std::format("page/backup/index_{}.html", datetime);
            fs::copy_file("page/index.html", new_fname,
                          fs::copy_options::overwrite_existing);
          }
        }

        {
          std::ofstream file("page/index.html");
          file << to_str<FormatTarget::HTML>(*portfolio);
          file.close();
        }

        {
          std::ofstream file("page/trades.html");
          file << to_str<FormatTarget::HTML>(portfolio->get_trades());
          file.close();
        }

        portfolio->plot(symbol);
      },
      this, symbol  //
  );

  td.detach();
}
