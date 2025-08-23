async function loadContent(type) {
  const html_fname = `${ticker}_${type}.html`;
  try {
    const html_res = await fetch(`./${html_fname}`);
    if (!html_res.ok) {
      throw new Error(`HTTP ${html_res.status}: ${html_res.statusText}`);
    }

    const html = await html_res.text();
    if (html.trim() === '') {
      throw new Error('Loaded content is empty');
    }

    document.getElementById('content').innerHTML = html;

    // Update button states
    document.querySelectorAll('button').forEach(btn => btn.classList.remove('active'));
    const targetButton = document.getElementById('btn' + type);
    if (targetButton) {
      targetButton.classList.add('active');
    } else {
      console.warn(`Button with ID 'btn${type}' not found`);
    }
  } catch (error) {
    console.error(`Error loading content:`, error);
    document.getElementById('content').innerHTML = `
      <div style="color: red; padding: 20px; border: 1px solid red;">
        <h3>Error loading content</h3>
        <p><strong>Error:</strong> ${error.message}</p>
      </div>
    `;
  }
}

document.querySelectorAll('.thing-abbr').forEach(div => {
  const hoverDiv = div.querySelector('.thing-desc');

  div.addEventListener('mouseenter', () => {
    const rect = div.getBoundingClientRect();
    hoverDiv.style.display = 'block';

    const hoverRect = hoverDiv.getBoundingClientRect();
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;

    let left, top;

    console.log(hoverDiv);

    if (rect.right + hoverRect.width <= viewportWidth)
      left = rect.right; // place right
    else if (rect.left - hoverRect.width >= 0)
      left = rect.left - hoverRect.width; // place left
    else
      left = viewportWidth - hoverRect.width; // place aligned 

    if (rect.bottom + hoverRect.height <= viewportHeight)
      top = rect.bottom; // place bottom
    else if (rect.top - hoverRect.height >= 0)
      top = rect.top - hoverRect.height; // place top
    else
      top = rect.top; // place aligned

    hoverDiv.style.left = left + 'px';
    hoverDiv.style.top = top + 'px';
  });

  div.addEventListener('mouseleave', () => {
    hoverDiv.style.display = 'none';
  });
});

// Load default content on page load
window.addEventListener('DOMContentLoaded', () => {
  loadContent('1h');
});
