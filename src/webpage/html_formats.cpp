#include "core/portfolio.h"
#include "util/format.h"
#include "util/times.h"

#include <cmath>
#include <string_view>

constexpr auto thing_abbr_templ = R"(<div class="thing-abbr">{}{}</div>)";
constexpr auto thing_desc_templ = R"(<div class="thing-desc">{}</div>)";

template <>
std::string to_str(const Filter& f) {
  return f.str;
}

template <>
std::string to_str<FormatTarget::HTML>(const Filter& f) {
  auto f_desc = f.desc == "" ? "" : std::format(thing_desc_templ, f.desc);
  return std::format(thing_abbr_templ, f.str, f_desc);
}

template <>
std::string to_str<FormatTarget::HTML>(const Hint& h) {
  auto h_str =
      h.severity() >= Severity::Urgent ? tagged(to_str(h), BOLD) : to_str(h);
  auto h_desc = h.desc == "" ? "" : std::format(thing_desc_templ, h.desc);
  return std::format(thing_abbr_templ, h_str, h_desc);
}

template <>
std::string to_str<FormatTarget::HTML>(const Reason& r) {
  auto r_str =
      r.severity() >= Severity::Urgent ? tagged(to_str(r), BOLD) : to_str(r);
  auto r_desc = r.desc == "" ? "" : std::format(thing_desc_templ, r.desc);
  return std::format(thing_abbr_templ, r_str, r_desc);
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

inline constexpr auto score_div_template = R"(
  <div class="thing-abbr">
    {}
    <div class="thing-desc">
      [{}, {}]
    </div>
  </div> 
)";

template <>
std::string to_str<FormatTarget::HTML>(const Score& scr) {
  std::string scr_str = "";
  if (std::abs(scr) > 4)
    scr_str = std::format("<b>{:+}</b>", Score::pretty(scr.final));
  else
    scr_str = std::format("{:+}", Score::pretty(scr.final));

  return std::format(score_div_template,        //
                     scr_str,                   //
                     Score::pretty(scr.entry),  //
                     Score::pretty(scr.exit)    //
  );
}

inline constexpr auto signal_div_template = R"(
  <div class="signal-div {}">
    <div class="signal-text nobreak">{}</div> 
    <div class="signal-time">{:%a, %b %d, %H:%M}</div>
    <div class="signal-score nobreak">{}</div>
  </div>
)";

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s) {
  return std::format(signal_div_template, to_str<FormatTarget::HTML>(s.type),
                     to_str(s.type), s.tp, to_str<FormatTarget::HTML>(s.score));
}

template <>
std::string to_str<FormatTarget::HTML>(const typename Filters::value_type& p,
                                       const std::string& sep) {
  auto timeframe = p.first == H_1.count()   ? "1h"
                   : p.first == H_4.count() ? "4h"
                   : p.first == D_1.count() ? "1d"
                                            : "align";
  constexpr auto templ = R"(
    <div class="filter-row flex-center">
      <div class="filter-row-tf nobreak"><b>{}:</b></div> 
      <div class="filter-row-list">{}</div>
    </div>
  )";
  return std::format(                                                  //
      templ,                                                           //
      timeframe,                                                       //
      join<FormatTarget::HTML>(p.second.begin(), p.second.end(), sep)  //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const CombinedSignal& s) {
  return std::format(                      //
      signal_div_template,                 //
      to_str<FormatTarget::HTML>(s.type),  //
      to_str(s.type),                      //
      now_ny_time(),                       //
      to_str<FormatTarget::HTML>(s.score)  //
  );
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
  if (sizing.rec == Recommendation::Avoid)
    return std::format("<div><b>{}</b></div>",
                       to_str<FormatTarget::HTML>(sizing.rec));

  constexpr auto templ = R"(
    <div><b>{}</b>: {} w/ {:.2f}</div>
    <div><b>Risk</b>: {} ({:.2f}%), {:.1f}</div>
  )";

  return std::format(templ,                                   //
                     to_str<FormatTarget::HTML>(sizing.rec),  //
                     colored("yellow", sizing.rec_shares),    //
                     sizing.rec_capital,                      //
                     colored("red", sizing.risk_amount),      //
                     sizing.risk_pct * 100,                   //
                     sizing.risk                              //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const Filters& filters) {
  constexpr auto trend_html = R"(
    <b>Trends</b>: 
    <ul>
      {}
    </ul>
  )";

  std::string str = "";
  for (auto& p : filters)
    str += std::format("<li>{}</li>",
                       to_str<FormatTarget::HTML>(p, std::string(" ")));
  return std::format(trend_html, str);
}

template <>
std::string to_str(const ProfitTarget& pt) {
  if (pt.target_price == 0.0)
    return "";
  auto str = std::format("{:.2f}", pt.target_price);
  return std::format("<div class=\"price_target\">{}</div>", str);
}

