#pragma once

#include "signal.h"

#include <string_view>

inline constexpr std::string_view html_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>

    :root {{
      --font-size: 13px;
      --padding-cell: 0.25em 0.4em;
      --padding-mobile: 0.2em 0.3em;
      --row-gap: 0.4em;
      --color-border: #ccc;
      --color-header: #f0f0f0;
      --color-hover: #f5f5f5;

      --color-signal-entry-bg: #c8facc;
      --color-signal-exit-bg: #ffd2d2;
      --color-signal-watchlist-bg: #d2e6ff;
      --color-signal-caution-bg: #fff5d2;
      --color-signal-mixed-bg: #f5f5f5;

      --color-black: #000;
      --color-green: #008000;
      --color-red: #d10000;
      --color-yellow: #b58900;
    }}

    body {{
      font-family: monospace;
      font-size: var(--font-size);
      margin: 0.5em;
      background-color: #fff;
    }}

    table {{
      width: 100%;
        <!-- table-layout: fixed; -->
      border-collapse: collapse;
    }}

    th, td {{
      border: 1px solid var(--color-border);
      padding: var(--padding-cell);
      text-align: left;
      overflow-wrap: break-word;
      word-break: break-word;
      overflow: hidden;
      transition: all 0.2s ease;
    }}

    th {{
      background-color: var(--color-header);
      cursor: pointer;
      user-select: none;
    }}

    tr:hover {{
      background-color: var(--color-hover);
    }}

    tr.signal-entry {{
      background-color: var(--color-signal-entry-bg);
      color: var(--color-black);
    }}

    tr.signal-exit {{
      background-color: var(--color-signal-exit-bg);
      color: var(--color-black);
    }}

    tr.signal-watchlist {{
      background-color: var(--color-signal-watchlist-bg);
      color: var(--color-black);
    }}

    tr.signal-caution,
    tr.signal-holdcautiously {{
      background-color: var(--color-signal-caution-bg);
      color: var(--color-black);
    }}

    tr.signal-mixed {{
      background-color: var(--color-signal-mixed-bg);
      color: var(--color-black);
    }}

    /* Column collapse behavior */
    th .col-header {{
      display: flex;
      justify-content: space-between;
      align-items: center;
    }}

    .collapsed .label {{
      display: none;
    }}

    th.collapsed,
    td.collapsed {{
      width: 13px;
      padding: 0;
      overflow: hidden;
    }}

    td.collapsed {{
      visibility: collapse;
      white-space: nowrap;
    }}

    .toggle-btn::after {{
      content: "▼";
      font-size: 0.9em;
      color: #666;
    }}

    th.collapsed .toggle-btn::after {{
      content: "▶";
    }}

    .signal-details {{
      padding-top: 1em;
      padding-left: 1em;
      margin-top: 0em;
      font-size: 0.92em;
      color: #444;
    }}

    #signal-filters button {{
      font-family: monospace;
      font-size: 0.85em;
      margin-right: 0.3em;
      padding: 0.3em 0.6em;
      border: 1px solid #aaa;
      border-radius: 4px;
      background-color: #eee;
      color: #333;
      cursor: pointer;
      transition: background-color 0.2s;
    }}

    #signal-filters button:hover {{
      background-color: #ddd;
    }}

    #signal-filters button.active {{
      background-color: #333;
      color: #fff;
      border-color: #333;
    }}
    </style>

    <style>
    /* Mobile support */
    @media (max-width: 768px) {{
      table, thead, tbody, th, td, tr {{
        display: block;
        width: 100%;
          padding-right: 1em;
      }}

      thead {{
        display: none;
      }}

      tr {{
        margin-bottom: var(--row-gap);
        border: 1px solid var(--color-border);
        border-radius: 4px;
        padding: 0.3em;
        background-color: #fff;
      }}

      td {{
        display: flex;
        justify-content: space-between;
        flex-wrap: wrap; /* Allows wrapping to avoid overflow */
        padding: var(--padding-mobile);
        border: none;
        border-bottom: 1px solid var(--color-border);
        width: 100%;
        box-sizing: border-box;
        overflow-wrap: break-word;
      }}

      td:last-child {{
        border-bottom: none;
      }}

      td::before {{
        content: attr(data-label);
        font-weight: bold;
        color: #666;
      }}

      td.collapsed::before {{
        display: none;
      }}

      td.collapsed {{
        display: none;
      }}

      .signal-details-row {{
        font-size: 0.85em;
        line-height: 1.2;
      }}

      .signal-details {{
        color: #333;
        align-content: start;
      }}
    }}
    </style>
  </head>

  <body>
    <h2>Portfolio Overview</h2>
    <div style="margin-bottom: 1em"> <b>Updated</b>: {}</div>

    <div id="signal-filters" style="margin-bottom: 0.5em;">
      <button class="filter-btn active" 
              data-type="entry" onclick="toggleSignal(this) ">Entry</button>
      <button class="filter-btn active" 
              data-type="exit" onclick="toggleSignal(this) ">Exit</button>
      <button class="filter-btn active" 
              data-type="watchlist" onclick="toggleSignal(this) ">Watchlist</button>
      <button class="filter-btn active" 
              data-type="caution" onclick="toggleSignal(this) ">Caution</button>
      <button class="filter-btn active" 
              data-type="holdcautiously" 
              onclick="toggleSignal(this) ">Hold Cautiously</button>
      <button class="filter-btn active" 
              data-type="mixed" onclick="toggleSignal(this) ">Mixed</button>
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
                <span class="label">EMA9/21</span>
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
                <span class="label">Position</span>
                <span class="toggle-btn"></span>
              </div>
            </th>
            <th>
              <div class="col-header" onclick=" toggleColumn(7) ">
                <span class="label">Stop Loss</span>
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
      function toggleColumn(colIndex) {{
        const th = document.querySelector(`th:nth-child(${{colIndex}})`);
        const tds = document.querySelectorAll(`td:nth-child(${{colIndex}})`);

        const isCollapsed = th.classList.toggle("collapsed");
        tds.forEach(td => {{
          if (isCollapsed) td.classList.add("collapsed");
          else td.classList.remove("collapsed");
        }});
      }}

      function toggleSignalDetails(row, detailRowId) {{
        const detailRow = document.getElementById(detailRowId);
        if (!detailRow) return;

        const isVisible = detailRow.style.display !== "none";
        detailRow.style.display = isVisible ? "none" : "table-row";
      }}

      function toggleSignal(button) {{
        const type = button.dataset.type;
        const rows = document.querySelectorAll(`tr.signal-${{type}}`);

        const isActive = button.classList.toggle('active');
        rows.forEach(row => {{
          row.style.display = isActive ? '' : 'none';

          // Hide detail row if it's visible
          const detailRow = 
            document.getElementById(`${{row.id.replace('row-', '')}}-details`);
          if (detailRow && !isActive) detailRow.style.display = 'none';
        }});
      }}

      function showAllSignals() {{
        const signalRows = document.querySelectorAll('tr[id^="row-"]');
        signalRows.forEach(row => row.style.display = '');

        const detailRows = document.querySelectorAll('.signal-details-row');
        detailRows.forEach(row => row.style.display = 'none');

        const buttons = document.querySelectorAll('#signal-filters .filter-btn');
        buttons.forEach(btn => btn.classList.add('active'));
      }}
    </script>
  </body>
