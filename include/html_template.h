#pragma once

#include "signal.h"

#include <string_view>

inline constexpr std::string_view index_reload =
    R"(<meta http-equiv="refresh" content="1">)";

inline constexpr std::string_view index_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    {}
  </head>
  <style>
    {}
  </style>

  <body>
    <h2>Portfolio Overview</h2>
    {}

    <div id="signal-filters" style="margin-bottom: 0.5em;">
      <button class="filter-btn active" 
              data-type="entry" onclick="toggleSignal(this) ">Entry</button>
      <button class="filter-btn active" 
              data-type="exit" onclick="toggleSignal(this) ">Exit</button>
      <button class="filter-btn active" 
              data-type="watchlist" onclick="toggleSignal(this) ">Watchlist</button>
      <button class="filter-btn" 
              data-type="caution" onclick="toggleSignal(this) ">Caution</button>
      <button class="filter-btn active" 
              data-type="holdcautiously" 
              onclick="toggleSignal(this) ">Hold Cautiously</button>
      <button class="filter-btn" 
              data-type="mixed" onclick="toggleSignal(this) ">Mixed</button>
      <button class="filter-btn" 
              data-type="none" onclick="toggleSignal(this) ">None</button>
      <button onclick="showAllSignals() ">Show All</button>
    </div>

    <div class="table-wrapper">
      <table>
        <thead>
          <tr>
            <th>
              <div class="col-header" onclick=" toggleColumn(1) ">
                <span class="label">Signal</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(2) ">
                <span class="label">Symbol</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(3) ">
                <span class="label">Price</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(4) ">
                <span class="label">EMA</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(5) ">
                <span class="label">RSI</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(6) ">
                <span class="label">MACD</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(7) ">
                <span class="label">Position</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(8) ">
                <span class="label">Price & Stop</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
          </tr>
        </thead>
        <tbody>
          {}
        </tbody>
      </table>
    </div>

    <script>
      {}
    </script>
  </body>
</html>
)";

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
    <td data-label="Signal">{}</td>
    <td data-label="Symbol">
      <div class="eventful">
        <a href="{}.html" target="_blank"><b>{}</b></a>{}
      </div>
    </td>
    <td data-label="Price">{}</td>
    <td data-label="EMA">{}</td>
    <td data-label="RSI">{}</td>
    <td data-label="MACD">{}</td>
    <td data-label="Position">{}</td>
    <td data-label="Stop Loss"><div>{}</div></td>
  </tr>
)";

inline constexpr std::string_view index_signal_template = R"(
  <tr id="{}-details" 
      class="signal-details-row" 
      style="display:none">
    <td colspan="4">
      <div class="signal-details">
        {}
      </div>
    </td>
    <td colspan="4">
      <div class="signal-details">
        {}
      </div>
    </td>
  </tr>
)";

inline constexpr std::string index_row_class(SignalType type) {
  switch (type) {
    case SignalType::Entry:
      return "signal-entry";
    case SignalType::Exit:
      return "signal-exit";
    case SignalType::Watchlist:
      return "signal-watchlist";
    case SignalType::Caution:
      return "signal-caution";
    case SignalType::HoldCautiously:
      return "signal-holdcautiously";
    case SignalType::Mixed:
      return "signal-mixed";
    default:
      return "signal-none";
  }
}

inline constexpr std::string_view trades_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      {}
    </style>
  </head>

  <body>
    <h2>Trades Overview</h2>

    <div class="table-wrapper">
      <table>
        <thead>
          <tr>
            <th>
              <div class="col-header" onclick=" toggleColumn(1) ">
                <span class="label">Time</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(2) ">
                <span class="label">Symbol</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(3) ">
                <span class="label">Action</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(4) ">
                <span class="label">Quantity</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(5) ">
                <span class="label">Price</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(6) ">
                <span class="label">Total</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
          </tr>
        </thead>
        <tbody>
          {}
        </tbody>
      </table>
    </div>

    <script>
      {}
    </script>
  </body>
</html>
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
