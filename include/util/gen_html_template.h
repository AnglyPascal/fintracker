
#pragma once
#include <string_view>

inline constexpr std::string_view index_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>Portfolio</title>
    <link rel="stylesheet" href="css/index.css">
    {}
  </head>

  <body>
    <div id="header">
      {}
      <div id="signal-filters">
        <button class="filter-btn active" 
          data-type="entry" onclick="toggleSignal(this) ">Entry</button>
        <button class="filter-btn active" 
          data-type="exit" onclick="toggleSignal(this) ">Exit</button>
        <button class="filter-btn active" 
          data-type="watchlist" onclick="toggleSignal(this) ">Watchlist</button>
        <button class="filter-btn active" 
          data-type="oldwatchlist" onclick="toggleSignal(this) ">Old Watchlist</button>
        <button class="filter-btn" 
          data-type="caution" onclick="toggleSignal(this) ">Caution</button>
        <button class="filter-btn active" 
          data-type="holdcautiously" 
          onclick="toggleSignal(this) ">Hold Cautiously</button>
        <button class="filter-btn" 
          data-type="mixed" onclick="toggleSignal(this) ">Mixed</button>
        <button class="filter-btn" 
          data-type="none" onclick="toggleSignal(this) ">None</button>
        <button class="filter-btn" 
          data-type="skip" onclick="toggleSignal(this) ">Skip</button>
        <button onclick="showAllSignals() ">Show All</button>
        <button class="stats-details-button" onclick="toggleStats() ">Show Details</button>
      </div>
    </div>

    <div class="table-wrapper">
      <table id="main-table">
        <thead>
          <tr>
            <th class="signal-th"> Signal </th>
            <th class="symbol-th"> Symbol </th>
            <th class="price-th"> Price </th>
            <th class="ema-th"> EMA </th>
            <th class="rsi-th"> RSI </th>
            <th class="macd-th"> MACD </th>
            <th class="position-th"> Position </th>
            <th class="stop-th"> Price & Stop </th>
          </tr>
        </thead>
        <tbody>
          {}
        </tbody>
      </table>
    </div>

    <script src="js/index.js"></script>
    <script src="http://localhost:35729/livereload.js?snipver=1"></script>
  </body>
</html>

)";

inline constexpr std::string_view ticker_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>{}</title>
    <link rel="stylesheet" href="css/ticker.css">
  </head>
  <body>
    <div id="title">
      <div id="title_text">{}</div>
      <div class="button-container">
        <button id="btn1h" class="active">1H</button>
        <button id="btn4h">4H</button>
        <button id="btn1d">1D</button>
      </div>
    </div>

    <div id="body">
      <div id="chart"></div>
      <div id="initial">{}</div>
      <div id="content"></div>
    </div>

    <script> let ticker = '{}'; </script>
    <script type="module">
    import {{ loadContent }} from "./js/plot.js";
    </script>
    <script src="http://localhost:35729/livereload.js?snipver=1"></script>

  </body>
</html>

)";

inline constexpr std::string_view indicators_template = R"(
<div class="indicators">
  <table>
    <tr> 
      <td class="stats curr-signal">{}</td>
      <td class="stats">
        <div><b>Reason</b></div>
        <ul>{}</ul>
      </td>
      <td class="stats">
        <div><b>Hint</b></div>
        <ul>{}</ul>
      </td>
    </tr>
  </table>

  <h3>Recent Signals</h3>
  <table class="memory-table">
    {}
  </table>
</div>

)";
