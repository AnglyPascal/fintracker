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

inline std::string closest_nyse_aligned_time(const std::string& ny_time_str) {
  using namespace std::chrono;

  // Parse input
  std::istringstream ss(ny_time_str);
  local_time<seconds> input_local;
  ss >> parse("%Y-%m-%d %H:%M:%S", input_local);
  if (ss.fail())
    return "Invalid datetime";

  zoned_time ny_time{"America/New_York", input_local};

  auto day_start = floor<days>(ny_time.get_local_time());
  year_month_day ymd{day_start};
  weekday wd{day_start};
  if (wd == Saturday || wd == Sunday)
    return "Market closed";

  // Market open = 09:30, last valid aligned time = 15:30
  local_time<seconds> market_open = day_start + hours{9} + minutes{30};
  local_time<seconds> last_slot = day_start + hours{15} + minutes{30};

  // Clamp before open
  if (input_local <= market_open)
    return std::format("{:%Y-%m-%d %H:%M:%S}",
                       zoned_time{"America/New_York", market_open});

  // Clamp after last slot
  if (input_local >= day_start + hours{16})
    return std::format("{:%Y-%m-%d %H:%M:%S}",
                       zoned_time{"America/New_York", last_slot});

  // Round to closest 1h-aligned slot from 09:30
  auto delta = duration_cast<seconds>(input_local - market_open);
  int slot =
      static_cast<int>((delta.count() + 1800) / 3600);  // round to nearest hour
  auto aligned = market_open + hours{slot};

  // Final safety clamp
  if (aligned > last_slot)
    aligned = last_slot;

  return std::format("{:%Y-%m-%d %H:%M:%S}",
                     zoned_time{"America/New_York", aligned});
}

template <>
std::string to_str(const Trade& t) {
  return std::format("{},{},{},{:.2f},{:.2f}",                                //
                     closest_nyse_aligned_time(t.date), t.ticker,             //
                     (t.action == Action::BUY ? "BUY" : "SELL"),              //
                     double(t.qty) / FLOAT_SCALE, double(t.px) / FLOAT_SCALE  //
  );
}

template <>
std::string to_str(const Trades& t) {
  std::string str = "Trades: \n";
  for (auto& [symbol, trades] : t) {
    str += std::format("\n• {}\n", symbol);
    for (auto& trade : trades) {
      str += std::format("  - {}\n", to_str(trade));
    }
  }
  return str;
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
      return "Mixed";
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
    auto reason_raw = join(reasons, to_str<Reason>, ", ");
    auto reason_str = reason_raw == "" ? "" : " (" + reason_raw + ")";

    result += std::format("   Signal: {}{}", type_str, reason_str);
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
  auto stop_line = to_str<FormatTarget::Telegram>(metrics.stop_loss);
  auto signal_str = to_str<FormatTarget::Telegram>(signal);

  return std::format(
      "{} *{}*{}\n"                             //
      "{}"                                      //
      "{}"                                      //
      "{}\n",                                   //
      signal.emoji(), symbol, pos_line,         //
      metrics_str,                              //
      stop_line == "" ? "" : stop_line + "\n",  //
      signal_str                                //
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

std::string Ticker::pos_to_str(bool tg) const {
  if (position == nullptr || position->qty <= 0)
    return "";

  auto price = metrics.last_price();
  auto pnl = position->pnl(price);
  auto pct = position->pct(price);

  auto pnl_str = std::format("{:+.2f} ({:+.2f}%)", pnl, pct);

  auto pos_str = to_str(*position);
  return tg ? std::format(" | {}, {}", pos_str, pnl_str)
            : std::format("{}, {}\n", pos_str, pnl_str);
}

template <>
std::string to_str<FormatTarget::Telegram>(const OpenPositions& open,
                                           const Tickers& tickers) {
  auto& positions = open.get_positions();

  if (positions.empty())
    return "No positions open";

  std::ostringstream msg;
  msg << "🗿 *Open Positions*\n\n";

  for (auto [symbol, position] : positions) {
    auto it = tickers.find(symbol);
    double price = it == tickers.end() ? 0.0 : it->second.metrics.last_price();
    msg << position_for_tg_str(symbol, position, position.pnl(price));
  }

  return msg.str();
}

template <>
std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  for (const auto& [symbol, ticker] : p.tickers) {
    if (ticker.signal.type == SignalType::Skip)
      continue;

    const auto& m = ticker.metrics;
    const auto& ind = m.indicators_1h;

    auto row_class = html_row_class(ticker.signal.type);

    double last_price = m.last_price();
    double ema9 = ind.ema9.values.empty() ? 0.0 : ind.ema9.values.back();
    double ema21 = ind.ema21.values.empty() ? 0.0 : ind.ema21.values.back();
    double rsi = ind.rsi.values.empty() ? 0.0 : ind.rsi.values.back();

    auto pos_str = ticker.has_position() ? ticker.pos_to_str(false) : "";
    auto stop_loss_str = to_str<FormatTarget::HTML>(m.stop_loss);

    body += std::format(html_row_template,  //
                        row_class, symbol, symbol, to_str(ticker.signal.type),
                        symbol, symbol, last_price, ema9, ema21, rsi, pos_str,
                        stop_loss_str);

    body += std::format(html_signal_template,  //
                        symbol,                //
                        to_str<FormatTarget::SignalEntry>(ticker.signal),
                        to_str<FormatTarget::SignalExit>(ticker.signal));
  }

  return std::format(html_template, body);
}
