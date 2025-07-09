import sys
import os.path
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import numpy as np

symbol = sys.argv[1]

trades_df = pd.read_csv("page/trades.csv")
trades_df["datetime"] = pd.to_datetime(trades_df["datetime"]).dt.strftime("%b %d %H:%M")

buy_trades = trades_df[trades_df["action"] == "BUY"]
sell_trades = trades_df[trades_df["action"] == "SELL"]

data_df = pd.read_csv("page/data.csv")
datetime = pd.to_datetime(data_df["datetime"]).dt.strftime("%b %d %H:%M")


def read_fits(name, n_fits=3):
    fits = []
    for i in range(n_fits):
        fname = f"page/{name}_fit_{i}.csv"
        if os.path.isfile(fname):
            fit = pd.read_csv(fname)
            fit["datetime"] = pd.to_datetime(fit["datetime"]).dt.strftime("%b %d %H:%M")
            fits.append(fit)
    return fits


price_fits = read_fits("price")
rsi_fits = read_fits("rsi")

fig = make_subplots(
    rows=4,
    cols=1,
    shared_xaxes=True,
    vertical_spacing=0.04,
    row_heights=[0.45, 0.1, 0.2, 0.2],
    subplot_titles=(
        f"{symbol} Price with EMA9 and EMA21",
        "Volume",
        "RSI (1h)",
        "MACD (1h)",
    ),
)


# Common line style function
def line_style(color, dash=None, width=1.2):
    return dict(color=color, width=width, dash=dash)


# --- 1. Price and EMA ---
fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["close"],
        name="Price",
        line=line_style("black", width=1.5),
    ),  # slightly bolder
    row=1,
    col=1,
)

fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["ema9"],
        name="EMA 9",
        line=line_style("red", width=1.0),
    ),  # red, thin, straight line
    row=1,
    col=1,
)

fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["ema21"],
        name="EMA 21",
        line=line_style("blue", width=1.0),  # blue, thin, straight line
    ),
    row=1,
    col=1,
)

# Normalize volume to [0, 1] so it fits within ~1/3 of price chart
max_volume = data_df["volume"].max()
volume_scale = 0.33 * data_df["close"].max() / max_volume
colors = np.where(data_df["close"] >= data_df["open"], "green", "red")

fig.add_trace(
    go.Bar(
        x=datetime,
        y=data_df["volume"],
        marker_color=colors,
        name="Volume",
        opacity=0.4,
    ),
    row=2,
    col=1,
)


# Add trades

if not buy_trades.empty:
    fig.add_trace(
        go.Scatter(
            x=buy_trades["datetime"],
            y=buy_trades["price"],
            mode="markers",
            name="Buy Trades",
            marker=dict(color="green", size=8, symbol="square"),
        ),
        row=1,
        col=1,
    )

if not sell_trades.empty:
    fig.add_trace(
        go.Scatter(
            x=sell_trades["datetime"],
            y=sell_trades["price"],
            mode="markers",
            name="Sell Trades",
            marker=dict(color="red", size=8, symbol="square"),
        ),
        row=1,
        col=1,
    )


# --- 2. RSI ---
fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["rsi"],
        name="RSI (1h)",
        line=line_style("purple", width=1.2),
    ),  # purple, default width
    row=3,
    col=1,
)

fig.add_hline(y=70, line=line_style("red", width=1.0, dash="dash"), row=3, col=1)
fig.add_hline(y=30, line=line_style("blue", width=1.0, dash="dash"), row=3, col=1)

# --- 3. MACD ---
fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["macd"],
        name="MACD",
        line=line_style("blue", width=1.2),
    ),  # blue, straight
    row=4,
    col=1,
)

fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["signal"],
        name="Signal",
        line=line_style("orange", width=1.2),
    ),  # orange, straight
    row=4,
    col=1,
)

# Crosspoints


def find_cross_points_with_polarity(x, y_fast, y_slow):
    """Return bullish and bearish cross points separately"""
    bullish_x, bullish_y = [], []
    bearish_x, bearish_y = [], []

    diff = np.array(y_fast) - np.array(y_slow)
    cross_indices = np.where(np.diff(np.sign(diff)) != 0)[0]

    for i in cross_indices:
        x_val = x.iloc[i + 1]  # Use .iloc for pandas Series
        y_avg = (y_fast.iloc[i + 1] + y_slow.iloc[i + 1]) / 2

        if diff[i] < 0 and diff[i + 1] > 0:
            bullish_x.append(x_val)
            bullish_y.append(y_avg)
        elif diff[i] > 0 and diff[i + 1] < 0:
            bearish_x.append(x_val)
            bearish_y.append(y_avg)

    return (bullish_x, bullish_y), (bearish_x, bearish_y)


