from pathlib import Path


def sanitize(path):
    s = path.read_text(encoding="utf-8")
    return s.replace("{", "{{").replace("}", "}}")


def combine(root, var_name):
    html_path = Path(f"{root}/{var_name}.html")

    html = sanitize(html_path)
    html = html.replace("PLACEHOLDER", "{}")

    return html


# Generate header file
output = f"""
#pragma once 
#include <string_view>

inline constexpr std::string_view index_template = R"(
{combine("src/webpage", "index")}
)";
"""

output_path = Path("src/webpage/gen_html_template.h")
output_path.write_text(output, encoding="utf-8")
