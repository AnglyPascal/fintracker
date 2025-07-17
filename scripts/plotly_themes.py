import plotly.graph_objects as go
import plotly.io as pio

pio.templates["draft"] = go.layout.Template(
    layout_annotations=[
        dict(
            name="draft watermark",
            text="DRAFT",
            textangle=-30,
            opacity=0.1,
            font=dict(color="black", size=100),
            xref="paper",
            yref="paper",
            x=0.5,
            y=0.5,
            showarrow=False,
        )
    ]
)

pio.templates["nord_dark"] = go.layout.Template(
    layout=go.Layout(
        template="plotly_dark",  # base from dark mode
        plot_bgcolor="#17171c",
        paper_bgcolor="#17171c",
        font=dict(
            family="Courier New, monospace",
            size=11,
            color="#D8DEE9"
        ),
    )
)

custom_dark = pio.templates["plotly_dark"].to_plotly_json()
custom_dark["layout"]["font"]["family"] = "Courier New, monospace"
custom_dark["layout"]["font"]["size"] = 11

custom_dark["layout"]["xaxis"] = dict(
    linecolor="red",
    tickcolor="red",
    titlefont=dict(color="red"),
    tickfont=dict(color="red"),
)

custom_dark["layout"]["yaxis"] = dict(
    linecolor="green",
    tickcolor="green",
    titlefont=dict(color="green"),
    tickfont=dict(color="green"),
)

pio.templates["plotly_dark_mono"] = custom_dark