</html>
)";

inline constexpr std::string_view html_row_template = R"(
  <tr class="{}" 
      id="row-{}" 
      onclick="toggleSignalDetails(this, '{}-details') ">
    <td data-label="Signal">{}</td>
    <td data-label="Symbol"><a href="{}.html">{}</a></td>
    <td data-label="Price">{:.2f}</td>
    <td data-label="EMA 9|21">{:<6.2f} | {:<6.2f}</td>
    <td data-label="RSI">{:.2f}</td>
    <td data-label="Position">{}</td>
    <td data-label="Stop Loss">{}</td>
  </tr>
)";

inline constexpr std::string_view html_signal_template = R"(
  <tr id="{}-details" 
      class="signal-details-row" 
      style="display:none">
    <td colspan="4">
      <div class="signal-details">
        {}
      </div>
    </td>
    <td colspan="3">
      <div class="signal-details">
        {}
      </div>
    </td>
  </tr>
)";

inline constexpr std::string html_row_class(SignalType type) {
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
      return "";
  }
}

inline constexpr std::string_view trades_template = R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>

    :root {{
      --font-size: 13px;
      --padding-cell: 0.25em 0.4em;
      --padding-mobile: 0.2em 0.3em;
      --row-gap: 0.4em;
      --color-border: #ccc;
      --color-header: #f0f0f0;
      --color-hover: #f5f5f5;

      --color-buy: #c8facc;
      --color-sell: #ffd2d2;

      --color-black: #000;
      --color-green: #008000;
      --color-red: #d10000;
      --color-yellow: #b58900;
    }}

    body {{
      font-family: monospace;
      font-size: var(--font-size);
      margin: 0.5em;
      background-color: #fff;
    }}

    table {{
      width: 100%;
      border-collapse: collapse;
    }}

    th, td {{
      border: 1px solid var(--color-border);
      padding: var(--padding-cell);
      text-align: left;
      overflow-wrap: break-word;
      word-break: break-word;
      overflow: hidden;
      transition: all 0.2s ease;
      font: 11px;
    }}

    th {{
      background-color: var(--color-header);
      cursor: pointer;
      user-select: none;
    }}

    tr:hover {{
      background-color: var(--color-hover);
    }}

    tr.buy {{
      background-color: var(--color-buy);
      color: var(--color-black);
    }}

    tr.sell {{
      background-color: var(--color-sell);
      color: var(--color-black);
    }}

    /* Column collapse behavior */
    th .col-header {{
      display: flex;
      justify-content: space-between;
      align-items: center;
    }}tumi pora

    .collapsed .label {{
      display: none;
    }}

    th.collapsed,
    td.collapsed {{
      width: 11px;
      padding: 0;
      overflow: hidden;
    }}

    td.collapsed {{
      visibility: collapse;
      white-space: nowrap;
    }}

    .toggle-btn::after {{
      content: "▼";
      font-size: 0.9em;
      color: #666;
    }}

    th.collapsed .toggle-btn::after {{
      content: "▶";
    }}

    </style>

    <style>
    /* Mobile support */
    @media (max-width: 768px) {{
      table, thead, tbody, th, td, tr {{
        display: block;
        width: 100%;
          padding-right: 1em;
      }}

      thead {{
        display: none;
      }}

      tr {{
        margin-bottom: var(--row-gap);
        border: 1px solid var(--color-border);
        border-radius: 4px;
        padding: 0.3em;
        background-color: #fff;
      }}

      td {{
        display: flex;
        justify-content: space-between;
        flex-wrap: wrap; /* Allows wrapping to avoid overflow */
        padding: var(--padding-mobile);
        border: none;
        border-bottom: 1px solid var(--color-border);
        width: 100%;
        box-sizing: border-box;
        overflow-wrap: break-word;
      }}

      td:last-child {{
        border-bottom: none;
      }}

      td::before {{
        content: attr(data-label);
        font-weight: bold;
        color: #666;
      }}

      td.collapsed::before {{
        display: none;
      }}

      td.collapsed {{
        display: none;
      }}
    }}
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
                <span class="label">Fees</span>
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
      function toggleColumn(colIndex) {{
        const th = document.querySelector(`th:nth-child(${{colIndex}})`);
        const tds = document.querySelectorAll(`td:nth-child(${{colIndex}})`);

        const isCollapsed = th.classList.toggle("collapsed");
        tds.forEach(td => {{
          if (isCollapsed) td.classList.add("collapsed");
          else td.classList.remove("collapsed");
        }});
      }}
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
    <td data-label="Fees">{:.2f}</td>
  </tr>
)";
