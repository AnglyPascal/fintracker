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

