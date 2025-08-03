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

function toggleStats() {
  const spans = document.querySelectorAll('.stats-details');
  const button = document.querySelector('.stats-details-button');
  const currentlyVisible = spans[0].style.display === 'inline';

  spans.forEach(span => {
    span.style.display = currentlyVisible ? 'none' : 'inline';
  });

  button.textContent = currentlyVisible ? 'Show Details' : 'Hide Details';
  button.classList.toggle('active', !currentlyVisible);
}
