import os
import pandas as pd
import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from typing import List, Tuple
import plotly.io as pio
import plotly_themes

import zmq
import ctypes
import signal
import sys

# Define your Nord-inspired color palette (you can change this)
nord_colorway = [
    "#88C0D0",  # bluish (Nord8)
    "#A3BE8C",  # greenish (Nord14)
    "#BF616A",  # redish (Nord11)
    "#D08770",  # orange (Nord12)
    "#B48EAD",  # purple (Nord15)
    "#5E81AC",  # dark blue (Nord10)
]

NORD = {
    "white": "#e5e9f0",
    "black": "#2e3440",
    "best_black": "#17171c",
    "red": "#BF616A",
    "green": "#A3BE8C",
    "blue": "#5E81AC",
    "orange": "#D08770",
    "purple": "#B48EAD",
    "cyan": "#88C0D0",
}

pio.templates.default = "plotly_dark_mono"


def csv_path(symbol: str, time: str, key: str) -> str:
    return f"page/src/{symbol}{time}_{key}.csv"


def load_df(symbol: str, time:str, key: str) -> pd.DataFrame:
    df = pd.read_csv(csv_path(symbol, time, key))
    df["datetime"] = pd.to_datetime(df["datetime"]).dt.strftime("%b %d %H:%M")
    return df


def read_fits(symbol: str, time: str, name: str, max_fits: int = 3) -> List[pd.DataFrame]:
    fits: List[pd.DataFrame] = []
    for i in range(max_fits):
        path = csv_path(symbol, time, f"{name}_fit_{i}")
        if os.path.isfile(path):
            fits.append(load_df(symbol, time, f"{name}_fit_{i}"))
    return fits


def find_cross_points(
    x: pd.Series, fast: pd.Series, slow: pd.Series
) -> Tuple[dict, dict]:
    diff = fast.to_numpy() - slow.to_numpy()
    idx = np.where(np.diff(np.sign(diff)) != 0)[0]
    bullish = {"x": [], "y": []}
    bearish = {"x": [], "y": []}
    for i in idx:
        dt_val = x.iloc[i + 1]
        avg = (fast.iloc[i + 1] + slow.iloc[i + 1]) / 2
        if diff[i] < 0 < diff[i + 1]:
            bullish["x"].append(dt_val)
            bullish["y"].append(avg)
        elif diff[i] > 0 > diff[i + 1]:
            bearish["x"].append(dt_val)
            bearish["y"].append(avg)
    return bullish, bearish


