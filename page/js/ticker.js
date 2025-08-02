async function loadContent(type) {
  const html_fname = `${ticker}_${type}.html`;
  const plot_fname = `${ticker}_${type}_plot.html`;
  try {
    const html_res = await fetch(`./${html_fname}`);
    const plot_res = await fetch(`./${plot_fname}`);
    if (!html_res.ok || !plot_res.ok)
      throw new Error(`HTTP ${html_res.status}: ${html_res.statusText}`);
    const html = await html_res.text();
    const plot = await plot_res.text();
    
    if (html.trim() === '' || plot.trim() === '') {
      throw new Error('Loaded content is empty');
    }
    
    document.getElementById('content').innerHTML = html;
    
    // Parse the Plotly HTML to extract the plot div and scripts
    const parser = new DOMParser();
    const plotDoc = parser.parseFromString(plot, 'text/html');
    
    // Find the plot div (usually has an id like 'plotly-div' or similar)
    const plotDiv = plotDoc.querySelector('div[id*="plotly"], div.plotly-graph-div, body > div');
    const scripts = plotDoc.querySelectorAll('script');
    
    if (plotDiv) {
      document.getElementById('plotly').innerHTML = plotDiv.outerHTML;
      
      // Execute the scripts
      scripts.forEach(script => {
        if (script.src) {
          // External script
          const newScript = document.createElement('script');
          newScript.src = script.src;
          document.head.appendChild(newScript);
        } else if (script.textContent.trim()) {
          // Inline script
          const newScript = document.createElement('script');
          newScript.textContent = script.textContent;
          document.body.appendChild(newScript);
        }
      });
    }
    
    // Update button states
    document.querySelectorAll('button').forEach(btn => btn.classList.remove('active'));
    const targetButton = document.getElementById('btn' + type);
    if (targetButton) {
      targetButton.classList.add('active');
    } else {
      console.error(`Button with ID 'btn${type}' not found`);
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

// Load default content on page load
window.addEventListener('DOMContentLoaded', () => {
  loadContent('1h');
});
