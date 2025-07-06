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
      table-layout: fixed;
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
      color: var(--color-green);
    }}

    tr.signal-caution,
    tr.signal-holdcautiously {{
      color: var(--color-red);
    }}

    tr.signal-mixed {{
      color: var(--color-yellow);
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
      width: 10px;
      padding: 0;
      overflow: hidden;
    }}

    .toggle-btn::after {{
      content: "▼";
      font-size: 0.9em;
      color: #666;
    }}

    th.collapsed .toggle-btn::after {{
      content: "▶";
    }}

    details {{
      cursor: pointer;
    }}

    details summary {{
      font-weight: bold;
      outline: none;
    }}

    .signal-details {{
      padding-top: 0.2em;
      margin-top: 0.3em;
      font-size: 0.92em;
      color: #444;
      white-space: pre-wrap;
    }}

    details summary::marker {{
      content: "▸ ";
    }}

    details[open] summary::marker {{
      content: "▾ ";
    }}

    /* Mobile support */
    @media (max-width: 768px) {{
      table, thead, tbody, th, td, tr {{
        display: block;
        width: 100%;
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
        padding: var(--padding-mobile);
        border: none;
        border-bottom: 1px solid var(--color-border);
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
<h2>Portfolio Overview</h2>
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
</script>
</body>
</html>
)";

inline constexpr std::string_view html_row_template = R"(
  <tr class="{}">
    <td data-label="Signal">{}</td>
    <td data-label="Symbol">{}</td>
    <td data-label="Price">{:.2f}</td>
    <td data-label="EMA9/21">{:.2f} / {:.2f}</td>
    <td data-label="RSI">{:.2f}</td>
    <td data-label="Position">{}</td>
    <td data-label="Stop Loss">{}</td>
  </tr>
)";

inline constexpr std::string_view html_signal_template = R"(
  <details>
    <summary>{}</summary>
    <div class="signal-details">{}</div>
  </details>
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
