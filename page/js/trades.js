let tradesData = {};
let allTrades = [];
let uniqueTickers = [];
let sortAscending = false;

const tickerFiltersContainer = document.getElementById("ticker-filters");
const fromDateInput = document.getElementById("from-date");
const toDateInput = document.getElementById("to-date");
const toggleBtn = document.getElementById("toggle-btn");
const tableBody = document.querySelector("#trades-table tbody");

fetch("/api/trades")
  .then(res => res.json())
  .then(data => {
    tradesData = data;
    allTrades = Object.values(tradesData).flat();
    uniqueTickers = Object.keys(tradesData);
    renderTickerCheckboxes();
    renderTable();
  });

document.getElementById("date-header").addEventListener("click", () => {
  sortAscending = !sortAscending;
  renderTable();
});

function renderTickerCheckboxes() {
  tickerFiltersContainer.innerHTML = "";
  const tickersPerCol = Math.ceil(uniqueTickers.length / 8);
  const sorted = [...uniqueTickers].sort();
  const columns = Array.from({ length: 8 }, (_, col) =>
    sorted.slice(col * tickersPerCol, (col + 1) * tickersPerCol)
  );

  for (let row = 0; row < tickersPerCol; ++row) {
    const rowDiv = document.createElement("div");
    rowDiv.classList.add("checkbox-row");
    for (let col = 0; col < columns.length; ++col) {
      const ticker = columns[col][row];
      if (!ticker) continue;
      const label = document.createElement("label");
      label.classList.add("custom-checkbox");
      label.innerHTML = `
        <input type="checkbox" value="${ticker}" checked>
        <span class="checkmark"></span>
        ${ticker}
      `;
      rowDiv.appendChild(label);
    }
    tickerFiltersContainer.appendChild(rowDiv);
  }
}

function getSelectedTickers() {
  return Array.from(tickerFiltersContainer.querySelectorAll("input:checked")).map(cb => cb.value);
}

function filterTrades() {
  const selectedTickers = getSelectedTickers();
  const fromDate = fromDateInput.value;
  const toDate = toDateInput.value;
  return allTrades.filter(trade => {
    if (!selectedTickers.includes(trade.ticker)) return false;
    if (fromDate && trade.date < fromDate) return false;
    if (toDate && trade.date > toDate) return false;
    return true;
  });
}

function renderTable() {
  const filtered = filterTrades().sort((a, b) => {
    const [dateA, timeA] = a.date.split(" ");
    const [dateB, timeB] = b.date.split(" ");
    if (dateA !== dateB)
      return sortAscending ? dateA.localeCompare(dateB) : dateB.localeCompare(dateA);
    return timeA.localeCompare(timeB);
  });

  tableBody.innerHTML = "";
  for (const trade of filtered) {
    const row = document.createElement("tr");
    row.className = trade.action === "BUY" ? "buy-row" : "sell-row";
    row.innerHTML = `
      <td>${trade.date.split(" ")[0]}</td>
      <td>${trade.ticker}</td>
      <td>${trade.action}</td>
      <td>${Number(trade.qty).toFixed(2)}</td>
      <td>${Number(trade.px).toFixed(2)}</td>
      <td>${Number(trade.total).toFixed(2)}</td>
      <td><input type="text" value="${trade.remark || ''}" data-id="${trade.id}" class="remark-input"></td>
    `;
    tableBody.appendChild(row);
  }
}

function toggleAllTickers() {
  const checkboxes = tickerFiltersContainer.querySelectorAll("input[type='checkbox']");
  const allChecked = Array.from(checkboxes).every(cb => cb.checked);
  checkboxes.forEach(cb => cb.checked = !allChecked);
  renderTable();
}

tickerFiltersContainer.addEventListener("change", renderTable);
fromDateInput.addEventListener("input", renderTable);
toDateInput.addEventListener("input", renderTable);
toggleBtn.addEventListener("click", toggleAllTickers);

document.addEventListener("input", (e) => {
  if (e.target.classList.contains("remark-input")) {
    const id = e.target.dataset.id;
    const remark = e.target.value;
    fetch("/api/update-remark", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id, remark })
    });
  }
});
