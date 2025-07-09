# 
- V:
    Exit Hints: stop very close, ema9 flattening near ema21
    still shows Watch
    - when in a postiion, and something alarming happens, change the signal to caution


- X portfolio now decides at the start which tickers to keep based on 1d and 4h indicators
    - it keeps the 1d decision in hand
    - at 4h mark, it makes that decision again

- X portfolio should also keep track of the tickers it's tracking, just in case
    - keep another thread running that monitors changes to tickers.csv

- have a separate thread keeping track of the market open hours
    - communicate with main thread using shared variable in portfolio called
        "MarketHours" or smth


