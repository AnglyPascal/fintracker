#include "_html_template.h"
#include "config.h"
#include "format.h"
#include "html_template.h"
#include "portfolio.h"
#include "times.h"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <thread>

template <typename Arg>
inline std::string colored(std::string_view color, Arg&& arg) {
  return std::format("<span style='color: var(--color-{});'>{}</span>",  //
                     color, std::forward<Arg>(arg));
}

inline std::string colored(std::string_view color, double arg) {
  return std::format("<span style='color: var(--color-{});'>{:.2f}</span>",  //
                     color, arg);
}

inline constexpr std::string csv_fname(const std::string& symbol,
                                       const std::string& time,
                                       const std::string& fn) {
  return std::format("page/src/{}{}_{}.csv", symbol, time, fn);
}

inline constexpr size_t n_days_plot = 90;

LocalTimePoint Indicators::plot(const std::string& sym,
                                const std::string& time) const {
  std::ofstream f(csv_fname(sym, time, "data"));
  f << "datetime,open,close,high,low,volume,ema9,ema21,rsi,macd,signal\n";

  size_t n_candles_per_day = (D_1 + interval - minutes{1}) / interval;
  size_t n = std::min(candles.size(), n_candles_per_day * n_days_plot);

  for (size_t i = candles.size() - n; i < candles.size(); i++)
    f << std::format(
        "{},{:.2f},{:.2f},{:.2f},{:.2f},{},"
        "{:.2f},{:.2f},{:.2f},{:.2f},{:.2f}\n",
        candles[i].datetime, candles[i].open, candles[i].close, candles[i].high,
        candles[i].low, candles[i].volume, _ema9.values[i], _ema21.values[i],
        _rsi.values[i], _macd.macd_line[i], _macd.signal_ema.values[i]);

  f.flush();
  f.close();

  auto trends_to_csv = [this, n, &sym, &time](auto& top_trends, auto& name) {
    for (size_t i = 0; i < top_trends.size(); i++) {
      std::ofstream f(csv_fname(sym, time, std::format("{}_fit_{}", name, i)));
      f << "datetime,value\n";

      auto& top_trend = top_trends[i];
      auto m = std::min((size_t)top_trend.period, n);
      for (size_t j = candles.size() - m; j < candles.size(); j++)
        f << std::format("{},{:.2f}\n", candles[j].datetime, top_trend.eval(j));

      f.flush();
      f.close();
    }
  };

  trends_to_csv(trends.price.top_trends, "price");
  trends_to_csv(trends.rsi.top_trends, "rsi");

  std::ofstream sr(csv_fname(sym, time, "support_resistance"));
  sr << "support,lower,upper,confidence\n";
  for (auto [lo, hi, conf] : support.zones)
    sr << std::format("0,{:.2f},{:.2f},{:.2f}\n", lo, hi, conf);
  for (auto [lo, hi, conf] : resistance.zones)
    sr << std::format("1,{:.2f},{:.2f},{:.2f}\n", lo, hi, conf);
  sr.flush();
  sr.close();

  return candles[candles.size() - n].time();
}

LocalTimePoint Metrics::plot(const std::string& sym) const {
  auto start_1h = ind_1h.plot(sym, "_1h");
  auto start_4h = ind_4h.plot(sym, "_4h");
  auto start_1d = ind_1d.plot(sym, "_1d");
  return std::max({start_1h, start_4h, start_1d});
}

template <>
std::string to_str(const Trade& t) {
  return std::format(                              //
      "{},{},{},{:.2f},{:.2f},{:.2f},{},{}",       //
      closest_nyse_aligned_time(t.date),           //
      t.ticker,                                    //
      (t.action == Action::BUY ? "BUY" : "SELL"),  //
      t.qty,                                       //
      t.px,                                        //
      t.total,                                     //
      t.remark,                                    //
      t.rating                                     //
  );
}

