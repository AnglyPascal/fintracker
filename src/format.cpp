#include "format.h"
#include "portfolio.h"
#include "times.h"

#include <functional>
#include <iostream>
#include <unordered_set>

template <>
std::string to_str<FormatTarget::Telegram>(const std::string& lang,
                                           const std::string& str) {
  return std::format("```{}\n{}\n```", lang, str);
}

template <>
std::string to_str(const Candle& candle) {
  auto& [datetime, open, high, low, close, volume] = candle;
  return std::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {}",  //
                     datetime, open, high, low, close, volume);
}

template <>
std::string to_str(const Trade& t) {
  return std::format(                               //
      "{},{},{},{:.2f},{:.2f},{:.2f}",              //
      closest_nyse_aligned_time(t.date), t.ticker,  //
      (t.action == Action::BUY ? "BUY" : "SELL"),   //
      t.qty,                                        //
      t.px,                                         //
      t.total                                       //
  );
}

template <>
std::string to_str<FormatTarget::Telegram>(
    const Trade& t,
    const std::pair<const Position*, double>& p) {
  auto [net_pos, pnl] = p;
  if (t.action == Action::BUY)
    return std::format("+ Bought: {} {} @ {}", t.ticker, t.qty, t.px);
  else if (net_pos != nullptr)
    return std::format("- Sold: {} {} @ {}", t.ticker, t.qty, t.px);
  else
    return std::format("{} Closed: {} {} @ {}, {:+}", pnl > 0 ? "+" : "-",
                       t.ticker, t.qty, t.px, pnl);
}

template <>
std::string to_str<FormatTarget::Telegram>(const Trades& t) {
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
  return std::format("{:.2f} @ {:.2f}", pos.qty, pos.px);
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
std::string to_str(const HintType& h) {
  switch (h) {
    // Entry reasons
    case HintType::Ema9ConvEma21:
      return "ema9↗21";
    case HintType::RsiConv50:
      return "rsi↗50";
    case HintType::MacdRising:
      return "macd↗";

    case HintType::PriceTrendingUp:
      return "price↗";
    case HintType::Ema21TrendingUp:
      return "ema21↗";
    case HintType::RsiTrendingUp:
      return "rsi↗";
    case HintType::RsiTrendingUpStrongly:
      return "rsi⇗";

    // Exit hints
    case HintType::Ema9DivergeEma21:
      return "ema9↘21";
    case HintType::RsiDropFromOverbought:
      return "rsi⭛";
    case HintType::MacdPeaked:
      return "macd▲";
    case HintType::Ema9Flattening:
      return "ema9↝21";
    case HintType::StopProximity:
      return "stop⨯";
    case HintType::StopInATR:
      return "stop!";

    case HintType::PriceTrendingDown:
      return "price↘";
    case HintType::Ema21TrendingDown:
      return "ema21↘";
    case HintType::RsiTrendingDown:
      return "rsi↘";
    case HintType::RsiTrendingDownStrongly:
      return "rsi⇘";

    default:
      return "";
  }
}

template <>
std::string to_str(const Hint& h) {
  return to_str(h.type);
}

template <>
std::string to_str(const Confirmation& h) {
  return h.str;
}

template <>
std::string to_str(const ReasonType& r) {
  switch (r) {
    // Entry reasons
    case ReasonType::EmaCrossover:
      return "ema⤯";
    case ReasonType::RsiCross50:
      return "rsi⤯50";
    case ReasonType::PullbackBounce:
      return "pullback⤴";
    case ReasonType::MacdHistogramCross:
      return "macd⤯";

    // Exit reasons
    case ReasonType::EmaCrossdown:
      return "ema⤰";
    case ReasonType::RsiOverbought:
      return "rsi↱70";
    case ReasonType::MacdBearishCross:
      return "macd⤰";
    // case ReasonType::TimeExit:
    //   return "exit⏲";
    case ReasonType::StopLossHit:
      return "stop⤰";

    default:
      return "";
  }
}

template <>
std::string to_str(const Reason& r) {
  return to_str(r.type);
}

template <>
std::string to_str(const std::string& str) {
  return str;
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
      return "None";
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

    auto reason_str =
        reasons.empty()
            ? ""
            : std::format("({})", join(reasons.begin(), reasons.end(), ", "));

    result += std::format("   Signal: {}{}", type_str, reason_str);
  } else {
    result += "   Hints: ";
  }

  // --- Hints ---
  std::string entry_str, exit_str;

  if (!sig.entry_hints.empty()) {
    for (const auto& hint : sig.entry_hints)
      entry_str += std::format("\n   + {}", to_str(hint));
  }

  if (!sig.exit_hints.empty()) {
    for (const auto& hint : sig.exit_hints)
      exit_str += std::format("\n   - {}", to_str(hint));
  }

  result += entry_str + exit_str;

  return result + "\n";
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
      return "[B]";
    case SignalType::Exit:
      return "[S]";
    case SignalType::Watchlist:
      return "[W]";
    case SignalType::Caution:
      return "[C]";
    case SignalType::HoldCautiously:
      return "[H]";
    case SignalType::Mixed:
      return "[M]";
    default:
      return "[-]";
  }
}

template <>
std::string to_str<FormatTarget::Telegram>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("   Stop: {:.2f}", sl.final_stop);
}

template <>
std::string to_str<FormatTarget::Telegram>(const Position* const& pos,
                                           const double& price) {
  if (pos == nullptr || pos->qty == 0)
    return "";
  return std::format(" | {}, {:+.2f} ({:+.2f}%)", to_str(*pos), pos->pnl(price),
                     pos->pct(price));
}

