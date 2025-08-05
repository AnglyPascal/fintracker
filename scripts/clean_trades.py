import pandas as pd
from datetime import timedelta
from pathlib import Path
from typing import List
import csv


def normalize_action(action):
    if isinstance(action, str):
        if "buy" in action.lower():
            return "BUY"
        elif "sell" in action.lower():
            return "SELL"
    return action


def process_file(filepath) -> pd.DataFrame:
    try:
        df: pd.DataFrame = pd.read_csv(filepath)

        df["Action"] = df["Action"].apply(normalize_action)

        df["Time"] = pd.to_datetime(df["Time"], errors="coerce")
        df = df.dropna(subset=["Time"])
        df["Time"] = df["Time"].dt.strftime("%Y-%m-%d %H:%M:%S")

        # Rename and select columns
        df.rename(
            columns={
                "No. of shares": "Qty",
                "Quantity": "Qty",
                "Price / share": "Price",
                "Total": "Total",
            },
            inplace=True,
        )

        df = df[df["Action"] != "Deposit"]  # type: ignore
        df = df[
            ["Time", "Ticker", "Action", "ID", "Qty", "Price", "Total"]
        ]  # type: ignore

        return df

    except Exception as e:
        print(f"Failed to process {filepath}: {e}")
        return pd.DataFrame()


def combine_group(group_rows):
    group_df = pd.DataFrame(group_rows)
    first_time = group_df["Time"].min()
    ticker = group_df["Ticker"].iloc[0]
    action = group_df["Action"].iloc[0]
    qty = group_df["Qty"].sum()
    total = group_df["Total"].sum()
    price = (group_df["Qty"] * group_df["Price"]).sum() / qty if qty != 0 else 0

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
            combined.append(combine_group(group))
            group = [row]

    if group:
        combined.append(combine_group(group))

    return pd.DataFrame(combined)


def get_modified_csv_files(input_dir: str) -> List[str]:
    input_path = Path(input_dir)
    history_path = input_path / "history.txt"
    modified_files = []

    history = {}
    if history_path.exists():
        with open(history_path, "r") as f:
            for line in f:
                name, mod_time = line.strip().split(",", 1)
                history[name] = float(mod_time)

    current = {}
    for file in input_path.glob("*.csv"):
        mtime = file.stat().st_mtime
        current[file.name] = mtime
        if file.name not in history or history[file.name] != mtime:
            modified_files.append(str(file))

    with open(history_path, "w") as f:
        for name, mtime in current.items():
            f.write(f"{name},{mtime}\n")

    return modified_files


def merge_trades_to_csv(csv_path: str, new_df: pd.DataFrame):
    required_columns = ["Time", "Ticker", "Action", "Qty", "Price", "Total"]

    try:
        existing_df = pd.read_csv(csv_path, parse_dates=["Time"])
    except FileNotFoundError:
        new_df["Remark"] = ""
        new_df["Rating"] = 0
        new_df["Time"] = pd.to_datetime(new_df["Time"])  # ensure datetime dtype
        new_df.to_csv(csv_path, index=False, quoting=csv.QUOTE_NONNUMERIC)
        return

    # Ensure both are datetime for merge compatibility
    new_df["Time"] = pd.to_datetime(new_df["Time"])
    existing_df["Time"] = pd.to_datetime(existing_df["Time"])

    check = all(
        col in existing_df.columns for col in required_columns + ["Remark", "Rating"]
    )
    if not check:
        print(existing_df.columns)
        raise ValueError("CSV is missing required columns")

    existing_core = existing_df[required_columns].drop_duplicates()
    new_core = new_df[required_columns]

    # Anti-join: keep only rows in new_core not in existing_core
    merged = new_core.merge(existing_core, how="left", indicator=True)
    unique_new_core = merged[merged["_merge"] == "left_only"].drop(columns=["_merge"])

    # Recover full new rows from new_df
    unique_new_df = new_df.merge(unique_new_core, on=required_columns, how="inner")
    unique_new_df["Remark"] = ""
    unique_new_df["Rating"] = 0

    combined_df = pd.concat([existing_df, unique_new_df], ignore_index=True)
    combined_df.sort_values(by="Time", inplace=True)

    combined_df.to_csv(csv_path, index=False)


def update_trades(input_dir, output_file):
    all_files = get_modified_csv_files(input_dir)

    if not all_files:
        return

    all_trades = pd.DataFrame()

    for file in all_files:
        df = process_file(file)
        all_trades = pd.concat([all_trades, df], ignore_index=True)

    all_trades.drop_duplicates(inplace=True)

    all_trades["Time"] = pd.to_datetime(all_trades["Time"])
    all_trades = all_trades.sort_values("Time")

    all_trades = combine_nearby_trades(all_trades)
    all_trades = all_trades[all_trades["Total"] >= 300]

    merge_trades_to_csv(output_file, all_trades)  # type: ignore


if __name__ == "__main__":
    update_trades("./private/trades_exported", "./private/trades.csv")
