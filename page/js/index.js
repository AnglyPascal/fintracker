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
  const spans = document.querySelectorAll('.stats-details, .signal-score-details');

  const button = document.querySelector('.stats-details-button');
  const currentlyVisible = spans[0].style.display === 'inline';

  spans.forEach(span => {
    span.style.display = currentlyVisible ? 'none' : 'inline';
  });

  button.textContent = currentlyVisible ? 'Show Details' : 'Hide Details';
  button.classList.toggle('active', !currentlyVisible);
}

document.querySelectorAll('.thing-abbr').forEach(div => {
  const hoverDiv = div.querySelector('.thing-desc');
  if (hoverDiv == null)
    return;

  div.addEventListener('mouseenter', () => {
    hoverDiv.style.display = 'block';

    requestAnimationFrame(() => {
      const rect = div.getBoundingClientRect();
      const hoverRect = hoverDiv.getBoundingClientRect();
      const viewportWidth = window.innerWidth;
      const viewportHeight = window.innerHeight;

      let left, top;

      if (rect.right + hoverRect.width <= viewportWidth)
        left = rect.right;
      else if (rect.left - hoverRect.width >= 0)
        left = rect.left - hoverRect.width;
      else
        left = Math.max(0, viewportWidth - hoverRect.width);

      if (rect.bottom + hoverRect.height <= viewportHeight)
        top = rect.bottom;
      else if (rect.top - hoverRect.height >= 0)
        top = rect.top - hoverRect.height;
      else
        top = Math.max(0, viewportHeight - hoverRect.height);

      hoverDiv.style.position = 'fixed';
      hoverDiv.style.left = left + 'px';
      hoverDiv.style.top = top + 'px';
      hoverDiv.style.zIndex = 9999;
    });
  });

  div.addEventListener('mouseleave', () => {
    hoverDiv.style.display = 'none';
  });
});
