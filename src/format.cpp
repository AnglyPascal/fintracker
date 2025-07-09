#include "format.h"
#include "portfolio.h"

#include <format>
#include <functional>
#include <iostream>
#include <unordered_set>

template <>
std::string to_str(const Candle& candle) {
  auto& [datetime, open, high, low, close, volume] = candle;
  return std::format("{} {:.2f} {:.2f} {:.2f} {:.2f} {}",  //
                     datetime, open, high, low, close, volume);
}

inline std::string closest_nyse_aligned_time(const std::string& ny_time_str) {
  using namespace std::chrono;

  // Parse input
  std::istringstream ss(ny_time_str);
  local_time<seconds> input_local;
  ss >> parse("%Y-%m-%d %H:%M:%S", input_local);
  if (ss.fail())
    return "1999-01-01 00:00:00";

  zoned_time ny_time{"America/New_York", input_local};

  auto day_start = floor<days>(ny_time.get_local_time());
  year_month_day ymd{day_start};
  weekday wd{day_start};
  if (wd == Saturday || wd == Sunday)
    return "1999-01-01 00:00:00";

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
  auto slot =
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
  return std::format(                               //
      "{},{},{},{:.2f},{:.2f},{:.2f}",              //
      closest_nyse_aligned_time(t.date), t.ticker,  //
      (t.action == Action::BUY ? "BUY" : "SELL"),   //
      t.qty,                                        //
      t.px,                                         //
      t.fees                                        //
  );
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
      return "ema9 ↗ ema21";
    case HintType::RsiConv50:
      return "rsi ↗ 50";
    case HintType::MacdRising:
      return "macd↗";

    case HintType::PriceTrendingUp:
      return "price↗";
    case HintType::Ema21TrendingUp:
      return "ema21↗";
    case HintType::RsiTrendingUp:
      return "rsi↗";
    case HintType::RsiTrendingUpStrongly:
      return "rsi↗!";

    // Exit hints
    case HintType::Ema9DivergeEma21:
      return "ema9 ↘ ema21";
    case HintType::RsiDropFromOverbought:
      return "rsi⭛";
    case HintType::MacdPeaked:
      return "macd⛰";
    case HintType::Ema9Flattening:
      return "ema9 ↝ ema21";
    case HintType::StopProximity:
      return "near stop";
    case HintType::StopInATR:
      return "stop inside atr zone";

    case HintType::PriceTrendingDown:
      return "price↘";
    case HintType::Ema21TrendingDown:
      return "ema21↘";
    case HintType::RsiTrendingDown:
      return "rsi↘";
    case HintType::RsiTrendingDownStrongly:
      return "rsi↘!";

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
      return "ema ⤯";
    case ReasonType::RsiCross50:
      return "rsi ↗ 50";
    case ReasonType::PullbackBounce:
      return "pullback⤴";
    case ReasonType::MacdHistogramCross:
      return "macd ⤯";

    // Exit reasons
    case ReasonType::EmaCrossdown:
      return "ema ⤰";
    case ReasonType::RsiOverbought:
      return "rsi ↱ 70";
    case ReasonType::MacdBearishCross:
      return "macd ⤰";
    case ReasonType::TimeExit:
      return "timed exit";
    case ReasonType::StopLossHit:
      return "stop ⤰";

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
std::string to_str<FormatTarget::Telegram>(const Ticker& ticker) {
  auto& [symbol, p, t, metrics, signal] = ticker;

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
  auto& [symbol, p, t, metrics, signal] = ticker;

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
  auto position = metrics.position;
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
std::string to_str<FormatTarget::Telegram>(const Positions& positions,
                                           const Portfolio& portfolio) {
  if (positions.empty())
    return "No positions open\n";

  std::string msg = "🗿 *Open Positions*\n\n";

  for (auto [symbol, position] : positions) {
    auto ticker = portfolio.get_ticker(symbol);
    double price = ticker == nullptr ? 0.0 : ticker->metrics.last_price();

    auto [qty, px, cost, _] = position;
    auto pnl = position.pnl(price);

    double total = cost;
    double gain = (total != 0) ? pnl / std::abs(total) * 100.0 : 0.0;
    auto pnl_str = std::format(" | PnL: {:+.2f} ({:+.2f}%)\n", pnl, gain);

    msg += std::format("- {:<6}{}{}{}\n", symbol, (qty > 0 ? " +" : " "),
                       to_str(position), (qty > 0 ? "" : pnl_str));
  }

  return msg;
}

template <>
std::string to_str<FormatTarget::Telegram>(const Portfolio& portfolio) {
  auto msg_id = TG::send_doc("page/index.html", "portfolio.html", "");
  if (msg_id != -1)
    TG::pin_message(msg_id);

  std::string msg;

  auto& tickers = portfolio.tickers;
  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Exit)
      msg += to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::HoldCautiously)
      msg += to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Entry)
      msg += to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Watchlist)
      msg += to_str<FormatTarget::Alert>(ticker);

  for (auto& [symbol, ticker] : tickers)
    if (ticker.signal.type == SignalType::Caution)
      msg += to_str<FormatTarget::Alert>(ticker);

  std::string header = msg == "" ? "" : "📊 *Market Update*\n\n";
  return header + msg;
}

template <>
std::string to_str<FormatTarget::Telegram>(const Signal& a, const Signal& b) {
  std::string output;

  if (a.type != b.type)
    output += std::format("{}{} -> {}{}\n",  //
                          a.emoji(), to_str(a.type), b.emoji(), to_str(b.type));

  auto compare = [&]<typename T>(const std::vector<T>& old_r,
                                 const std::vector<T>& new_r, auto& label) {
    std::set<T> old_set{old_r.begin(), old_r.end()};
    std::set<T> new_set{new_r.begin(), new_r.end()};

    std::vector<std::string> added, removed;
    for (auto& t : new_set)
      if (!old_set.count(t))
        added.push_back(to_str(t));
    for (auto& t : old_set)
      if (!new_set.count(t))
        removed.push_back(to_str(t));

    if (!added.empty()) {
      output +=
          std::format("+ {}: {}\n", label, join(added.begin(), added.end()));
    }
    if (!removed.empty()) {
      output += std::format("- {}: {}\n", label,
                            join(removed.begin(), removed.end()));
    }
  };

  compare(a.entry_reasons, b.entry_reasons, "🡄r");
  compare(a.exit_reasons, b.exit_reasons, "🡆r");
  compare(a.entry_hints, b.entry_hints, "🡄h");
  compare(a.exit_hints, b.exit_hints, "🡆h");

  return output.empty() ? "" : output;
}