template <>
std::string to_str(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  auto str = std::format("{:.2f}", sl.final_stop);
  if (sl.use_stop_limit)
    str += std::format(" -> {:.2f}", sl.limit_price);
  return std::format("<div class=\"stop_loss\">{}</div>", str);
}

template <>
std::string to_str<FormatTarget::HTML>(const StopLoss& sl) {
  if (sl.final_stop == 0.0)
    return "";
  constexpr auto templ = R"(
    <b>{}</b>: {}, {}, <b>{}</b> {}
  )";
  return std::format(                                 //
      templ,                                          //
      sl.order_type == StopOrderType::STOP_LOSS       //
          ? (sl.is_trailing ? "Trailing" : "Stop")    //
          : "StopLimit",                              //
      colored("yellow", sl.swing_low),                //
      colored("white", sl.atr_stop),                  //
      colored("blue", sl.final_stop),                 //
      sl.order_type == StopOrderType::STOP_LOSS       //
          ? ""                                        //
          : "& " + colored("yellow", sl.limit_price)  //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const Forecast& f) {
  return std::format("<b>Forecast</b>: <b>{}</b> {}, {}",                  //
                     colored(f.exp_pnl > 0 ? "green" : "red", f.exp_pnl),  //
                     colored("gray", std::format("[{}d]", f.holding_days)),
                     colored("blue", f.conf));
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
  return std::format("{} (<b>{:+.2f}</b>%)", to_str(*pos), pos->pct(price));
}

inline std::string signal_type_score(double val) {
  return std::format("<span style='opacity: {0:.2f}'>({0:.1f})</span>", val);
}

inline std::string reason_list(const auto& header,
                               auto& lst,
                               auto cls,
                               auto& stats) {
  constexpr auto div = R"(
  <li><b>{}</b>
    <ul>
      {}
    </ul>
  </li>
  )";

  std::string body = "";
  for (auto& r : lst)
    if (r.cls() == cls) {
      auto colored_stats = [&](auto& stats) {
        return std::format(
            " {}: <b>{}</b> ({}) <b>{}</b>",                              //
            colored("snow", signal_type_score(r.score)),                  //
            colored(stats.avg_pnl > 0 ? "green" : "red", stats.avg_pnl),  //
            colored("white", stats.pnl_volatility),                       //
            colored("blue", stats.imp)                                    //
        );
      };

      std::string stat_str = "";
      auto it = stats.find(r.type);
      if (it != stats.end())
        stat_str = colored_stats(it->second);
      body += std::format("<li>{}<span class=\"stats-details\">{}</span></li>",
                          to_str<FormatTarget::HTML>(r), stat_str);
    }

  if (body.empty())
    return "";

  return std::format(div, header, body);
}

inline constexpr auto signal_entry_template = R"(
{}
<div class="signal-list">
  <ul>
    {}
    {}
  </ul>
</div>
)";

template <>
std::string to_str<FormatTarget::HTML>(const Signal&, const Forecast& fc) {
  if (fc.exp_pnl == 0.0)
    return "";

  std::string icon = "";
  if (fc.exp_pnl > 4)
    icon = tagged("🌣", BOLD, YELLOW);
  else if (fc.exp_pnl > 0)
    icon = tagged("🌥", YELLOW);
  else if (fc.exp_pnl > -4)
    icon = tagged("🌧", GRAY);
  else
    icon = tagged("⛈", BOLD, GRAY);

  return std::format(                                          //
      "<li>{}: {} {}, {}</li>",                                //
      icon,                                                    //
      colored(fc.exp_pnl > 0 ? "green" : "red", fc.exp_pnl),   //
      colored("gray", std::format("[{}d]", fc.holding_days)),  //
      colored("blue", fc.conf)                                 //
  );
}

template <>
std::string to_str<FormatTarget::HTML>(const Signal& s, const Indicators& ind) {
  auto a = reason_list(tagged("▲", GREEN), s.reasons, SignalClass::Entry,  //
                       ind.stats.reason);                                  //
  auto b = reason_list(tagged("△", GREEN), s.hints, SignalClass::Entry,    //
                       ind.stats.hint);                                    //
  auto c = reason_list(tagged("▼", RED), s.reasons, SignalClass::Exit,     //
                       ind.stats.reason);                                  //
  auto d = reason_list(tagged("▽", RED), s.hints, SignalClass::Exit,       //
                       ind.stats.hint);                                    //
  auto e = a + b + c + d;

  return std::format(                            //
      signal_entry_template,                     //
      to_str<FormatTarget::HTML>(s),             //
      e,                                         //
      to_str<FormatTarget::HTML>(s, s.forecast)  //
  );
}
