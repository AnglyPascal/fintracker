import sys
from pathlib import Path


def sanitize(path):
    s = path.read_text(encoding="utf-8")
    return s.replace("{", "{{").replace("}", "}}")


def combine(var_name):
    css_path = Path(f"data/{var_name}.css")
    js_path = Path(f"data/{var_name}.js")
    html_path = Path(f"data/{var_name}.html")

    # Read and transform the content
    css = sanitize(css_path)
    js = sanitize(js_path)
    html = sanitize(html_path)

    for find, replace in [("CSS", css), ("JS", js), ("PLACEHOLDER", "{}")]:
        html = html.replace(find, replace)

    return html


# Generate header file
output = f"""
#pragma once 
#include <string_view>

inline constexpr std::string_view index_template = R"(
{combine("index")}
)";

inline constexpr std::string_view trades_template = R"(
{combine("trades")}
)";
"""

output_path = Path("include/_html_template.h")
output_path.write_text(output, encoding="utf-8")
