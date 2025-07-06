#include "format.h"
#include "html_template.h"
#include "portfolio.h"

#include <format>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

template <>
std::string to_str(const Candle& candle) {
  auto& [datetime, open, high, low, close, volume] = candle;
  return std::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {}", datetime, open, high,
                     low, close, volume);
}

template <>
std::string to_str(const Trade& t) {
  return std::format("{:<10} {:<6} {:<4} {:>6.2f} @ {:>6.2f}",                //
                     t.date, t.ticker,                                        //
                     (t.action == Action::BUY ? "BUY" : "SELL"),              //
                     double(t.qty) / FLOAT_SCALE, double(t.px) / FLOAT_SCALE  //
  );
}

template <>
std::string to_str(const Position& pos) {
  return std::format("{:.2f} @ {:.2f}", pos.price(), pos.quantity());
}

template <>
std::string to_str(const EMA& ema) {
  return ema.values.empty() ? "--" : std::format("{:.2f}", ema.values.back());
}

template <>
std::string to_str(const RSI& rsi) {
  return rsi.values.empty() ? "--" : std::format("{:.2f}", rsi.values.back());
}

template <>
std::string to_str<FormatTarget::Telegram>(const Metrics& metrics) {
  const auto& ind = metrics.indicators_1h;

  auto ema_str = std::format("{}/{}", to_str(ind.ema9), to_str(ind.ema21));
  auto rsi_str = to_str(ind.rsi);
  auto [high, pb] = metrics.pullback();
  auto pb_str = std::format("{:.2f} ({:.2f}%)", high, pb);

  return std::format(
      "   Px: {:.2f} | EMA 9/21: {}\n   RSI: {} | Pullback: {}\n",
      metrics.last_price(), ema_str, rsi_str, pb_str);
}

template <>
std::string to_str<FormatTarget::Console>(const Metrics& metrics) {
  const auto& ind = metrics.indicators_1h;

  auto ema_str = std::format("{}/{}", to_str(ind.ema9), to_str(ind.ema21));
  auto rsi_str = to_str(ind.rsi);
  auto [high, pb] = metrics.pullback();
  auto pb_str = std::format("{:.2f} ({:.2f}%)", high, pb);

  return std::format("{:<8}{:<10.2f}{:<17}{:<10}{:<17}", metrics.symbol,
                     metrics.last_price(), ema_str, rsi_str, pb_str);
}

template <>
std::string to_str(const Hint& h) {
  return h.str;
}

template <>
std::string to_str(const Reason& r) {
  switch (r.type) {
    // Entry reasons
    case ReasonType::EmaCrossover:
      return "EMA crossover";
    case ReasonType::RsiCross50:
      return "RSI crossed 50";
    case ReasonType::PullbackBounce:
      return "Pullback bounce";
    case ReasonType::MacdHistogramCross:
      return "MACD histogram cross";

    // Exit reasons
    case ReasonType::EmaCrossdown:
      return "EMA crossdown";
    case ReasonType::RsiOverbought:
      return "RSI overbought";
    case ReasonType::MacdBearishCross:
      return "MACD bearish cross";
    case ReasonType::TimeExit:
      return "Timed exit";
    case ReasonType::StopLossHit:
      return "Stop loss hit";

    default:
      return "No reason";
  }
}

template <typename T, typename Fn = std::function<std::string(const T&)>>
std::string join(
    const std::vector<T>& items,
    Fn to_str = [](const T& x) { return x; },
    const std::string& sep = ", ") {
  std::string result;
  for (size_t i = 0; i < items.size(); ++i) {
    result += to_str(items[i]);
    if (i + 1 < items.size())
      result += sep;
  }
  return result;
}

template <>
std::string to_str(const SignalType& type) {
  switch (type) {
    case SignalType::Entry:
      return "Buy";
    case SignalType::Exit:
      return "Sell";
    case SignalType::Watchlist:
      return "Watch";
    case SignalType::Caution:
      return "Watch cautiously";
    case SignalType::HoldCautiously:
      return "Hold cautiously";
    case SignalType::Mixed:
      return "Confusing";
    default:
      return "";
  }
}

