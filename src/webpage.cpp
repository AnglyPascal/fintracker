#include "format.h"
#include "html_template.h"
#include "portfolio.h"

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

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
  return std::format("{}: sw={:.2f}, atr={:.2f}",
                     sl.is_trailing ? "Trail" : "Stop", sl.swing_low,
                     sl.atr_stop);
}

template <>
std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  for (auto& [symbol, ticker] : p.tickers) {
    auto& m = ticker.metrics;
    auto& ind = m.indicators_1h;

    auto row_class = html_row_class(ticker.signal.type);

    double last_price = m.last_price();
    double ema9 = ind.ema9.values.empty() ? 0.0 : ind.ema9.values.back();
    double ema21 = ind.ema21.values.empty() ? 0.0 : ind.ema21.values.back();
    double rsi = ind.rsi.values.empty() ? 0.0 : ind.rsi.values.back();

    auto pos_str = ticker.has_position() ? ticker.pos_to_str(false) : "";
    auto stop_loss_str = to_str<FormatTarget::HTML>(m.stop_loss);

    auto& conf = ticker.signal.confirmations;
    auto confirmation = ticker.signal.type == SignalType::Entry
                            ? ": " + join(conf.begin(), conf.end())
                            : "";

    body += std::format(
        html_row_template,  //
        row_class, symbol, symbol, to_str(ticker.signal.type) + confirmation,
        symbol, symbol, last_price, ema9, ema21, rsi, pos_str, stop_loss_str);

    body += std::format(html_signal_template,  //
                        symbol,                //
                        to_str<FormatTarget::SignalEntry>(ticker.signal),
                        to_str<FormatTarget::SignalExit>(ticker.signal));
  }

  auto datetime = std::format("{:%b %d, %H:%M}", current_datetime());
  return std::format(html_template, datetime, body);
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
  auto td = std::thread(
      [](auto portfolio) {
        auto lock = portfolio->reader_lock();

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

        for (auto& [symbol, _] : portfolio->tickers) {
          auto& all_trades = portfolio->get_trades();

          std::ofstream f("page/trades.csv");
          f << "datetime,name,action,qty,price,fees\n";
          if (auto it = all_trades.find(symbol); it != all_trades.end()) {
            for (auto& trade : it->second)
              f << to_str(trade) << std::endl;
          }
          f.flush();
          f.close();

          auto& tickers = portfolio->tickers;
          if (auto it = tickers.find(symbol); it != tickers.end())
            it->second.metrics.plot();
        }
      },
      this  //
  );

  td.detach();
}
