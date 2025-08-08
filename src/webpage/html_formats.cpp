#include "format.h"
#include "portfolio.h"
#include "times.h"

#include <cmath>

template <>
std::string to_str<FormatTarget::HTML>(const Hint& h) {
  return h.severity() >= Severity::Urgent ? std::format("<b>{}</b>", to_str(h))
                                          : to_str(h);
}

template <>
std::string to_str<FormatTarget::HTML>(const Reason& r) {
  return r.severity() >= Severity::Urgent ? std::format("<b>{}</b>", to_str(r))
                                          : to_str(r);
}

template <>
std::string to_str<FormatTarget::HTML>(const Rating& type) {
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

inline constexpr std::string_view signal_div_template = R"(
  <div class="signal {}">
    <div class="signal-text">{}</div> 
    <div class="signal-time">{:%d-%m %H:%M}</div>
    <div class="signal-score">{:+}</div>
  </div>
)";

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s) {
  return std::format(signal_div_template, to_str<FormatTarget::HTML>(s.type),
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

  return std::format(signal_div_template,  //
                     to_str<FormatTarget::HTML>(s.type),
                     to_str(s.type) + conf_str, now_ny_time(),
                     (int)std::round(s.score * 10));
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

inline constexpr std::string_view signal_entry_template = R"(
{}
<div class="signal-list">
  <ul>
    {}
  </ul>
</div>
)";

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Indicators& ind) {
  auto a = reason_list("▲", s.reasons, SignalClass::Entry,  //
                       ind.stats.reason);
  auto b = reason_list("△", s.hints, SignalClass::Entry,  //
                       ind.stats.hint);
  auto c = reason_list("▼", s.reasons, SignalClass::Exit,  //
                       ind.stats.reason);
  auto d = reason_list("▽", s.hints, SignalClass::Exit,  //
                       ind.stats.hint);
  auto e = a + b + c + d;

  return std::format(signal_entry_template,  //
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

  for (auto& f : filters.at(H_4.count()))
    trend_4h += to_str<FormatTarget::HTML>(f) + " ";
  for (auto& f : filters.at(D_1.count()))
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
std::string to_str(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("{:.2f}", sl.final_stop);
}

template <>
std::string to_str<FormatTarget::HTML>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  return std::format("<b>Stops</b>: {:.2f}, {:.2f}, <b>{}</b>",  //
                     sl.swing_low, sl.atr_stop, colored("blue", sl.final_stop));
}

inline constexpr std::string_view signal_overview_template = R"(
  <div class="trend">{}</div>
  <div class="forecast">{}</div>
  <div class="stop_loss">{}</div>
  <div class="position_sizing">{}</div>
)";

inline constexpr std::string_view combined_signal_template = R"(
  <td class="overview">{}</td>
  <td class="curr_signal signal-1h">{}</td>
  <td class="signal-4h">{}</td>
  <td class="signal-1d">{}</td>
)";

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
      std::format(signal_overview_template,
                  to_str<FormatTarget::HTML>(s.filters, trends_1h),   //
                  to_str<FormatTarget::HTML>(s.forecast),             //
                  to_str<FormatTarget::HTML>(ticker.stop_loss),       //
                  to_str<FormatTarget::HTML>(ticker.position_sizing)  //
      );

  return std::format(combined_signal_template,                    //
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