def plot(symbol: str, time: str) -> None:
    trades = load_df(symbol, "", "trades")
    buys = trades[trades["action"] == "BUY"]
    sells = trades[trades["action"] == "SELL"]

    data = pd.read_csv(csv_path(symbol, time, "data"))
    data["datetime"] = pd.to_datetime(data["datetime"]).dt.strftime("%b %d %H:%M")
    dt = pd.Series(data["datetime"])

    price_fits = read_fits(symbol, time, "price")
    rsi_fits = read_fits(symbol, time, "rsi")

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

    # Helper for line style
    ls = lambda color, dash=None, w=1.0: dict(
        color=color, dash=dash or "solid", width=w
    )

    # 1. Price & EMAs
    fig.add_trace(
        go.Scatter(x=dt, y=data["close"], name="Price", line=ls(NORD["white"], w=1.5)),
        row=1,
        col=1,
    )
    fig.add_trace(
        go.Scatter(x=dt, y=data["ema9"], name="EMA 9", line=ls(NORD["red"])),
        row=1,
        col=1,
    )
    fig.add_trace(
        go.Scatter(x=dt, y=data["ema21"], name="EMA 21", line=ls(NORD["blue"])),
        row=1,
        col=1,
    )

    # Volume
    colors = np.where(data["close"] >= data["open"], NORD["green"], NORD["red"])
    fig.add_trace(
        go.Bar(x=dt, y=data["volume"], marker_color=colors, name="Volume", opacity=0.4),
        row=2,
        col=1,
    )

    # Trade markers
    if not buys.empty:
        fig.add_trace(
            go.Scatter(
                x=buys["datetime"],
                y=buys["price"],
                mode="markers",
                name="Buy",
                marker=dict(color=NORD["green"], size=8, symbol="square"),
            ),
            row=1,
            col=1,
        )
    if not sells.empty:
        fig.add_trace(
            go.Scatter(
                x=sells["datetime"],
                y=sells["price"],
                mode="markers",
                name="Sell",
                marker=dict(color=NORD["red"], size=8, symbol="square"),
            ),
            row=1,
            col=1,
        )

    # 2. RSI
    fig.add_trace(
        go.Scatter(x=dt, y=data["rsi"], name="RSI", line=ls(NORD["purple"], w=1.2)),
        row=3,
        col=1,
    )

    fig.add_shape(
        type="line",
        xref="x domain",
        yref="y3",
        x0=0,
        x1=1,
        y0=70,
        y1=70,
        line=ls(NORD["red"]),
    )

    fig.add_shape(
        type="line",
        xref="x domain",
        yref="y3",
        x0=0,
        x1=1,
        y0=30,
        y1=30,
        line=ls(NORD["blue"]),
    )

    # 3. MACD & Signal
    fig.add_trace(
        go.Scatter(x=dt, y=data["macd"], name="MACD", line=ls(NORD["blue"], w=1.2)),
        row=4,
        col=1,
    )
    fig.add_trace(
        go.Scatter(
            x=dt, y=data["signal"], name="Signal", line=ls(NORD["orange"], w=1.2)
        ),
        row=4,
        col=1,
    )

    # Cross points for MACD and EMA
    cross_items: List[Tuple[pd.Series, pd.Series, int, str]] = [
        (pd.Series(data["macd"]), pd.Series(data["signal"]), 4, "MACD"),
        (pd.Series(data["ema9"]), pd.Series(data["ema21"]), 1, "EMA"),
    ]
    for fast, slow, row_idx, label in cross_items:
        bull, bear = find_cross_points(dt, fast, slow)
        fig.add_trace(
            go.Scatter(
                x=bull["x"],
                y=bull["y"],
                mode="markers",
                name=f"{label} Bull",
                marker=dict(color=NORD["green"], size=6),
            ),
            row=row_idx,
            col=1,
        )
        fig.add_trace(
            go.Scatter(
                x=bear["x"],
                y=bear["y"],
                mode="markers",
                name=f"{label} Bear",
                marker=dict(color=NORD["red"], size=6),
            ),
            row=row_idx,
            col=1,
        )

    # Add fit lines
    for fits, r_idx, lbl in [(price_fits, 1, "Price"), (rsi_fits, 3, "RSI")]:
        for i, fdf in enumerate(fits):
            fig.add_trace(
                go.Scatter(
                    x=fdf["datetime"],
                    y=fdf["value"],
                    name=f"{lbl} fit {i}",
                    line=ls(NORD["white"], dash="dash"),
                    visible=True if i == 0 else "legendonly",
                ),
                row=r_idx,
                col=1,
            )

    # Layout and zoom
    fig.update_layout(
        height=900,
        width=1500,
        autosize=False,
        showlegend=True,
        title_text=f"Technical Indicators for {symbol}",
        margin=dict(t=80, b=40),
        legend=dict(
            itemclick="toggle",
            itemdoubleclick="toggleothers",
        ),
    )

    n = min(len(dt), 7 * 7)
    start = len(dt) - n
    fig.update_xaxes(range=[start, len(dt) - 1], tickangle=45, type="category")

    # Dynamic y-axis limits
    def set_ylim(series: pd.Series | pd.DataFrame, row_idx: int, pad: float = 0.02):
        sub = series.iloc[start:]
        span = max(abs(sub.max()), abs(sub.min()))
        lo = sub.min() - pad * span
        hi = sub.max() + pad * span
        fig.update_yaxes(range=[lo, hi], row=row_idx, col=1)

    set_ylim(data["close"], 1)
    set_ylim(data["macd"], 4)

    # Save as HTML
    fig.write_html(f"page/public/{symbol}{time}_plot.html")


def main(port):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PULL)
    socket.bind(f"tcp://127.0.0.1:{port}")  # Match C++ port

    print("Plot daemon running...")
    while True:
        message = socket.recv_json()  # Expecting a list of tickers
        tickers = message.get("tickers", [])
        if isinstance(tickers, list) and all(isinstance(t, str) for t in tickers):
            for ticker in tickers:
                for time in ["_1h", "_4h", "_1d"]:
                    plot(str(ticker), time)
        else:
            print("No tickers received")


def shutdown(sig, frame):
    print("Daemon shutting down")
    sys.exit(0)


if __name__ == "__main__":
    import sys

    port = "5555"
    if len(sys.argv) > 1:
        port = sys.argv[1]

    # Send SIGTERM to self if parent dies
    libc = ctypes.CDLL("libc.so.6")
    libc.prctl(1, signal.SIGTERM)  # PR_SET_PDEATHSIG

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    main(port)