void Portfolio::write_plot_data(const std::string& symbol) const {
  if (!config.plot_en)
    return;

  auto it = tickers.find(symbol);
  if (it == tickers.end())
    return;

  spdlog::trace("plotting {}", symbol.c_str());

  auto start_time = it->second.metrics.plot(symbol);

  std::ofstream f(csv_fname(symbol, "", "trades"));
  f << "datetime,name,action,qty,price,total,remark,rating\n";

  auto& all_trades = get_trades();
  if (auto it = all_trades.find(symbol); it != all_trades.end()) {
    for (auto& trade : it->second) {
      if (trade.time() > start_time)
        f << to_str(trade) << std::endl;
    }
  }
  f.flush();
  f.close();
}

/******************************
 * Webpage related formatting *
 ******************************/

template <>
std::string to_str<FormatTarget::HTML>(const Hint& h) {
  if (h.severity() >= Severity::Urgent)
    return std::format("<b>{}</b>", to_str(h));
  return to_str(h);
}

template <>
std::string to_str<FormatTarget::HTML>(const Reason& r) {
  if (r.severity() >= Severity::Urgent)
    return std::format("<b>{}</b>", to_str(r));
  return to_str(r);
}

inline constexpr std::string index_row_class(Rating type) {
  switch (type) {
    case Rating::Entry:
      return "signal-entry";
    case Rating::Exit:
      return "signal-exit";
    case Rating::Watchlist:
      return "signal-watchlist";
    case Rating::Caution:
      return "signal-caution";
    case Rating::HoldCautiously:
      return "signal-holdcautiously";
    case Rating::Mixed:
      return "signal-mixed";
    case Rating::Skip:
      return "signal-skip";
    default:
      return "signal-none";
  }
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s) {
  return std::format(index_signal_div_template, index_row_class(s.type),
                     to_str(s.type), s.tp, (int)std::round(s.score * 10));
}

template <>
std::string to_str<FormatTarget::HTML>(const CombinedSignal& s) {
  std::string conf_str = "";

  if (s.type == Rating::Exit) {
    if (s.stop_hit.type != StopHitType::None)
      conf_str = s.stop_hit.str();
  } else if (s.type == Rating::Entry) {
    auto& conf = s.confirmations;
    conf_str = join(conf.begin(), conf.end());
  }

  if (conf_str != "")
    conf_str = std::format(": ({})", conf_str);

  return std::format(index_signal_div_template,  //
                     index_row_class(s.type), to_str(s.type) + conf_str,
                     now_ny_time(), (int)std::round(s.score * 10));
}

template <>
std::string to_str<FormatTarget::HTML>(const Forecast& f) {
  return std::format("<b>Forecast</b>: <b>{} / {}</b>, {:.2f}",  //
                     colored("green", f.expected_return),        //
                     colored("red", f.expected_drawdown),        //
                     f.confidence);
}

inline std::string reason_list(auto& header, auto& lst, auto cls, auto& stats) {
  constexpr std::string_view div = R"(
  <li><b>{}</b>
    <ul>
      {}
    </ul>
  </li>
  )";

  std::string body = "";
  for (auto& r : lst)
    if (r.cls() == cls && r.source() != Source::Trend) {
      auto colored_stats = [&, cls](auto gain, auto loss, auto win) {
        auto g_str = colored("green", gain);
        auto l_str = colored("red", loss);

        if (cls == SignalClass::Entry)
          return std::format("<b>{}</b> / {}, <b>{:.2f}</b>", g_str, l_str,
                             win);
        else
          return std::format("{} / <b>{}</b>, <b>{:.2f}</b>", g_str, l_str,
                             1 - win);
      };

      std::string stat_str = "";
      auto it = stats.find(r.type);
      if (it != stats.end()) {
        auto [_, ret, dd, w, _, _] = it->second;
        stat_str = ": " + colored_stats(ret, dd, w);
      }
      body += std::format("<li>{}<span class=\"stats-details\">{}</span></li>",
                          to_str(r), stat_str);
    }

  if (body.empty())
    return "";

  return std::format(div, header, body);
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Indicators& ind) {
  auto a = reason_list("▲", s.reasons, SignalClass::Entry,  //
                       ind.reason_stats);
  auto b = reason_list("△", s.hints, SignalClass::Entry,  //
                       ind.hint_stats);
  auto c = reason_list("▼", s.reasons, SignalClass::Exit,  //
                       ind.reason_stats);
  auto d = reason_list("▽", s.hints, SignalClass::Exit,  //
                       ind.hint_stats);
  auto e = a + b + c + d;

  return std::format(index_signal_entry_template,  //
                     to_str<FormatTarget::HTML>(s), e);
}

