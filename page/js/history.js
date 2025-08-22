const time_buttons = Array.from(document.querySelectorAll(".button-container button"));
const contents = {
  btn1h: document.getElementById("content-1h"),
  btn4h: document.getElementById("content-4h"),
  btn1d: document.getElementById("content-1d"),
};

function showContent(buttonId) {
  // Hide all content divs
  Object.values(contents).forEach(div => div.style.display = "none");
  // Remove active class from all buttons
  time_buttons.forEach(btn => btn.classList.remove("active"));

  // Show the matching content
  contents[buttonId].style.display = "block";
  document.getElementById(buttonId).classList.add("active");
}

document.addEventListener("DOMContentLoaded", function() {
  // Add click listeners
  time_buttons.forEach(btn => {
    btn.addEventListener("click", () => {
      showContent(btn.id);
    });
  });

  // Show first by default
  showContent("btn1h");
});

document.addEventListener('keydown', (event) => {
  if (['h', 'l'].includes(event.key) && !event.ctrlKey && !event.metaKey && !event.altKey) {
    event.preventDefault();
    let currentIndex = time_buttons.findIndex(btn => btn.classList.contains('active'));
    if (event.key === 'h') {
      currentIndex = (currentIndex - 1 + time_buttons.length) % time_buttons.length;
    } else if (event.key === 'l') {
      currentIndex = (currentIndex + 1) % time_buttons.length;
    }
    // Pass the new button's id, not the index
    showContent(time_buttons[currentIndex].id);
  }
});

const buttons = document.querySelectorAll('[class*="sym_"]');
buttons.forEach(button => {
  // Extract symbol from button class (assumes format like "sym_AAPL")
  const symbol = button.className.match(/sym_(\w+)/)?.[1];

  if (!symbol)
    return;

  button.addEventListener('click', () => {
    const sameSymbolButtons = document.querySelectorAll(`.sym_${symbol}`);

    const isAnyActive =
      Array.from(sameSymbolButtons).some(btn => btn.classList.contains('active'));

    sameSymbolButtons.forEach(btn => {
      if (isAnyActive)
        btn.classList.remove('active');
      else
        btn.classList.add('active');
    });
  });
});

document.querySelectorAll('.symbol-entry').forEach(div => {
  const hoverDiv = div.querySelector('.symbol-details');

  div.addEventListener('mouseenter', () => {
    const rect = div.getBoundingClientRect();
    hoverDiv.style.display = 'block';

    const hoverRect = hoverDiv.getBoundingClientRect();
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;

    let left, top;

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
