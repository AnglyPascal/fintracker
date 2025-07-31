#pragma once

#include "_html_template.h"

#include <string_view>

inline constexpr std::string_view index_reload =
    R"(<meta http-equiv="refresh" content="1">)";

inline constexpr std::string_view index_subtitle_template = R"(
    <div class="subtitle">
      <div class="update-block">
        <b>Updated</b>: {}
      </div>
      <div class="update-block">
        <b>Analyst Updates:</b> 
          <a href="https://www.marketbeat.com/ratings/us/" target="_blank">MarketBeat</a>
      </div>
      <div class="update-block">
        <a href="trades.html" target="_blank"><b>Trades</b></a>
      </div>
      <div class="update-block">
        <b>Earnings:</b> 
          <a href="https://finance.yahoo.com/calendar/earnings/" target="_blank">Yahoo</a>
          <a href="calendar.html">Calendar</a>
      </div>
    </div>
)";

inline constexpr std::string_view index_event_template = R"(
  <a href="https://finance.yahoo.com/quote/{}/analysis/" 
     target="_blank">
     {}
  </a>
)";

inline constexpr std::string_view index_row_template = R"(
  <tr class="{}" 
      id="row-{}" 
      style="{}"
      onclick="toggleSignalDetails(this, '{}-details') ">
    <td data-label="Signal"><div class="eventful">{}</div></td>
    <td data-label="Symbol">
      <div class="eventful">
        <a href="{}.html" target="_blank"><b>{}</b></a>{}
      </div>
    </td>
    <td data-label="Price"><div>{}</div></td>
    <td data-label="EMA"><div>{}</div></td>
    <td data-label="RSI"><div>{}</div></td>
    <td data-label="MACD"><div>{}</div></td>
    <td data-label="Position"><div>{}</div></td>
    <td data-label="Stop Loss"><div>{}</div></td>
  </tr>
)";

inline constexpr std::string_view index_signal_td_template = R"(
  <td class="{}">
    {}
  </td>
)";

inline constexpr std::string_view index_signal_entry_template = R"(
<div class="signal-list">
  <ul>
    {}
  </ul>
</div>
)";

inline constexpr std::string_view index_signal_template = R"(
  <tr id="{}-details" 
      class="signal-details-row" 
      style="display:none">
    <td colspan="8">
      <div>
        <table class="signal">
          <tr class="signal-details-tr">
            {}
            {}
          </tr>
        </table>
      </div>
    </td>
  </tr>
)";

inline constexpr std::string_view trades_row_template = R"(
  <tr class="{}">
    <td data-label="Time">{}</td>
    <td data-label="Symbol"><a href="{}.html">{}</a></td>
    <td data-label="Action">{}</td>
    <td data-label="Quantity">{:.2f}</td>
    <td data-label="Price">{:.2f}</td>
    <td data-label="Total">{:.2f}</td>
  </tr>
)";

inline constexpr std::string_view trades_json_item = R"(
  {{ "date": "{}", "ticker": "{}", "action": "{}", 
    "qty": {:.2f}, "px": {:.2f}, "total": {:.2f} }},
)";

inline constexpr std::string_view trades_json_dict = R"(
"{}": [
{}
],
)";

