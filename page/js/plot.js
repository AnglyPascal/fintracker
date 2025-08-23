import {
  DateTime
} from 'https://cdn.jsdelivr.net/npm/luxon@3.7.1/+esm';

// Nord color palette
const NORD = {
  white: "#e5e9f0",
  black: "#2e3440",
  gray: "#4c566a",
  best_black: "#17171c",
  red: "#BF616A",
  green: "#A3BE8C",
  teal: "#8fbcbb",
  blue: "#5E81AC",
  dark_blue: "#2c4057",
  orange: "#D08770",
  dark_yellow: "#84724e",
  yellow: "#ebcb8b",
  purple: "#B48EAD",
  cyan: "#88C0D0",

  support: "#2c4057",
  resistance: "#84724e",
};

// Global variables
let currentTimeframe = '1h';

function csvPath(symbol, time, key) {
  return `src/${symbol}${time}_${key}.csv`;
}

function jsonPath(symbol, time, key) {
  return `src/${symbol}${time}_${key}.json`;
}

function formatDateTime(dateStr) {
  const date = new Date(dateStr);
  return date.toLocaleDateString('en-US', {
    month: 'short',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
  });
}

Date.prototype.getWeek = function() {
  const date = new Date(this.getTime());
  date.setHours(0, 0, 0, 0);
  // Thursday in current week decides the year.
  date.setDate(date.getDate() + 3 - (date.getDay() + 6) % 7);
  const week1 = new Date(date.getFullYear(), 0, 4);
  return 1 + Math.round(((date - week1) / 86400000
    - 3 + (week1.getDay() + 6) % 7) / 7);
};

function isFirstCandleOfWeek(dtStr) {
  const dt = new Date(dtStr); // Already NYC local time
  const day = dt.getDay();     // 0 = Sunday, 1 = Monday, ...
  const hour = dt.getHours();
  const minute = dt.getMinutes();
  return day === 1 && hour === 9 && minute === 30;
}

async function loadCSV(path) {
  try {
    const response = await fetch(path);
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    const csvText = await response.text();

    return new Promise((resolve, reject) => {
      Papa.parse(csvText, {
        header: true,
        dynamicTyping: true,
        skipEmptyLines: true,
        complete: function(results) {
          if (results.errors.length > 0) {
            reject(new Error('CSV parsing error: ' +
              results.errors[0].message));
          } else {
            // Format datetime column
            const data = results.data.map(row => ({
              ...row,
              datetime: row.datetime ?
                formatDateTime(row.datetime) : row.datetime,
              startOfWeek: isFirstCandleOfWeek(row.datetime)
            }));
            resolve(data);
          }
        },
        error: function(error) {
          reject(error);
        }
      });
    });
  } catch (error) {
    console.error(`Failed to load CSV from ${path}:`, error);
    return null;
  }
}

async function loadDF(symbol, time, key) {
  const path = csvPath(symbol, time, key);
  return await loadCSV(path);
}

async function readFits(symbol, time, name) {
  const rawFits = await loadDF(symbol, time, 'trends');

  const fits = rawFits
    .filter(row => row.plot === name)
    .map(row =>
      [
        {
          datetime: formatDateTime(row.d0),
          value: row.v0,
        },
        {
          datetime: formatDateTime(row.d1),
          value: row.v1,
        }
      ]
    );

  return fits;
}

function findCrossPoints(x, fast, slow) {
  const bullish = {
    x: [],
    y: []
  };
  const bearish = {
    x: [],
    y: []
  };

  for (let i = 0; i < fast.length - 1; i++) {
    const diff1 = fast[i] - slow[i];
    const diff2 = fast[i + 1] - slow[i + 1];

    if (diff1 < 0 && diff2 > 0) {
      // Bullish cross
      bullish.x.push(x[i + 1]);
      bullish.y.push((fast[i + 1] + slow[i + 1]) / 2);
    } else if (diff1 > 0 && diff2 < 0) {
      // Bearish cross
      bearish.x.push(x[i + 1]);
      bearish.y.push((fast[i + 1] + slow[i + 1]) / 2);
    }
  }

  return {
    bullish,
    bearish
  };
}

function createLineStyle(color, dash = 'solid', width = 1.0) {
  return {
    color,
    dash,
    width
  };
}

