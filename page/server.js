import express from 'express';
import fs from 'fs/promises';
import {parse} from 'csv-parse/sync';
import {stringify} from 'csv-stringify/sync';
import livereload from 'livereload';
import connectLivereload from 'connect-livereload';

const app = express();
const PORT = 8000;
const CSV_PATH = '../private/trades.csv';

const liveReloadServer = livereload.createServer();
liveReloadServer.watch([
    "public",
    "css",
    "js"
]);

liveReloadServer.server.once("connection", () => {
    setTimeout(() => {
        liveReloadServer.refresh("/");
    }, 100);
});

app.use(connectLivereload());
app.use(express.json());
app.use(express.static('public'));
app.use("/css", express.static("css"));
app.use("/src", express.static("src"));
app.use("/js", express.static("js"));
app.use("/public", express.static("public"));

let trades = [];
let tradesByTicker = {};

async function loadTrades() {
    const csvText = await fs.readFile(CSV_PATH, 'utf8');
    const records = parse(csvText, {
        columns: true,
        skip_empty_lines: true
    });

    trades = records.map((r, idx) => ({
        id: idx.toString(),
        date: r.Time,
        ticker: r.Ticker,
        action: r.Action,
        qty: r.Qty,
        px: r.Price,
        total: r.Total,
        remark: r.Remark || '',
        rating: r.Rating ? parseInt(r.Rating, 10) : 0
    }));

    tradesByTicker = {};
    for (const trade of trades) {
        if (!tradesByTicker[trade.ticker]) {
            tradesByTicker[trade.ticker] = [];
        }
        tradesByTicker[trade.ticker].push(trade);
    }
}

async function saveTrades() {
    try {
        const records = trades.map(t => ({
            Time: t.date,
            Ticker: t.ticker,
            Action: t.action,
            Qty: t.qty,
            Price: t.px,
            Total: t.total,
            Remark: t.remark || '',
            Rating: Number.isInteger(t.rating) ? t.rating : 0
        }));
        const csv = stringify(records, {
            header: true,
            columns: [
                'Time',
                'Ticker',
                'Action',
                'Qty',
                'Price',
                'Total',
                {key: 'Remark', quoted: true},
                'Rating'
            ]
        });
        await fs.writeFile(CSV_PATH, csv, 'utf8');
    } catch (err) {
    }
}

app.get('/api/trades', async (_, res) => {
    await loadTrades();
    res.json(tradesByTicker);
});

app.put('/api/update-remark', async (req, res) => {
    const {id, remark} = req.body;
    const trade = trades.find(t => t.id === id);
    if (!trade) return res.status(404).json({error: 'Trade not found'});

    trade.remark = remark;
    await saveTrades();
    res.json({success: true});
});

app.put('/api/update-rating', async (req, res) => {
    const {id, rating} = req.body;
    const trade = trades.find(t => t.id === id);
    if (!trade) return res.status(404).json({error: 'Trade not found'});

    trade.rating = rating;
    await saveTrades();
    res.json({success: true});
});

app.listen(PORT, () => {
    console.log(`Server running at http://localhost:${PORT}`);
});
