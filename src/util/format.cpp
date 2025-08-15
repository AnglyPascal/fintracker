#include "util/format.h"
#include "core/portfolio.h"
#include "util/times.h"

template <>
std::string to_str(const SysTimePoint& datetime) {
  return std::format("{:%F %T}", datetime);
}

template <>
std::string to_str(const LocalTimePoint& datetime) {
  return std::format("{:%F %T}", datetime);
}

template <>
std::string to_str<FormatTarget::Telegram>(const std::string& lang,
                                           const std::string& str) {
  return std::format("```{}\n{}\n```", lang, str);
}

template <>
std::string to_str(const Candle& candle) {
  auto& [_, open, high, low, close, volume] = candle;
  return std::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {}",  //
                     candle.time(), open, high, low, close, volume);
}

template <>
std::string to_str<FormatTarget::Telegram>(
    const Trade& t,
    const std::pair<const Position*, double>& p) {
  auto [net_pos, pnl] = p;
  if (t.action == Action::BUY)
    return std::format("+ Bought: {} {:.2f} @ {:.2f}", t.ticker, t.qty, t.px);
  else if (net_pos != nullptr)
    return std::format("- Sold: {} {:.2f} @ {:.2f}", t.ticker, t.qty, t.px);
  else
    return std::format("{} Closed: {} {:.2f} @ {:.2f}, {:+.2f}",
                       pnl > 0 ? "+" : "-", t.ticker, t.qty, t.px, pnl);
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
std::string to_str(const std::string& str) {
  return str;
}

template <>
std::string to_str(const Confirmation& h) {
  return h.str;
}

template <>
std::string to_str(const Reason& r) {
  return r.str();
}

template <>
std::string to_str(const Hint& h) {
  return h.str();
}

template <>
std::string to_str(const Rating& type) {
  switch (type) {
    case Rating::Entry:
      return "Entry";
    case Rating::Exit:
      return "Exit";
    case Rating::Watchlist:
      return "Watch";
    case Rating::OldWatchlist:
      return "Watch";
    case Rating::Caution:
      return "Caution";
    case Rating::HoldCautiously:
      return "Hold";
    case Rating::Mixed:
      return "Mixed";
    case Rating::Skip:
      return "Skip";
    default:
      return "None";
  }
}

template <>
std::string to_str(const Recommendation& recom) {
  switch (recom) {
    case Recommendation::StrongBuy:
      return "Strong Buy";
    case Recommendation::Buy:
      return "Buy";
    case Recommendation::WeakBuy:
      return "Weak Buy";
    case Recommendation::Caution:
      return "Caution";
    case Recommendation::Avoid:
      return "Avoid";
    default:
      return "";
  }
}

template <>
std::string to_str(const TimeframeRisk& risk) {
  return std::format("Vol: {:.2f}%, Trend: {:.2f}%",  //
                     risk.volatility_score, risk.trend_alignment * 100);
}

template <>
std::string to_str<FormatTarget::Telegram>(const Signal& sig) {
  if (!sig.has_reasons() && !sig.has_hints())
    return "";

  auto list = [](auto& lst) {
    std::string entry, exit;
    for (auto& l : lst) {
      if (l.cls() == SignalClass::None)
        continue;

      if (l.cls() == SignalClass::Entry)
        entry += std::format("  + {}\n", to_str(l));
      else
        exit += std::format("  - {}\n", to_str(l));
    }
    return entry + exit;
  };

  auto reason_str = list(sig.reasons);
  if (reason_str != "")
    reason_str = "# Reasons:\n" + reason_str + "\n";

  auto hint_str = list(sig.hints);
  if (hint_str != "")
    hint_str = "# Hints:\n" + hint_str + "\n";

  return std::format("{}{}", reason_str, hint_str);
}

std::string emoji(Rating type) {
  switch (type) {
    case Rating::Entry:
      return "[B]";
    case Rating::Exit:
      return "[S]";
    case Rating::Watchlist:
      return "[W]";
    case Rating::Caution:
      return "[C]";
    case Rating::HoldCautiously:
      return "[H]";
    case Rating::Mixed:
      return "[M]";
    default:
      return "[-]";
  }
}

template <>
std::string to_str<FormatTarget::Telegram>(const Position* const& pos,
                                           const double& price) {
  if (pos == nullptr || pos->qty == 0)
    return "";
  return std::format("# {}, {:+.2f} ({:+.2f}%)", to_str(*pos), pos->pnl(price),
                     pos->pct(price));
}

template <>
std::string to_str<FormatTarget::Telegram>(const Metrics& m) {
  auto& ind = m.ind_1h;
  return std::format("Px: {:.2f} | EMA9/EMA21: {:.2f}/{:.2f} | RSI: {:.2f}\n",
                     ind.price(-1), ind.ema9(-1), ind.ema21(-1), ind.rsi(-1));
}

template <>
std::string to_str<FormatTarget::Telegram>(const Ticker& ticker) {
  auto& symbol = ticker.symbol;
  auto& metrics = ticker.metrics;

  auto& ind = metrics.ind_1h;
  auto& signal = ticker.signal;

  auto stop_line =
      metrics.has_position()
          ? std::format("Stop: {:.2f}\n", ticker.stop_loss.final_stop)
          : "";

  auto pos_line =
      to_str<FormatTarget::Telegram>(metrics.position, metrics.last_price());

  return std::format(
      "{} \"{}\" {}\n\n"  //
      "{}"                //
      "{}\n"
      "{}\n",                                   //
      emoji(signal.type), symbol, pos_line,     //
      to_str<FormatTarget::Telegram>(metrics),  //
      stop_line,
      to_str<FormatTarget::Telegram>(ind.signal)  //
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

    auto [qty, px, cost, _, _] = position;
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
    if (signal.type == Rating::Exit)
      exit += symbol + "  ";
    else if (signal.type == Rating::Entry)
      entry += symbol + "  ";
    else if (signal.type == Rating::HoldCautiously)
      caution += symbol + "  ";
    else if (signal.type == Rating::Watchlist)
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

  auto header =
      std::format("-- Status Update {:%F %T}\n", portfolio.last_updated);
  return header + signal_str + positions;
}

template <>
std::string to_str<FormatTarget::Alert>(const Signal& a, const Signal& b) {
  std::string output;

  if (a.type != b.type)
    output += std::format("{} -> {}\n", emoji(a.type), emoji(b.type));

  auto compare = []<typename T>(const std::vector<T>& old_r,
                                const std::vector<T>& new_r, SignalClass cls,
                                auto& label) {
    std::set<T> old_set{old_r.begin(), old_r.end()};
    std::set<T> new_set{new_r.begin(), new_r.end()};

    std::vector<std::string> added, removed;
    for (auto& t : new_set)
      if (t.cls() == cls && t.severity() >= Severity::High && !old_set.count(t))
        added.push_back(to_str(t));
    for (auto& t : old_set)
      if (t.cls() == cls && t.severity() >= Severity::High && !new_set.count(t))
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

  output += compare(a.reasons, b.reasons, SignalClass::Entry, "▲");
  output += compare(a.reasons, b.reasons, SignalClass::Exit, "▼");
  output += compare(a.hints, b.hints, SignalClass::Entry, "△");
  output += compare(a.hints, b.hints, SignalClass::Exit, "▽");

  return output.empty() ? "" : output;
}

inline bool is_interesting(const Ticker& ticker, const Signal& prev_signal) {
  if (ticker.metrics.has_position())
    return true;
  auto& sig = ticker.metrics.ind_1h.signal;
  return sig.is_interesting() || prev_signal.is_interesting();
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

    msg += std::format(
        "{}: {}\n", symbol,
        to_str<FormatTarget::Alert>(prev_signal, ticker.metrics.ind_1h.signal));
  }

  return msg;
}

template <>
std::string to_str(const LinearRegression& lr) {
  return std::format("y = {:.2f} * x + {:.2f}", lr.slope, lr.intercept);
}

template <>
std::string to_str(const TrendLine& tl) {
  std::ostringstream oss;
  oss << "Period: " << tl.period << ", Slope: " << tl.line.slope
      << ", R²: " << tl.r2;
  return oss.str();
}