template <>
std::string to_str<FormatTarget::HTML>(const Recommendation& recom) {
  switch (recom) {
    case Recommendation::StrongBuy:
      return colored("green", "Strong Buy");
    case Recommendation::Buy:
      return colored("green", "Buy");
    case Recommendation::WeakBuy:
      return colored("blue", "Weak Buy");
    case Recommendation::Caution:
      return colored("yellow", "Caution");
    case Recommendation::Avoid:
      return colored("red", "Avoid");
    default:
      return "";
  }
}

template <>
std::string to_str<FormatTarget::HTML>(const PositionSizing& sizing) {
  if (sizing.recommendation == Recommendation::Avoid)
    return std::format("<div><b>{}</b></div>",
                       to_str<FormatTarget::HTML>(sizing.recommendation));

  constexpr std::string_view templ = R"(
    <div><b>{}</b>: {} w/ {:.2f}</div>
    <div><b>Risk</b>: {} ({:.2f}%), {:.1f}</div>
    {}
  )";

  std::string warnings = "";
  if (!sizing.warnings.empty()) {
    for (auto& warning : sizing.warnings) {
      warnings += warning + ", ";
    }
    warnings.pop_back();
    warnings.pop_back();

    warnings = std::format("<div><b>Warnings:</b> {}</div>", warnings);
  }

  return std::format(templ,                                              //
                     to_str<FormatTarget::HTML>(sizing.recommendation),  //
                     colored("yellow", sizing.recommended_shares),       //
                     sizing.recommended_capital,                         //
                     colored("red", sizing.actual_risk_amount),          //
                     sizing.actual_risk_pct * 100,                       //
                     sizing.overall_risk_score,                          //
                     warnings                                            //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const Filter& f) {
  auto str = f.str;

  if (str == "⇗")
    return colored("green", "<b>⇗</b>");
  if (str == "↗")
    return colored("green", "<b>↗</b>");
  if (str == "🡒")
    return colored("blue", "<b>🡒</b>");
  if (str == "↘")
    return colored("red", "<b>↘</b>");

  return str;
}

template <>
std::string to_str<FormatTarget::HTML>(const Filters& filters,
                                       const std::vector<Hint>& trends_1h) {
  std::string trend_1h, trend_1d, trend_4h;

  for (auto& h : trends_1h)
    trend_1h += h.str() + " ";

  if (!trend_1h.empty())
    trend_1h = std::format("<li><b>1h</b>: {}</li>", trend_1h);

  for (auto& f : filters.trends_4h)
    trend_4h += to_str<FormatTarget::HTML>(f) + " ";
  for (auto& f : filters.trends_1d)
    trend_1d += to_str<FormatTarget::HTML>(f) + " ";

  constexpr std::string_view trend_html = R"(
    <b>Trends</b>: 
    <ul>
      {}
      <li><b>4h</b>: {}</li>
      <li><b>1d</b>: {}</li>
    </ul>
  )";

  return std::format(trend_html, trend_1h, trend_4h, trend_1d);
}

template <>
std::string to_str<FormatTarget::HTML>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("<b>Stops</b>: {:.2f}, {:.2f}, <b>{}</b>",  //
                     sl.swing_low, sl.atr_stop, colored("blue", sl.final_stop));
}