template <>
std::string to_str<FormatTarget::Telegram>(const Signal& sig) {
  if (!sig.has_signal() && !sig.has_hints())
    return "";

  std::string result;

  // --- Signal line ---
  if (sig.has_signal()) {
    auto type_str = to_str(sig.type);
    auto& reasons =
        sig.type == SignalType::Entry ? sig.entry_reasons : sig.exit_reasons;
    auto reason_str = join(reasons, to_str<Reason>, ", ");

    result += std::format("   Signal: {} ({})", type_str, reason_str);
  } else {
    result += "   Hints: ";
  }

  // --- Hints ---
  std::string entry_str, exit_str;

  if (!sig.entry_hints.empty()) {
    for (const auto& hint : sig.entry_hints)
      entry_str += std::format("\n   + {}", hint.str);
  }

  if (!sig.exit_hints.empty()) {
    for (const auto& hint : sig.exit_hints)
      exit_str += std::format("\n   - {}", hint.str);
  }

  result += entry_str + exit_str;

  return result + "\n";
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s) {
  std::ostringstream out;

  if (!s.entry_reasons.empty()) {
    out << "<div><b>Entry Reasons:</b><ul>";
    for (const auto& r : s.entry_reasons)
      out << std::format("<li>{}</li>", to_str(r));
    out << "</ul></div>";
  }

  if (!s.exit_reasons.empty()) {
    out << "<div><b>Exit Reasons:</b><ul>";
    for (const auto& r : s.exit_reasons)
      out << std::format("<li>{}</li>", to_str(r));
    out << "</ul></div>";
  }

  if (!s.entry_hints.empty()) {
    out << "<div><b>Entry Hints:</b><ul>";
    for (const auto& r : s.entry_hints)
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
std::string to_str<FormatTarget::Console>(const Signal& sig) {
  if (!sig.has_signal() && !sig.has_hints())
    return "";

  std::string result;

  // --- Signal line ---
  if (sig.has_signal()) {
    auto type_str = to_str(sig.type);
    auto& reasons =
        sig.type == SignalType::Entry ? sig.entry_reasons : sig.exit_reasons;
    auto reason_str = join(reasons, to_str<Reason>, ", ");

    result += std::format("Signal: {}{} ({}){}", sig.color(), type_str,
                          reason_str, RESET);
  } else {
    result += "Hints: ";
  }

  // --- Hints ---
  std::string entry_str, exit_str;

  if (!sig.entry_hints.empty()) {
    entry_str = std::format(
        "{}{}{}", GREEN,
        join(sig.entry_hints, [](auto& hint) { return hint.str; }), RESET);
  }

  if (!sig.exit_hints.empty()) {
    exit_str = std::format(
        "{}{}{}", RED,
        join(sig.exit_hints, [](auto& hint) { return hint.str; }), RESET);
  }

  if (!entry_str.empty() || !exit_str.empty())
    result += std::format(                                     //
        "{}{}{}{}",                                            //
        sig.has_signal() ? " | " : "",                         //
        entry_str,                                             //
        !entry_str.empty() && !exit_str.empty() ? " | " : "",  //
        exit_str                                               //
    );

  return result;
}

std::string Signal::color_bg() const {
  if (type == SignalType::Entry)
    return GREEN_BG;
  if (type == SignalType::Exit)
    return RED_BG;
  if (type == SignalType::Watchlist)
    return !entry_reasons.empty() ? GREEN_BG : RED_BG;
  if (type == SignalType::HoldCautiously)
    return RED_BG;
  if (type == SignalType::Mixed)
    return YELLOW_BG;
  return RESET;
}

std::string Signal::color() const {
  if (type == SignalType::Entry)
    return GREEN_BG + BLACK;
  if (type == SignalType::Exit)
    return RED_BG + BLACK;
  if (type == SignalType::Watchlist)
    return GREEN;
  if (type == SignalType::Caution)
    return RED;
  if (type == SignalType::HoldCautiously)
    return RED;
  if (type == SignalType::Mixed)
    return YELLOW;
  return RESET;
}

std::string Signal::emoji() const {
  switch (type) {
    case SignalType::Entry:
      return "💚";
    case SignalType::Exit:
      return "❌";
    case SignalType::Watchlist:
      return "🟡";
    case SignalType::Caution:
      return "🟠";
    case SignalType::HoldCautiously:
      return "⚠️";
    case SignalType::Mixed:
      return "🌀";
    default:
      return "🙈";
  }
}

template <>
std::string to_str<FormatTarget::Telegram>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("   Stop: {:.2f}", sl.final_stop);
}

template <>
std::string to_str<FormatTarget::Console>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("Stops: sw = {:.2f}, ema = {:.2f}, atr = {:.2f}",  //
                     sl.swing_low, sl.ema_stop, sl.atr_stop);
}

template <>
std::string to_str<FormatTarget::HTML>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("sw: {:.2f}, atr: {:.2f}", sl.swing_low, sl.atr_stop);
}

inline std::string position_for_tg_str(const std::string& symbol,
                                       const Position& pos,
                                       double pnl) {
  auto [qty, px, cost] = pos;

  double total = (double)(cost) / COST_SCALE;
  double gain = (total != 0) ? pnl / std::abs(total) * 100.0 : 0.0;
  auto pnl_str = std::format(" | PnL: {:+.2f} ({:+.2f}%)\n", pnl, gain);

  return std::format("- {:<7}{}{}{}\n", symbol, (qty > 0 ? " +" : " "),
                     to_str(pos), (qty > 0 ? "" : pnl_str));
}