function setYAxisRange(data, pad = 0.02) {
  const values = data.filter(v => v != null && !isNaN(v));
  if (values.length === 0) return {};

  const min = Math.min(...values);
  const max = Math.max(...values);
  const span = Math.max(Math.abs(max), Math.abs(min));
  const lo = min - pad * span;
  const hi = max + pad * span;

  return {
    range: [lo, hi]
  };
}

function createPlotlyLayout({
  symbol,
  timeframe,
  zoomRange,
  data,
  start,
  NORD,
  heights,
  srShapes
}) {
  const subplots = [{
    key: 'yaxis',
    title: `Price and EMA`,
    rangeFn: () => setYAxisRange(data.slice(start).map(row => row.close))
  },
  {
    key: 'yaxis2',
    title: 'Volume',
    rangeFn: () => setYAxisRange(data.slice(start).map(row => row.volume))
  },
  {
    key: 'yaxis3',
    title: `RSI (${timeframe})`,
    fixedRange: [21, 79],
    // rangeFn: () => setYAxisRange(data.slice(start).map(row => row.rsi))
  },
  {
    key: 'yaxis4',
    title: `MACD (${timeframe})`,
    rangeFn: () => setYAxisRange(data.slice(start).map(row => row.macd))
  }
  ];

  const spacing = 0.075;
  const numPlots = subplots.length;

  if (!heights || heights.length !== numPlots) {
    throw new Error(`Invalid 'heights' array: must provide ${numPlots} values`);
  }

  const heightSum = heights.reduce((a, b) => a + b, 0);
  const totalHeight = 1 - spacing * (numPlots - 1);

  const scaledHeights = heights.map(h => h / heightSum * totalHeight);
  scaledHeights.reverse();

  // Compute domains from bottom up
  const domains = [];
  let currentBottom = 0;
  for (let i = 0; i < numPlots; i++) {
    const h = scaledHeights[i];
    domains.push([currentBottom, currentBottom + h]);
    currentBottom += h + spacing;
  }
  domains.reverse(); // match top to bottom order

  // Assign domains and ranges to each yaxis
  const yaxes = {};
  subplots.forEach((subplot, i) => {
    const domain = domains[i];
    const axis = {
      domain,
      anchor: 'x',
      title: subplot.title,
      color: NORD.white
    };
    if (subplot.fixedRange)
      axis.range = subplot.fixedRange;
    if (subplot.rangeFn)
      Object.assign(axis, subplot.rangeFn());
    if (subplot.key == 'yaxis2')
      axis['nticks'] = 3;
    yaxes[subplot.key] = axis;
  });

  const dividers = domains.slice(1).map(([_, bottom]) => ({
    type: 'line',
    xref: 'paper',
    yref: 'paper',
    x0: 0,
    x1: 1,
    y0: bottom + spacing / 1.5,
    y1: bottom + spacing / 1.5,
    line: {
      color: NORD.black,
      width: 5
    }
  }));

  const rsiLines = [{
    y: 70,
    color: NORD.red,
    width: 1.2,
  },
  {
    y: 50,
    color: NORD.gray,
    width: 1.7,
  },
  {
    y: 30,
    color: NORD.blue,
    width: 1.2,
  }
  ].map(({
    y,
    color,
    width
  }) => ({
    type: 'line',
    xref: 'paper',
    yref: 'y3',
    x0: 0,
    x1: 1,
    y0: y,
    y1: y,
    line: {
      color,
      width: width
    },
    layer: 'below',
  }));

  return {
    height: 890,
    width: 1500,
    autosize: false,
    showlegend: true,
    title: {
      text: `${symbol} (${timeframe.toUpperCase()})`,
      x: 0.5,
      y: 1.2,
      xanchor: 'center',
      yanchor: 'top',
      font: {
        size: 18
      }
    },
    margin: {
      t: 150,
      b: 80,
      l: 50,
      r: 50
    },
    legend: {
      itemclick: 'toggle',
      itemdoubleclick: 'toggleothers',
      orientation: 'h',
      x: 0.5,
      y: 1.0,
      xanchor: 'center',
      yanchor: 'bottom',
      font: {
        size: 14,
      }
    },
    plot_bgcolor: NORD.best_black,
    paper_bgcolor: "rgba(23, 23, 28, 0)",
    font: {
      size: 14,
      color: NORD.white,
      family: 'Share Tech Mono, monospace',
    },
    hoverlabel: {
      font: {
        family: 'Share Tech Mono, monospace',
        size: 16
      }
    },
    xaxis: {
      domain: [0, 1],
      anchor: 'y4',
      range: zoomRange,
      tickangle: 60,
      nticks: 50,
      type: 'category',
      color: NORD.white,
      tickfont: {
        family: 'Share Tech',
        size: 14,
      }
    },
    ...yaxes,
    shapes: [...dividers, ...rsiLines, ...srShapes]
  };
}

