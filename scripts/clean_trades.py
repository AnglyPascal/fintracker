import pandas as pd
from datetime import timedelta
from pathlib import Path


# Normalize "Market Buy", "Limit Sell", etc. to "Buy"/"Sell"
def normalize_action(action):
    if isinstance(action, str):
        if "buy" in action.lower():
            return "BUY"
        elif "sell" in action.lower():
            return "SELL"
    return action


# Extract and clean trades from a single file
def process_file(filepath):
    try:
        df = pd.read_csv(filepath)

        # Normalize Action
        df["Action"] = df["Action"].apply(normalize_action)

        # Robust datetime parsing and remove nanoseconds
        df["Time"] = pd.to_datetime(df["Time"], errors="coerce")
        df = df.dropna(subset=["Time"])
        df["Time"] = df["Time"].dt.strftime("%Y-%m-%d %H:%M:%S")

        # Rename and select columns
        df.rename(
            columns={
                "No. of shares": "Qty",
                "Price / share": "Price",
                "Total": "Total",
            },
            inplace=True,
        )

        df = df[df["Action"] != "Deposit"]
        df = df[["Time", "Ticker", "Action", "ID", "Qty", "Price", "Total"]]

        return df

    except Exception as e:
        print(f"Failed to process {filepath}: {e}")
        return pd.DataFrame()


def _combine_group(group_rows):
    group_df = pd.DataFrame(group_rows)
    first_time = group_df["Time"].min()
    ticker = group_df["Ticker"].iloc[0]
    action = group_df["Action"].iloc[0]
    qty = group_df["Qty"].sum()
    total = group_df["Total"].sum()
    price = (group_df["Qty"] * group_df["Price"]).sum() / qty if qty != 0 else 0

    # if ticker == "ECL":  # FIXME: why are they separate groups?
    #     print(group_rows)

    return {
        "Time": first_time,
        "Ticker": ticker,
        "Action": action,
        "Qty": qty,
        "Price": round(price, 4),
        "Total": round(total, 2),
    }


def combine_nearby_trades(df, max_gap_minutes=5):
    combined = []
    group = []

    df = df.sort_values("Time").reset_index(drop=True)
    max_gap = timedelta(minutes=max_gap_minutes)

    for _, row in df.iterrows():
        current_time = row["Time"]

        if not group:
            group.append(row)
            continue

        last = group[-1]

        same_ticker = row["Ticker"] == last["Ticker"]
        same_action = row["Action"] == last["Action"]
        time_diff = current_time - last["Time"]

        if same_ticker and same_action and time_diff <= max_gap:
            group.append(row)
        else:
            combined.append(_combine_group(group))
            group = [row]

    if group:
        combined.append(_combine_group(group))

    return pd.DataFrame(combined)


# Main logic
def normalize_all_trades(input_dir, output_file):
    input_path = Path(input_dir)
    all_files = list(input_path.glob("*.csv"))

    if not all_files:
        print(f"No CSV files found in {input_dir}")
        return

    all_trades = pd.DataFrame()

    for file in all_files:
        df = process_file(file)
        all_trades = pd.concat([all_trades, df], ignore_index=True)

    # Remove exact duplicates (same row values)
    all_trades.drop_duplicates(inplace=True)

    all_trades["Time"] = pd.to_datetime(all_trades["Time"])  # Convert to datetime
    all_trades = all_trades.sort_values("Time")  # Sort ascending by default

    all_trades = combine_nearby_trades(all_trades)
    all_trades = all_trades[all_trades["Total"] >= 300]

    # all_trades = all_trades[["Time", "Ticker", "Action", "Qty", "Price", "Total"]]

    # Save to output
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    all_trades.to_csv(output_path, index=False)
    # print(f"Normalized trades saved to {output_path}")


# Entry point
if __name__ == "__main__":
    normalize_all_trades("./private/trades_exported", "./private/trades.csv")