template <>
std::string to_str<FormatTarget::Telegram>(const Ticker& ticker) {
  auto& [symbol, p, t, metrics, signal, position] = ticker;

  auto metrics_str = to_str<FormatTarget::Telegram>(metrics);
  auto pos_line = ticker.pos_to_str(true);
  auto stop_line = to_str<FormatTarget::Telegram>(metrics.stop_loss) + "\n";
  auto signal_str = to_str<FormatTarget::Telegram>(signal);

  return std::format(
      "{} *{}*{}\n"                      //
      "{}"                               //
      "{}"                               //
      "{}\n",                            //
      signal.emoji(), symbol, pos_line,  //
      metrics_str,                       //
      stop_line,                         //
      signal_str                         //
  );
}

template <>
std::string to_str<FormatTarget::Alert>(const Ticker& ticker) {
  auto& [symbol, p, t, metrics, signal, position] = ticker;

  auto pos_line = ticker.pos_to_str(true);
  auto signal_str = to_str<FormatTarget::Telegram>(signal);

  return std::format(
      "{} *{}*{}\n"                      //
      "{}\n",                            //
      signal.emoji(), symbol, pos_line,  //
      signal_str                         //
  );
}

template <>
std::string to_str<FormatTarget::Console>(const Ticker& ticker) {
  auto& [symbol, p, t, metrics, signal, position] = ticker;

  auto metrics_str = to_str<FormatTarget::Console>(metrics);
  auto pos_line = ticker.pos_to_str(false);
  auto stop_line = to_str<FormatTarget::Console>(metrics.stop_loss) + "\n";
  auto signal_str = to_str<FormatTarget::Console>(signal);

  return std::format(
      "{}••{}  "                           //
      "{}{}{}"                             //
      "{}\n"                               //
      "{}",                                //
      signal.color_bg(), RESET,            //
      signal.color(), metrics_str, RESET,  //
      signal_str,                          //
      pos_line                             //
  );
}

std::string Ticker::pos_to_str(bool tg) const {
  if (position == nullptr || position->qty <= 0)
    return "";

  auto price = metrics.last_price();
  auto pnl = position->pnl(price);
  auto pct = position->pct(price);

  auto pnl_str = std::format("{:+.2f} ({:+.2f}%)", pnl, pct);

  auto pos_str = to_str(*position);
  return tg ? std::format(" | {}, {}", pos_str, pnl_str)
            : std::format(                                //
                  "->  {}{}{}, {} | {}\n",                //
                  pnl > 0 ? GREEN : RED, pos_str, RESET,  //
                  pnl_str,                                //
                  has_position()
                      ? ""
                      : to_str<FormatTarget::Telegram>(metrics.stop_loss)  //
              );
}

void Portfolio::console_update() const {
  std::system("clear");
  std::string header = std::format(
      "    "
      "{:<8}"   // Ticker
      "{:<10}"  // Price
      "{:<17}"  // EMA
      "{:<10}"  // RSI
      "{:<17}"  // High (PB)
      "\n",
      "Ticker", "Price",              //
      "EMA 9/21", "RSI", "High (PB)"  //
  );

  std::cout << header << std::string(header.size(), '-') << std::endl;
  for (const auto& [symbol, ticker] : tickers)
    if (ticker.metrics.last_price() != 0)
      std::cout << to_str<FormatTarget::Console>(ticker);
  std::cout << std::endl;
}

std::string OpenPositions::to_str(bool tg) const {
  if (positions.empty())
    return "";

  std::ostringstream msg;
  msg << "🗿 *Open Positions*\n\n";

  for (auto [symbol, position] : positions)
    msg << position_for_tg_str(symbol, position, 0);

  return msg.str();
}

template <>
std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  for (const auto& [symbol, ticker] : p.tickers) {
    const auto& m = ticker.metrics;
    const auto& ind = m.indicators_1h;

    auto row_class = html_row_class(ticker.signal.type);
    auto signal_html = std::format(html_signal_template,  //
                                   to_str(ticker.signal.type),
                                   to_str<FormatTarget::HTML>(ticker.signal));

    double last_price = m.last_price();
    double ema9 = ind.ema9.values.empty() ? 0.0 : ind.ema9.values.back();
    double ema21 = ind.ema21.values.empty() ? 0.0 : ind.ema21.values.back();
    double rsi = ind.rsi.values.empty() ? 0.0 : ind.rsi.values.back();

    auto pos_str = ticker.has_position() ? ticker.pos_to_str(false) : "-";
    auto stop_loss_str = to_str<FormatTarget::HTML>(m.stop_loss);

    body += std::format(html_row_template,  //
                        row_class, signal_html, symbol, last_price, ema9, ema21,
                        rsi, pos_str, stop_loss_str);
  }

  return std::format(html_template, body);
}