function claspToTimeframe(datetimeStr, timeframe) {
  if (timeframe === '1h')
    return datetimeStr;

  const datetimeFmt = "LLL dd, HH:mm";

  const dt = DateTime.fromFormat(datetimeStr, datetimeFmt, {
    zone: 'America/New_York'
  });

  const marketOpen = dt.set({
    hour: 9,
    minute: 30,
    second: 0,
    millisecond: 0
  });
  if (dt < marketOpen)
    return marketOpen.toISO();

  const diffMinutes = dt.diff(marketOpen, 'minutes').minutes;

  let clasped;

  if (timeframe === '4h') {
    const intervals = Math.floor(diffMinutes / 240);
    clasped = marketOpen.plus({
      minutes: intervals * 240
    });
  } else if (timeframe === '1d') {
    clasped = marketOpen;
  } else {
    console.log(`Unsupported timeframe: ${timeframe}`);
  }

  return clasped.toFormat(datetimeFmt);
}

async function loadSupportResistanceTraces(path, dt, price) {
  const data = await fetch(path).then(r => r.json());

  const opacity = (row) => {
    const in_zone = row.lo < price && price <= row.hi;
    if (in_zone)
      return 1;

    const conf = row.confidence;
    const weight = 1 / 0.1;
    const scaled = (Math.exp(weight * conf) - 1) / (Math.exp(weight) - 1);
    return scaled * 0.7;
  };

  const makeTrace = (row, sup, swings) => {
    swings.push(...row.sps);
    return {
      type: 'rect',
      xref: 'x',
      yref: 'y',
      x0: dt[0],
      x1: dt[dt.length - 1],
      y0: row.lo,
      y1: row.hi,
      fillcolor: sup ? NORD.support : NORD.resistance,
      opacity: opacity(row),
      line: { width: 0 },
      layer: 'below',
      name: sup ? "Supports" : "Resistances",
    };
  };

  const swing_lows = [];
  const swing_highs = [];
  const res = [
    ...(data.support || []).map(row => makeTrace(row, true, swing_lows)),
    ...(data.resistance || []).map(row => makeTrace(row, false, swing_highs)),
  ];

  const swings = [
    {
      x: swing_lows.map(t => formatDateTime(t.tp)),
      y: swing_lows.map(t => t.price),
      type: 'scatter',
      mode: 'markers',
      marker: {
        color: NORD.blue,
        size: 10,
        symbol: 'square'
      },
      xaxis: 'x',
      yaxis: 'y',
      showlegend: false,
    },
    {
      x: swing_highs.map(t => formatDateTime(t.tp)),
      y: swing_highs.map(t => t.price),
      type: 'scatter',
      mode: 'markers',
      marker: {
        color: NORD.yellow,
        size: 10,
        symbol: 'square'
      },
      xaxis: 'x',
      yaxis: 'y',
      showlegend: false,
    }
  ];

  return [res, swings];
}

