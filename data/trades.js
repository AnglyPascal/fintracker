const allTrades = Object.values(tradesData).flat();
const uniqueTickers = Object.keys(tradesData);

const tickerFiltersContainer = document.getElementById("ticker-filters");
const fromDateInput = document.getElementById("from-date");
const toDateInput = document.getElementById("to-date");
const toggleBtn = document.getElementById("toggle-btn");
const tableBody = document.querySelector("#trades-table tbody");

let sortAscending = false

document.getElementById("date-header").addEventListener("click", () => {
  sortAscending = !sortAscending;
  renderTable();
});

function renderTickerCheckboxes() {
  // Clear existing content
  tickerFiltersContainer.innerHTML = "";

  // Sort tickers column-wise for up to 8 columns
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

function updateSortIndicator() {
  const header = document.getElementById("date-header");
  header.textContent = sortAscending ? "Date ↑" : "Date ↓";
}

function renderTable() {
  const filtered = filterTrades().sort((a, b) => {
    const [dateA, timeA] = a.date.split(" ");
    const [dateB, timeB] = b.date.split(" ");

    if (dateA !== dateB) {
      return sortAscending
        ? dateA.localeCompare(dateB)
        : dateB.localeCompare(dateA);
    }

    // Same date — always sort by time ascending
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
      <td>${trade.qty}</td>
      <td>${trade.px.toFixed(2)}</td>
      <td>${trade.total.toFixed(2)}</td>
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

// Initialize
renderTickerCheckboxes();
renderTable();

tickerFiltersContainer.addEventListener("change", renderTable);
fromDateInput.addEventListener("input", renderTable);
toDateInput.addEventListener("input", renderTable);
toggleBtn.addEventListener("click", toggleAllTickers);

