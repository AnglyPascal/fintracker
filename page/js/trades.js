let tradesData = {};
let allTrades = [];
let uniqueTickers = [];
let sortAscending = false;
let selectedMaxRating = 0;

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
    const columns = Array.from({length: 8}, (_, col) =>
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

    let trades = allTrades.filter(trade => {
        if (!selectedTickers.includes(trade.ticker)) return false;
        if (fromDate && trade.date < fromDate) return false;
        if (toDate && trade.date > toDate) return false;
        return true;
    });

    trades = selectedMaxRating === 0
        ? trades
        : trades.filter(trade => trade.rating != 0 && trade.rating <= selectedMaxRating);

    trades = trades.sort((a, b) => {
        const [dateA, timeA] = a.date.split(" ");
        const [dateB, timeB] = b.date.split(" ");
        if (dateA !== dateB)
            return sortAscending ? dateA.localeCompare(dateB) : dateB.localeCompare(dateA);
        return timeA.localeCompare(timeB);
    });

    return trades;
}

function resizeAllTextareas() {
    document.querySelectorAll(".remark-input").forEach(textarea => {
        textarea.style.height = "auto";
        textarea.style.height = textarea.scrollHeight + "px";
    });
}

function renderTable() {
    tableBody.innerHTML = "";
    for (const trade of filterTrades()) {
        const row = document.createElement("tr");
        const cell = trade.action === "BUY" ? "buy-cell" : "sell-cell";
        row.className = trade.action === "BUY" ? "buy-row" : "sell-row";
        row.innerHTML = `
      <td>${trade.date.split(" ")[0]}</td>
      <td>${trade.ticker}</td>
      <td class="${cell}">${trade.action}</td>
      <td>${Number(trade.qty).toFixed(2)}</td>
      <td>${Number(trade.px).toFixed(2)}</td>
      <td>${Number(trade.total).toFixed(2)}</td>
      <td class="remark-cell"><textarea data-id="${trade.id}" 
           class="remark-input">${trade.remark || ''}</textarea></td>
      <td class="rating-cell" data-id="${trade.id}" data-rating="${trade.rating || 0}">
        ${[1, 2, 3, 4, 5].map(i => `
          <span class="rating-number ${i <= trade.rating ? 'selected' : ''}" data-value="${i}">${i}</span>
        `).join('')}
      </td>
    `;
        tableBody.appendChild(row);
    }

    resizeAllTextareas(); // Ensure initial fit
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
        const textarea = e.target;
        const id = textarea.dataset.id;
        const remark = textarea.value;

        textarea.style.height = "auto";
        textarea.style.height = textarea.scrollHeight + "px";

        fetch("/api/update-remark", {
            method: "PUT",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify({id, remark})
        });
    }
});

document.addEventListener("click", (e) => {
    if (e.target.classList.contains("rating-number")) {
        const clickedValue = parseInt(e.target.dataset.value);
        const cell = e.target.closest(".rating-cell");
        const id = cell.dataset.id;
        const currentRating = parseInt(cell.dataset.rating);

        // Determine if we should toggle off
        const newRating = (clickedValue === currentRating) ? 0 : clickedValue;

        // Update cell attribute
        cell.dataset.rating = newRating;

        // Update visual state
        cell.querySelectorAll(".rating-number").forEach(span => {
            const value = parseInt(span.dataset.value);
            span.classList.toggle("selected", value <= newRating);
        });

        // Update backend
        fetch("/api/update-rating", {
            method: "PUT",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify({id, rating: newRating})
        });
    }
});

const ratingButtons = document.querySelectorAll("#rating-filter button");

ratingButtons.forEach(btn => {
    btn.addEventListener("click", () => {
        // Remove .active from all buttons
        ratingButtons.forEach(b => b.classList.remove("active"));
        // Mark this one active
        btn.classList.add("active");

        // Update the selected rating
        selectedMaxRating = parseInt(btn.dataset.rating, 10);
        renderTable();
    });
});