template <>
std::string to_str<FormatTarget::Telegram>(const Ticker& ticker) {
  auto& [symbol, _, _, metrics, signal, _] = ticker;

  auto pos_line =
      to_str<FormatTarget::Telegram>(metrics.position, metrics.last_price());
  auto stop_line =
      metrics.has_position()
          ? to_str<FormatTarget::Telegram>(metrics.stop_loss) + "\n"
          : "";

  return std::format(
      "{} {}{}\n"                               //
      "{}"                                      //
      "{}"                                      //
      "{}\n",                                   //
      signal.emoji(), symbol, pos_line,         //
      to_str<FormatTarget::Telegram>(metrics),  //
      stop_line,                                //
      to_str<FormatTarget::Telegram>(signal)    //
  );
}

template <>
std::string to_str<FormatTarget::Telegram>(const Positions& positions,
                                           const Portfolio& portfolio) {
  if (positions.empty())
    return "-- No positions open\n";

  std::string msg = "-- Open Positions\n";

  for (auto [symbol, position] : positions) {
    auto ticker = portfolio.get_ticker(symbol);
    double price = ticker == nullptr ? 0.0 : ticker->metrics.last_price();

    auto [qty, px, cost, _] = position;
    auto pnl = position.pnl(price);

    double total = cost;
    double gain = (total != 0) ? pnl / std::abs(total) * 100.0 : 0.0;
    auto pnl_str = std::format(" | PnL: {:+.2f} ({:+.2f}%)\n", pnl, gain);

    msg += std::format("{:<8} {}{}\n", "\"" + symbol + "\"", to_str(position),
                       (qty > 0 ? "" : pnl_str));
  }

  return msg;
}

template <>
std::string to_str<FormatTarget::Telegram>(const Portfolio& portfolio) {
  auto _ = portfolio.reader_lock();

  std::string exit, caution, entry, watch;
  for (auto& [symbol, ticker] : portfolio.tickers) {
    auto& signal = ticker.signal;
    if (signal.type == SignalType::Exit)
      exit += symbol + "  ";
    else if (signal.type == SignalType::Entry)
      entry += symbol + "  ";
    else if (signal.type == SignalType::HoldCautiously)
      caution += symbol + "  ";
    else if (signal.type == SignalType::Watchlist)
      watch += symbol + "  ";
  }

  std::string signal_str;

  if (exit != "")
    signal_str += "🚨 exit :: " + exit + "\n";

  if (caution != "")
    signal_str += "⚠️ hold cautiously :: " + caution + "\n";

  if (entry != "")
    signal_str += "✅ entry :: " + entry + "\n";

  if (watch != "")
    signal_str += "👀 watchlist :: " + watch + "\n";

  if (signal_str == "")
    signal_str = "😴 nothing to report\n";

  signal_str += "\n";

  auto positions =
      to_str<FormatTarget::Telegram>(portfolio.get_positions(), portfolio);

  std::string header = "-- Status Update\n";
  return header + signal_str + positions;
}

template <>
std::string to_str<FormatTarget::Alert>(const Signal& a, const Signal& b) {
  std::string output;

  if (a.type != b.type)
    output += std::format("{} -> {}\n", a.emoji(), b.emoji());

  auto compare = []<typename T>(const std::vector<T>& old_r,
                                const std::vector<T>& new_r, auto& label) {
    std::set<T> old_set{old_r.begin(), old_r.end()};
    std::set<T> new_set{new_r.begin(), new_r.end()};

    std::vector<std::string> added, removed;
    for (auto& t : new_set)
      if (t.severity >= Severity::High && !old_set.count(t))
        added.push_back(to_str(t));
    for (auto& t : old_set)
      if (t.severity >= Severity::High && !new_set.count(t))
        removed.push_back(to_str(t));

    std::string str;
    if (!added.empty()) {
      str += std::format("+ {}: {}\n", label, join(added.begin(), added.end()));
    }
    if (!removed.empty()) {
      str += std::format("- {}: {}\n", label,
                         join(removed.begin(), removed.end()));
    }
    return str;
  };

  output += compare(a.entry_reasons, b.entry_reasons, "▲");
  output += compare(a.exit_reasons, b.exit_reasons, "▼");
  output += compare(a.entry_hints, b.entry_hints, "△");
  output += compare(a.exit_hints, b.exit_hints, "▽");

  return output.empty() ? "" : output;
}

inline bool is_interesting(const Ticker& ticker, const Signal& prev_signal) {
  if (ticker.metrics.has_position())
    return true;
  return ticker.signal.is_interesting() || prev_signal.is_interesting();
}

inline std::vector<std::string> split_lines(const std::string& s) {
  std::vector<std::string> lines;
  std::istringstream stream(s);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  return lines;
}

template <>
std::string to_str<FormatTarget::Alert>(
    const Portfolio& portfolio,
    const std::unordered_map<std::string, Signal>& prev_signals) {
  std::string msg;

  std::vector<std::vector<std::string>> split_blocks;

  for (auto& [symbol, ticker] : portfolio.tickers) {
    auto& prev_signal = prev_signals.at(symbol);

    if (prev_signal.type == ticker.signal.type)
      continue;

    if (!is_interesting(ticker, prev_signal))
      continue;

    msg += std::format("{}: {}\n", symbol,
                       to_str<FormatTarget::Alert>(prev_signal, ticker.signal));
  }

  return msg;
}