async function plotChart(symbol) {
  const timeframe = currentTimeframe;

  try {
    const timeframeMapped = `_${timeframe}`;

    // Load main data
    const data = await loadDF(symbol, timeframeMapped, 'data');
    if (!data || data.length === 0) {
      throw new Error(`No data found for ${symbol}${timeframeMapped}`);
    }

    // Load trades data
    const raw_trades = await loadDF(symbol, '', 'trades');
    const trades = raw_trades.map(trade => ({
      ...trade,
      datetime: claspToTimeframe(trade.datetime, timeframe)
    }));
    const buys = trades ? trades.filter(t => t.action === 'BUY') : [];
    const sells = trades ? trades.filter(t => t.action === 'SELL') : [];

    // Load fit data
    const priceFits = await readFits(symbol, timeframeMapped, 'price');
    const rsiFits = await readFits(symbol, timeframeMapped, 'rsi');

    // Extract datetime series
    const dt = data.map(row => row.datetime);

    const lastPrice = data[data.length - 1].close;
    const srPath = jsonPath(symbol, timeframeMapped, 'support_resistance');
    const [srShapes, _] = await loadSupportResistanceTraces(srPath, dt, lastPrice);

    // Create traces
    const traces = [];

    const weekStartDates = [];

    data.forEach(row => {
      if (row.startOfWeek)
        weekStartDates.push(row.datetime);
    });

    // List of subplot y-axis targets
    const yAxes = ['y', 'y2', 'y3', 'y4'];

    const weekLines = [];
    for (const d of weekStartDates) {
      for (const yaxis of yAxes) {
        weekLines.push({
          type: 'line',
          x0: d,
          x1: d,
          y0: -1e9,
          y1: 1e9, // effectively "infinite"
          mode: 'lines',
          line: createLineStyle(NORD.red, 'solid', 1.5),
          hoverinfo: 'skip',
          xaxis: 'x',
          yaxis: yaxis,
          showlegend: false,
        });
      }
    }

    // 1. Price & EMAs
    traces.push({
      x: dt,
      y: data.map(row => row.ema9),
      type: 'scatter',
      mode: 'lines',
      name: 'EMA 9',
      line: createLineStyle(NORD.red, 'solid', 1.5),
      xaxis: 'x',
      yaxis: 'y',
      hoverinfo: 'none',
    });

    traces.push({
      x: dt,
      y: data.map(row => row.ema21),
      type: 'scatter',
      mode: 'lines',
      name: 'EMA 21',
      line: createLineStyle(NORD.blue, 'solid', 1.6),
      xaxis: 'x',
      yaxis: 'y',
      hoverinfo: 'none',
    });

    const priceHoverText = (row) => {
      const volStr = Intl.NumberFormat('en-US', {
        notation: "compact",
        maximumFractionDigits: 1
      }).format(row.volume);

      return `<b>${row.close}</b>, ${row.low} — ${row.high}<br>` +
        `<b>EMA 9/21</b>: ${row.ema9}/${row.ema21}<br>` +
        `<b>Vol</b>: ${volStr}, <b>RSI</b>: ${row.rsi}<br>`
        ;
    }

    traces.push({
      x: dt,
      y: data.map(row => row.close),
      type: 'scatter',
      mode: 'lines',
      name: 'Price',
      line: createLineStyle(NORD.white, 'solid', 1.8),
      xaxis: 'x',
      yaxis: 'y',
      hovertext: data.map(row => priceHoverText(row)),
      hovertemplate: `%{x}<br>%{hovertext}<extra></extra>`,
    });

    // Volume bars
    const colors = data.map(row => row.close >= row.open ? NORD.green : NORD.red);
    traces.push({
      x: dt,
      y: data.map(row => row.volume),
      type: 'bar',
      name: 'Volume',
      marker: {
        color: colors,
        opacity: 0.3
      },
      xaxis: 'x',
      yaxis: 'y2',
      hovertemplate: `%{x} <b>%{y}</b><extra></extra>`,
    });

    // Define a lambda function to create trade traces
    const createTradeTrace = (trades, name, color) => ({
      x: trades.map(t => t.datetime),
      y: trades.map(t => t.price),
      customdata: trades.map(t => [
        t.remark ? t.remark : "",
        t.rating === 0 ? "" : `(${t.rating})`
      ]),
      type: 'scatter',
      mode: 'markers',
      name: name,
      marker: {
        color: color,
        size: 10,
        symbol: 'diamond-open'
      },
      hovertemplate:
        `%{x} <b>%{y}</b> ` +
        `<b>%{customdata[1]}</b><br><b>%{customdata[0]}</b>` +
        `<extra></extra>`,
      xaxis: 'x',
      yaxis: 'y',
      showlegend: false,
    });

    // Push Buy and Sell traces using the lambda
    traces.push(createTradeTrace(buys, 'Buy', NORD.green));
    traces.push(createTradeTrace(sells, 'Sell', NORD.red));

    // RSI
    traces.push({
      x: dt,
      y: data.map(row => row.rsi),
      type: 'scatter',
      mode: 'lines',
      name: 'RSI',
      line: createLineStyle(NORD.purple, 'solid', 2),
      xaxis: 'x',
      hovertemplate: `%{x}, <b>%{y}</b><extra></extra>`,
      yaxis: 'y3'
    });

    // MACD & Signal
    traces.push({
      x: dt,
      y: data.map(row => row.macd),
      type: 'scatter',
      mode: 'lines',
      name: 'MACD',
      line: createLineStyle(NORD.blue, 'solid', 2),
      xaxis: 'x',
      yaxis: 'y4',
      hovertext: data.map(row => `<b>${row.macd}</b> ${row.signal}`),
      hovertemplate: `%{x} %{hovertext}<extra></extra>`,
    });

    traces.push({
      x: dt,
      y: data.map(row => row.signal),
      type: 'scatter',
      mode: 'lines',
      name: 'Signal',
      line: createLineStyle(NORD.orange, 'solid', 1.6),
      xaxis: 'x',
      yaxis: 'y4',
      hoverinfo: 'none',
    });

    const macdHistogram = data.map(row => row.macd - row.signal);

    const colorWithOpacity = (hexColor, opacity) => {
      const r = parseInt(hexColor.slice(1, 3), 16);
      const g = parseInt(hexColor.slice(3, 5), 16);
      const b = parseInt(hexColor.slice(5, 7), 16);
      return `rgba(${r}, ${g}, ${b}, ${opacity})`;
    };

    const histogramColors = macdHistogram.map((val, i) => {
      if (i === 0) {
        return val >= 0
          ? colorWithOpacity(NORD.green, 0.8)
          : colorWithOpacity(NORD.red, 0.8);
      }

      const prevVal = macdHistogram[i - 1];
      const bullish = val >= 0 ? (val > prevVal) : (val < prevVal);
      const baseColor = val >= 0 ? NORD.green : NORD.red;

      return colorWithOpacity(baseColor, bullish ? 0.5 : 0.25);
    });

    // MACD Histogram (Bars)
    traces.push({
      x: dt,
      y: macdHistogram,
      type: 'bar',
      name: 'Histogram',
      marker: {
        color: histogramColors,
        line: { width: 0 }, // No border
      },
      xaxis: 'x',
      yaxis: 'y4',
      hovertext: data.map((row, i) =>
        `<b>${(row.macd - row.signal).toFixed(2)}<b> ` +
        `${i > 0 ? (macdHistogram[i] > macdHistogram[i - 1] ? "↗" : "↘") : "-"}`
      ),
      hovertemplate: `%{x} %{hovertext}<extra></extra>`,
    });

    // Cross points
    const crossItems = [{
      fast: data.map(row => row.macd),
      slow: data.map(row => row.signal),
      yaxis: 'y4',
      label: 'MACD',
      symbol: 'circle',
      size: 8,
    },
    {
      fast: data.map(row => row.ema9),
      slow: data.map(row => row.ema21),
      yaxis: 'y',
      label: 'EMA',
      symbol: 'circle',
      size: 8,
    },
    {
      fast: data.map(row => row.close),
      slow: data.map(row => row.ema21),
      yaxis: 'y',
      label: 'Price',
      symbol: 'x',
      size: 8,
    },
    {
      fast: data.map(row => row.rsi),
      slow: data.map(_ => 50),
      yaxis: 'y3',
      label: 'RSI',
      symbol: 'circle',
      size: 6,
    }
    ];

    crossItems.forEach(({
      fast,
      slow,
      yaxis,
      label,
      symbol,
      size
    }) => {
      const {
        bullish,
        bearish
      } = findCrossPoints(dt, fast, slow);

      if (bullish.x.length > 0) {
        traces.push({
          x: bullish.x,
          y: bullish.y,
          type: 'scatter',
          mode: 'markers',
          name: `${label} Bull`,
          marker: {
            color: NORD.green,
            size: size,
            symbol: symbol,
          },
          hovertext: bullish.x.map((xVal, _) => `${label} bull: ${xVal}`),
          hovertemplate: `%{hovertext}<extra></extra>`,
          xaxis: 'x',
          yaxis: yaxis,
          showlegend: false,
        });
      }

      if (bearish.x.length > 0) {
        traces.push({
          x: bearish.x,
          y: bearish.y,
          type: 'scatter',
          mode: 'markers',
          name: `${label} Bear`,
          marker: {
            color: NORD.red,
            size: size,
            symbol: symbol,
          },
          hovertext: bullish.x.map((xVal, _) => `${label} bear: ${xVal}`),
          hovertemplate: `%{hovertext}<extra></extra>`,
          xaxis: 'x',
          yaxis: yaxis,
          showlegend: false,
        });
      }
    });

    const plotFit = (fit, i, name, yaxis) => {
      const color = fit[0].value > fit[1].value ? NORD.yellow : NORD.teal;
      traces.push({
        x: fit.map(row => row.datetime),
        y: fit.map(row => row.value),
        type: 'scatter',
        mode: 'lines',
        name: `${name} fit ${i}`,
        line: createLineStyle(color, '12px,15px', 1.5),
        visible: i === 0 ? true : 'legendonly',
        xaxis: 'x',
        yaxis: yaxis,
        hoverinfo: 'none',
      });
    };

    // Add fit lines
    priceFits.forEach((fit, i) => { plotFit(fit, i, "Price", "y"); });
    rsiFits.forEach((fit, i) => { plotFit(fit, i, "RSI", "y3"); });

    // traces.push(...srSwings);
    traces.push(...weekLines);

    const n = Math.min(dt.length, 40);
    const start = dt.length - n;
    const zoomRange = [start, dt.length - 1];

    // Layout
    const layout = createPlotlyLayout({
      symbol,
      timeframe,
      zoomRange,
      data,
      start,
      NORD,
      heights: [4, 1, 2, 1.8],
      srShapes,
    });

    // Plot the chart
    Plotly.newPlot('chart', traces, layout, {
      displayModeBar: true,
      displaylogo: false,
      modeBarButtonsToRemove: ['pan2d', 'lasso2d'],
      responsive: true
    });

  } catch (error) {
    console.error('Error plotting chart:', error);
    document.getElementById('chart').innerHTML =
      `<div style="color: #bf616a; font-weight: bold;">
                Error: ${error.message}
            </div>`;
  }
}

