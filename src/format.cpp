#include "format.h"
#include "portfolio.h"

#include <format>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

template <>
std::string to_str(const Trade& t) {
  return std::format("{:<10} {:<6} {:<4} {:>6.2f} @ {:>6.2f}",                //
                     t.date, t.ticker,                                        //
                     (t.action == Action::BUY ? "BUY" : "SELL"),              //
                     double(t.qty) / FLOAT_SCALE, double(t.px) / FLOAT_SCALE  //
  );
}

std::string Position::to_str() const {
  auto qty_d = double(qty) / FLOAT_SCALE;
  auto px_d = double(px) / FLOAT_SCALE;
  return std::format("{:.2f} @ {:.2f}", qty_d, px_d);
}

inline std::string format_price(double value) {
  return std::format("{:.2f}", value);
}

std::string EMA::to_str() const {
  return values.empty() ? "--" : format_price(values.back());
}

std::string RSI::to_str() const {
  return values.empty() ? "--" : format_price(values.back());
}

std::string Ticker::to_str(bool tg) const {
  auto metrics_str = metrics.to_str(tg);
  auto pos_line = pos_to_str(tg);
  auto stop_line = position == nullptr || position->qty == 0
                       ? ""
                       : metrics.stop_loss.to_str(tg) + "\n";

  return !tg ? std::format(
                   "{}••{}  "                           //
                   "{}{}{}"                             //
                   "{}\n"                               //
                   "{}",                                //
                   signal.color_bg(), RESET,            //
                   signal.color(), metrics_str, RESET,  //
                   signal.to_str(tg),                   //
                   pos_line                             //
                   )
             : std::format(
                   "{} *{}*{}\n"                      //
                   "{}"                               //
                   "{}"                               //
                   "{}\n",                            //
                   signal.emoji(), symbol, pos_line,  //
                   metrics_str,                       //
                   stop_line,                         //
                   signal.to_str(tg)                  //
               );
}

std::string Ticker::pos_to_str(bool tg) const {
  if (position == nullptr || position->qty <= 0)
    return "";

  auto price = metrics.last_price();
  auto pnl = position->pnl(price);
  auto pct = position->pct(price);

  auto pnl_str = std::format("{:+.2f} ({:+.2f}%)", pnl, pct);

  return tg ? std::format(" | {}, {}", position->to_str(), pnl_str)
            : std::format("->  {}{}{}, {} | {}\n",                           //
                          pnl > 0 ? GREEN : RED, position->to_str(), RESET,  //
                          pnl_str, metrics.stop_loss.to_str(false));
}

std::string Metrics::to_str(bool tg) const {
  const auto& ind = indicators_1h;

  auto ema_str = std::format("{}/{}", ind.ema9.to_str(), ind.ema21.to_str());
  auto rsi_str = ind.rsi.to_str();
  auto [high, pb] = pullback();
  auto pb_str = std::format("{:.2f} ({:.2f}%)", high, pb);

  return tg ? std::format(
                  "   Px: {:.2f} | EMA 9/21: {}\n   RSI: {} | Pullback: {}\n",
                  last_price(), ema_str, rsi_str, pb_str)
            : std::format("{:<8}{:<10.2f}{:<17}{:<10}{:<17}", symbol,
                          last_price(), ema_str, rsi_str, pb_str);
}

inline std::string reason_to_str(Reason r) {
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

bool Signal::has_signal() const {
  return type != SignalType::None;
}

bool Signal::has_hints() const {
  return !entry_hints.empty() || !exit_hints.empty();
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

std::string Signal::to_str(bool tg) const {
  if (!has_signal() && !has_hints())
    return "";

  auto color_reset = tg ? "" : RESET;

  std::string result;

  // --- Signal line ---
  if (has_signal()) {
    auto type_str = type_to_str();
    auto& reasons = type == SignalType::Entry ? entry_reasons : exit_reasons;
    auto reason_str = join(reasons, reason_to_str, ", ");

    if (tg)
      result += std::format("   Signal: {} ({})", type_str, reason_str);
    else
      result += std::format("Signal: {}{} ({}){}", color(), type_str,
                            reason_str, color_reset);
  } else {
    if (tg)
      result += "   Hints: ";
    else
      result += "Hints: ";
  }

  // --- Hints ---
  std::string entry_str, exit_str;

  if (!entry_hints.empty()) {
    if (tg)
      for (const auto& hint : entry_hints)
        entry_str += std::format("\n   + {}", hint.str);
    else
      entry_str = std::format(
          "{}{}{}", GREEN,
          join(entry_hints, [](auto& hint) { return hint.str; }), color_reset);
  }

  if (!exit_hints.empty()) {
    if (tg)
      for (const auto& hint : exit_hints)
        exit_str += std::format("\n   - {}", hint.str);
    else
      exit_str = std::format(
          "{}{}{}", RED, join(exit_hints, [](auto& hint) { return hint.str; }),
          color_reset);
  }

  if (tg) {
    result += entry_str + exit_str;
  } else {
    if (!entry_str.empty() || !exit_str.empty())
      result += std::format(                                     //
          "{}{}{}{}",                                            //
          has_signal() ? " | " : "",                             //
          entry_str,                                             //
          !entry_str.empty() && !exit_str.empty() ? " | " : "",  //
          exit_str                                               //
      );
  }

  return result + (tg ? "\n" : "");
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
      return "";
  }
}

std::string Signal::type_to_str() const {
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
      std::cout << ticker.to_str(false);
  std::cout << std::endl;
}

std::string StopLoss::to_str(bool tg) const {
  return tg ? std::format("   Stop: {:.2f}", final_stop)
            : std::format("Stops: sw = {:.2f}, ema = {:.2f}, atr = {:.2f}",  //
                          swing_low, ema_stop, atr_stop);
}

inline std::string position_for_tg_str(const std::string& symbol,
                                       const Position& pos,
                                       double pnl) {
  auto [qty, px, cost] = pos;

  double total = (double)(cost) / COST_SCALE;
  double gain = (total != 0) ? pnl / std::abs(total) * 100.0 : 0.0;
  auto pnl_str = std::format(" | PnL: {:+.2f} ({:+.2f}%)\n", pnl, gain);

  return std::format("- {:<7}{}{}{}\n", symbol, (qty > 0 ? " +" : " "),
                     pos.to_str(), (qty > 0 ? "" : pnl_str));
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

