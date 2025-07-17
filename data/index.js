function toggleColumn(colIndex) {
  const th = document.querySelector(`th:nth-child(${colIndex})`);
  const tds = document.querySelectorAll(`td:nth-child(${colIndex})`);

  const isCollapsed = th.classList.toggle("collapsed");
  tds.forEach(td => {
    if (isCollapsed) td.classList.add("collapsed");
    else td.classList.remove("collapsed");
  });
}

function toggleSignalDetails(row, detailRowId) {
  const detailRow = document.getElementById(detailRowId);
  if (!detailRow) return;

  const isVisible = detailRow.style.display !== "none";
  detailRow.style.display = isVisible ? "none" : "table-row";
}

function toggleSignal(button) {
  const type = button.dataset.type;
  const rows = document.querySelectorAll(`tr.signal-${type}`);

  const isActive = button.classList.toggle('active');
  rows.forEach(row => {
    row.style.display = isActive ? '' : 'none';

    // Hide detail row if it's visible
    const detailRow = 
      document.getElementById(`${row.id.replace('row-', '')}-details`);
    if (detailRow && !isActive) detailRow.style.display = 'none';
  });
}

function showAllSignals() {
  const signalRows = document.querySelectorAll('tr[id^="row-"]');
  signalRows.forEach(row => row.style.display = '');

  const detailRows = document.querySelectorAll('.signal-details-row');
  detailRows.forEach(row => row.style.display = 'none');

  const buttons = document.querySelectorAll('#signal-filters .filter-btn');
  buttons.forEach(btn => btn.classList.add('active'));
}