# --- MACD/Signal Crosses ---
macd = find_cross_points_with_polarity(datetime, data_df["macd"], data_df["signal"])
(macd_bull_x, macd_bull_y), (macd_bear_x, macd_bear_y) = macd

fig.add_trace(
    go.Scatter(
        x=macd_bull_x,
        y=macd_bull_y,
        mode="markers",
        name="MACD Bullish",
        marker=dict(color="green", size=6, symbol="circle"),
    ),
    row=4,
    col=1,
)

fig.add_trace(
    go.Scatter(
        x=macd_bear_x,
        y=macd_bear_y,
        mode="markers",
        name="MACD Bearish",
        marker=dict(color="red", size=6, symbol="circle"),
    ),
    row=4,
    col=1,
)

# --- EMA9/EMA21 Crosses ---
ema = find_cross_points_with_polarity(datetime, data_df["ema9"], data_df["ema21"])
(ema_bull_x, ema_bull_y), (ema_bear_x, ema_bear_y) = ema

fig.add_trace(
    go.Scatter(
        x=ema_bull_x,
        y=ema_bull_y,
        mode="markers",
        name="EMA Bullish",
        marker=dict(color="green", size=6, symbol="circle"),
    ),
    row=1,
    col=1,
)

fig.add_trace(
    go.Scatter(
        x=ema_bear_x,
        y=ema_bear_y,
        mode="markers",
        name="EMA Bearish",
        marker=dict(color="red", size=6, symbol="circle"),
    ),
    row=1,
    col=1,
)

# Fits


def trace_fit(fit, row, col, name, idx, show=True):
    fig.add_trace(
        go.Scatter(
            x=fit["datetime"],
            y=fit["value"],
            name=f"{name} fits {idx}",
            line=line_style("black", dash="dash"),
            visible=True if show else "legendonly",
        ),
        row=row,
        col=col,
    )


def add_fits(fits, row, col, name):
    if len(fits) == 0:
        return
    trace_fit(fits[0], row, col, name, 0, True)
    for i in range(1, len(fits)):
        trace_fit(fits[i], row, col, name, i, False)


add_fits(price_fits, 1, 1, "Price")
add_fits(rsi_fits, 3, 1, "RSI")

# Layout
fig.update_layout(
    height=900,
    width=1500,
    showlegend=True,
    title_text=f"Technical Indicators for {symbol}",
    margin=dict(t=80, b=40),
    legend=dict(
        itemclick="toggle",
        itemdoubleclick="toggleothers",
    ),
)

rows_per_day = 7
n_days = 7
n_rows = min(len(datetime), rows_per_day * n_days)

start_idx = len(datetime) - n_rows
end_idx = len(datetime) - 1

fig.update_xaxes(
    range=[start_idx, end_idx],
    tickformat="%b %d %H:%M",
    tickangle=45,
    type="category",
)

# Get the subset of data for y-axis zooming
subset_price = data_df["close"].iloc[start_idx : end_idx + 1]
smax = abs(subset_price.max())
ymin = subset_price.min() - smax * 0.02
ymax = subset_price.max() + smax * 0.02
fig.update_yaxes(range=[ymin, ymax], row=1, col=1)

subset_macd = data_df["macd"].iloc[start_idx : end_idx + 1]
smax = max(abs(subset_macd.max()), abs(subset_macd.min()))
ymin = subset_macd.min() - smax * 0.15
ymax = subset_macd.max() + smax * 0.15
fig.update_yaxes(range=[ymin, ymax], row=4, col=1)

subset_volume = data_df["volume"].iloc[start_idx : end_idx + 1]
smax = max(abs(subset_volume.max()), abs(subset_volume.min()))
ymin = subset_volume.min()
ymax = subset_volume.max() + smax * 0.05
fig.update_yaxes(range=[ymin, ymax], row=2, col=1)

# Save as interactive HTML
output_file = f"page/{symbol}.html"
fig.write_html(output_file)
