import sys
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

symbol = sys.argv[1]

data_df = pd.read_csv("data.csv")
datetime = data_df["datetime"]

# Create subplots
fig = make_subplots(
    rows=3,
    cols=1,
    shared_xaxes=True,
    vertical_spacing=0.05,
    row_heights=[0.5, 0.25, 0.25],
    subplot_titles=(f"{symbol} Price with EMA9 and EMA21", "RSI (1h)",
                    "MACD (1h)"),
)


# Common line style function
def line_style(color, dash=None, width=1.2):
    return dict(color=color, width=width, dash=dash)


# --- 1. Price and EMA ---
fig.add_trace(
    go.Scatter(x=datetime,
               y=data_df["price"],
               name="Price",
               line=line_style("black")),
    row=1,
    col=1,
)

fig.add_trace(
    go.Scatter(x=datetime,
               y=data_df["ema9"],
               name="EMA 9",
               line=line_style("red", dash="dash")),
    row=1,
    col=1,
)

fig.add_trace(
    go.Scatter(
        x=datetime,
        y=data_df["ema21"],
        name="EMA 21",
        line=line_style("blue", dash="dash"),
    ),
    row=1,
    col=1,
)

# --- 2. RSI ---
fig.add_trace(
    go.Scatter(x=datetime,
               y=data_df["rsi"],
               name="RSI (1h)",
               line=line_style("purple")),
    row=2,
    col=1,
)

fig.add_hline(y=70, line=dict(color="red", dash="dash"), row=2, col=1)
fig.add_hline(y=30, line=dict(color="green", dash="dash"), row=2, col=1)

# --- 3. MACD ---
fig.add_trace(
    go.Scatter(x=datetime,
               y=data_df["macd"],
               name="MACD",
               line=line_style("orange")),
    row=3,
    col=1,
)

fig.add_trace(
    go.Scatter(x=datetime,
               y=data_df["signal"],
               name="Signal",
               line=line_style("blue")),
    row=3,
    col=1,
)

# Layout
fig.update_layout(
    height=800,
    width=1200,
    showlegend=True,
    title_text=f"Technical Indicators for {symbol}",
    margin=dict(t=80, b=40),
)

fig.update_xaxes(tickangle=45)

# Save as interactive HTML
output_file = f"{symbol}.html"
fig.write_html(output_file)

print(f"Saved interactive chart to {output_file}")