template <>
std::string to_str<FormatTarget::HTML>(const CombinedSignal& s,
                                       const Ticker& ticker) {
  auto& ind_1h = ticker.metrics.ind_1h;
  auto& sig_1h = ind_1h.signal;

  auto& ind_4h = ticker.metrics.ind_4h;
  auto& sig_4h = ind_4h.signal;

  auto& ind_1d = ticker.metrics.ind_1d;
  auto& sig_1d = ind_1d.signal;

  std::vector<Hint> trends_1h;
  for (auto h : ticker.metrics.ind_1h.signal.hints) {
    if (h.source() == Source::Trend)
      trends_1h.push_back(h);
  }

  auto overview =
      std::format(index_signal_overview_template,
                  to_str<FormatTarget::HTML>(s.filters, trends_1h),   //
                  to_str<FormatTarget::HTML>(s.forecast),             //
                  to_str<FormatTarget::HTML>(ticker.stop_loss),       //
                  to_str<FormatTarget::HTML>(ticker.position_sizing)  //
      );

  return std::format(index_combined_signal_template,              //
                     overview,                                    //
                     to_str<FormatTarget::HTML>(sig_1h, ind_1h),  //
                     to_str<FormatTarget::HTML>(sig_4h, ind_4h),  //
                     to_str<FormatTarget::HTML>(sig_1d, ind_1d)   //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Source& src) {
  std::string interesting;
  auto add = [&interesting, src](auto& iter) {
    for (auto& a : iter) {
      if (a.source() != src)
        continue;
      if (a.severity() >= Severity::High) {
        interesting += " " + to_str<FormatTarget::HTML>(a);
      }
    }
  };

  add(s.reasons);
  add(s.hints);

  return interesting;
}

template <>
std::string to_str(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("{:.2f}", sl.final_stop);
}

template <>
std::string to_str(const Event& ev) {
  if (ev.type == '\0')
    return "";

  auto days_until = ev.days_until();
  if (days_until < 0)
    return "";
  return std::format("{}{:+}", ev.type, days_until);
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
  return (type == Rating::Caution || type == Rating::Mixed ||
          type == Rating::None || type == Rating::Skip);
};

template <>
std::string to_str<FormatTarget::HTML>(const Portfolio& p) {
  std::string body;

  std::vector<std::pair<const std::string*, const Ticker*>> sorted;
  for (auto& [symbol, ticker] : p.tickers)
    sorted.emplace_back(&symbol, &ticker);

  std::sort(sorted.begin(), sorted.end(), [](auto& lhs, auto& rhs) {
    auto lt = lhs.second->signal.type;
    auto rt = rhs.second->signal.type;
    if (lt == rt)
      return *lhs.first < *rhs.first;
    return lt < rt;
  });

  for (auto [s, t] : sorted) {
    auto& symbol = *s;
    auto& ticker = *t;

    auto& m = ticker.metrics;
    auto& ind_1h = m.ind_1h;

    auto& sig = ticker.signal;
    auto& sig_1h = ind_1h.signal;

    auto row_class = index_row_class(sig.type);

    auto pos_str = to_str<FormatTarget::HTML>(m.position, m.last_price());
    auto stop_loss_str = m.has_position()  //
                             ? std::format("<b>{:.2f}</b>, {}", m.last_price(),
                                           to_str(ticker.stop_loss))
                             : std::format("<b>{:.2f}</b>", m.last_price());

    auto event = p.calendar.next_event(symbol);
    auto event_str = std::format(index_event_template, symbol, to_str(event));

    auto stop_str = sig.stop_hit.str();
    auto str = [&](auto src) {
      return to_str<FormatTarget::HTML>(sig_1h, src);
    };

    body +=
        std::format(index_row_template,                                     //
                    row_class, symbol,                                      //
                    hide(m, sig.type) ? "display:none;" : "", symbol,       //
                    to_str<FormatTarget::HTML>(sig),                        //
                    symbol, symbol, event_str,                              //
                    str(Source::Price) + stop_str + str(Source::SR),        //
                    str(Source::EMA), str(Source::RSI), str(Source::MACD),  //
                    pos_str, stop_loss_str                                  //
        );

    std::string mem_str = "";
    int n = 1;
    auto& past_sigs = ind_1h.memory.past;
    for (auto it = past_sigs.rbegin(); it != past_sigs.rend(); it++) {
      auto s =
          std::format(index_signal_td_template,  //
                      "old_signal", to_str<FormatTarget::HTML>(*it, ind_1h));

      mem_str += s;
      if (n++ == 4)
        break;
    }

    body += std::format(index_signal_template,                    //
                        symbol,                                   //
                        to_str<FormatTarget::HTML>(sig, ticker),  //
                        mem_str);
  }

  auto datetime = std::format("{:%b %d, %H:%M}", p.last_updated);
  auto subtitle = std::format(index_subtitle_template, datetime);
  auto reload = (config.replay_en && config.debug_en) ? index_reload : "";

  return std::format(index_template, reload, subtitle, body);
}

template <>
std::string to_str<FormatTarget::HTML>(const Indicators& ind) {
  auto& sig = ind.signal;

  auto stats_html = []<typename S>(auto& stats, S) {
    std::string html;
    for (const auto& [rtype, stat] : stats) {
      if (rtype == S::none)
        continue;
      auto [_, ret, dd, winrate, _, _] = stat;

      S r{rtype};
      auto ret_str = colored("green", ret);
      auto dd_str = colored("red", dd);

      if (r.cls() == SignalClass::Entry)
        ret_str = std::format("<b>{}</b>", ret_str);
      else {
        winrate = 1 - winrate;
        dd_str = std::format("<b>{}</b>", dd_str);
      }

      html += std::format("<li class=\"{}\">{}: {} / {}, <b>{:.2f}</b></li>",
                          r.cls() == SignalClass::Entry ? "entry" : "exit",
                          to_str(r), ret_str, dd_str, winrate);
    }
    return html;
  };

  // Recent signals memory
  std::string mem_row, mem_html;
  auto& past = ind.memory.past;
  int i = 0;
  for (auto it = past.rbegin(); it != past.rend(); ++it) {
    i++;
    mem_row += std::format("<td class=\"signal-td\">{}</td>",
                           to_str<FormatTarget::HTML>(*it, ind));

    if (i % 4 == 0) {
      mem_html += std::format("<tr>{}</tr>", mem_row);
      mem_row = "";
    }
  }

  return std::format(indicators_template,
                     to_str<FormatTarget::HTML>(sig, ind),    //
                     stats_html(ind.reason_stats, Reason{}),  //
                     stats_html(ind.hint_stats, Hint{}),      //
                     mem_html                                 //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const Ticker& ticker) {
  auto& m = ticker.metrics;
  auto& forecast = ticker.signal.forecast;

  auto body =
      std::format(ticker_body_template,
                  to_str<FormatTarget::HTML>(m.position, m.last_price()),  //
                  to_str<FormatTarget::HTML>(ticker.stop_loss),            //
                  to_str<FormatTarget::HTML>(forecast),                    //
                  to_str<FormatTarget::HTML>(ticker.position_sizing)       //
      );

  auto& sym = ticker.symbol;
  return std::format(ticker_template,
                     sym,       //
                     sym,       //
                     body, sym  //
  );
}

void Portfolio::write_page() const {
  std::thread([this]() {
    Timer timer;
    auto _ = reader_lock();

    auto index_fname = config.replay_en ? "page/public/index_replay.html"
                                        : "page/public/index.html";

    namespace fs = std::filesystem;
    if (!config.replay_en && fs::exists("page/public/index.html"))
      fs::copy_file(
          "page/public/index.html",
          std::format("page/backup/index_{:%F_%T}.html", now_ny_time()),
          fs::copy_options::overwrite_existing);

    std::ofstream index(index_fname);
    index << to_str<FormatTarget::HTML>(*this);
    index.flush();
    index.close();

    spdlog::info("[page] written in {:.2f}ms", timer.diff_ms());

    for (auto& [symbol, ticker] : tickers) {
      std::ofstream ticker_file(std::format("page/public/{}.html", symbol));
      ticker_file << to_str<FormatTarget::HTML>(ticker);
      ticker_file.flush();
      ticker_file.flush();

      auto print_ind = [&](auto& ind, auto& time) {
        std::ofstream f(std::format("page/public/{}_{}.html", symbol, time));
        f << to_str<FormatTarget::HTML>(ind);
        f.flush();
        f.flush();
      };

      print_ind(ticker.metrics.ind_1h, "1h");
      print_ind(ticker.metrics.ind_4h, "4h");
      print_ind(ticker.metrics.ind_1d, "1d");
    }
  }).detach();
}
