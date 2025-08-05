#pragma once

#include "_html_template.hpp"

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
  <tr class="info-row {}" 
      id="row-{}" 
      style="{}"
      onclick="toggleSignalDetails(this, '{}-details') ">
    <td data-label="Signal">{}</td>
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
{}
<div class="signal-list">
  <ul>
    {}
  </ul>
</div>
)";

inline constexpr std::string_view index_signal_div_template = R"(
  <div class="signal {}">
    <div class="signal-text">{}</div> 
    <div class="signal-time">{:%d-%m %H:%M}</div>
    <div class="signal-score">{:+}</div>
  </div>
)";

inline constexpr std::string_view index_signal_overview_template = R"(
  <div class="trend">{}</div>
  <div class="forecast">{}</div>
  <div class="stop_loss">{}</div>
  <div class="position_sizing">{}</div>
)";

inline constexpr std::string_view index_combined_signal_template = R"(
  <td class="overview">{}</td>
  <td class="curr_signal signal-1h">{}</td>
  <td class="signal-4h">{}</td>
  <td class="signal-1d">{}</td>
)";

inline constexpr std::string_view index_signal_template = R"(
  <tr id="{}-details" 
      class="signal-details-row" 
      style="display:none">
    <td colspan="8" class="signal-table">
      <div class="signal-table">
        <table class="signal">
          <tr class="combined-signal-tr">{}</tr>
          <tr class="signal-memory-tr">{}</tr>
        </table>
      </div>
    </td>
  </tr>
)";

inline constexpr std::string_view ticker_body_template = R"(
  <div><b>Position:</b> {}</div>
  <div><b>Stop Loss:</b> {}</div>
  <div><b>Forecast:</b> {}</div>
  <div><b>Position size:</b> {}</div>
)";

