document.addEventListener("DOMContentLoaded", function () {
  const buttons = document.querySelectorAll(".button-container button");
  const contents = {
    btn1h: document.getElementById("content-1h"),
    btn4h: document.getElementById("content-4h"),
    btn1d: document.getElementById("content-1d"),
  };

  function showContent(buttonId) {
    // Hide all content
    Object.values(contents).forEach(div => div.style.display = "none");

    // Remove active class from all buttons
    buttons.forEach(btn => btn.classList.remove("active"));

    // Show selected content and mark button active
    contents[buttonId].style.display = "block";
    document.getElementById(buttonId).classList.add("active");
  }

  // Add click listeners
  buttons.forEach(btn => {
    btn.addEventListener("click", () => {
      showContent(btn.id);
    });
  });

  // Default state
  showContent("btn1h");
});

// Get all buttons that match the pattern sym_*
const buttons = document.querySelectorAll('[class*="sym_"]');

buttons.forEach(button => {
    // Extract symbol from button class (assumes format like "sym_AAPL")
    const symbol = button.className.match(/sym_(\w+)/)?.[1];
    
    if (!symbol) return; // Skip if no symbol found
    
    button.addEventListener('click', () => {
        // Get all buttons with the same sym_* class
        const sameSymbolButtons = document.querySelectorAll(`.sym_${symbol}`);
        
        // Check if any button in this group is currently active
        const isAnyActive = 
          Array.from(sameSymbolButtons).some(btn => btn.classList.contains('active'));
        
        // Toggle: if any are active, remove active from all; otherwise add active to all
        sameSymbolButtons.forEach(btn => {
            if (isAnyActive) {
                btn.classList.remove('active');
            } else {
                btn.classList.add('active');
            }
        });
    });
});

document.querySelectorAll('.symbol-entry').forEach(div => {
    const hoverDiv = div.querySelector('.symbol-details');
    
    div.addEventListener('mouseenter', (e) => {
        const rect = div.getBoundingClientRect();
        hoverDiv.style.display = 'block';
        hoverDiv.style.left = rect.left + 'px';
        hoverDiv.style.top = (rect.bottom + 5) + 'px'; // 5px below
    });
    
    div.addEventListener('mouseleave', () => {
        hoverDiv.style.display = 'none';
    });
});