export async function loadContent(timeframe) {
  try {
    const html_fname = `${ticker}_${timeframe}.html`;
    const html_res = await fetch(`./${html_fname}`);
    if (!html_res.ok) {
      throw new Error(`HTTP ${html_res.status}: ${html_res.statusText}`);
    }
    const html = await html_res.text();
    if (html.trim() === '') {
      throw new Error('Loaded content is empty');
    }
    document.getElementById('content').innerHTML = html;
  } catch (error) {
    console.warn('Could not load HTML content:', error.message);
  }

  document.querySelectorAll('.button-container button').forEach(btn => {
    btn.classList.remove('active');
  });
  document.getElementById(`btn${timeframe}`).classList.add('active');

  currentTimeframe = timeframe;
  plotChart(ticker);

  document.querySelectorAll('.thing-abbr').forEach(div => {
    const hoverDiv = div.querySelector('.thing-desc');

    div.addEventListener('mouseenter', () => {
      const rect = div.getBoundingClientRect();
      hoverDiv.style.display = 'block';

      const hoverRect = hoverDiv.getBoundingClientRect();
      const viewportWidth = window.innerWidth;
      const viewportHeight = window.innerHeight;

      let left, top;

      if (rect.right + hoverRect.width <= viewportWidth)
        left = rect.right; // place right
      else if (rect.left - hoverRect.width >= 0)
        left = rect.left - hoverRect.width; // place left
      else
        left = viewportWidth - hoverRect.width; // place aligned 

      if (rect.bottom + hoverRect.height <= viewportHeight)
        top = rect.bottom; // place bottom
      else if (rect.top - hoverRect.height >= 0)
        top = rect.top - hoverRect.height; // place top
      else
        top = rect.top; // place aligned

      hoverDiv.style.left = left + 'px';
      hoverDiv.style.top = top + 'px';
    });

    div.addEventListener('mouseleave', () => {
      hoverDiv.style.display = 'none';
    });
  });
}

const plotlyJS =
  'https://cdnjs.cloudflare.com/ajax/libs/plotly.js/2.26.0/plotly.min.js';
const papaJS =
  'https://cdnjs.cloudflare.com/ajax/libs/PapaParse/5.4.1/papaparse.min.js';

// Initialize chart when page loads
document.addEventListener('DOMContentLoaded', function() {
  const plotlyScript = document.createElement('script');
  plotlyScript.src = plotlyJS;
  plotlyScript.onload = function() {
    const papaScript = document.createElement('script');
    papaScript.src = papaJS;
    papaScript.onload = function() {
      loadContent('1h');

      // Add button event listeners programmatically
      document.getElementById('btn1h').addEventListener(
        'click', () => loadContent('1h'));
      document.getElementById('btn4h').addEventListener(
        'click', () => loadContent('4h'));
      document.getElementById('btn1d').addEventListener(
        'click', () => loadContent('1d'));
    };
    document.head.appendChild(papaScript);
  };
  document.head.appendChild(plotlyScript);
});

