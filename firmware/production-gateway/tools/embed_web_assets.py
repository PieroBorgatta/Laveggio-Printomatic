Import("env")

from pathlib import Path


PROJECT_DIR = Path(env["PROJECT_DIR"])
DATA_DIR = PROJECT_DIR / "data"
OUTPUT = PROJECT_DIR / "include" / "WebAssets.h"


def raw_string(name: str, path: Path) -> str:
    content = path.read_text(encoding="utf-8")
    return (
        f"inline constexpr char {name}[] PROGMEM = R\"PESALINK_WEB("
        f"{content})PESALINK_WEB\";\n"
    )


def byte_array(name: str, path: Path) -> str:
    data = path.read_bytes()
    rows = []
    for offset in range(0, len(data), 20):
        rows.append(",".join(str(value) for value in data[offset : offset + 20]))
    body = ",\n".join(rows)
    return (
        f"inline constexpr uint8_t {name}[] PROGMEM = {{\n{body}\n}};\n"
        f"inline constexpr size_t {name}_LEN = sizeof({name});\n"
    )


generated = """// Generated from data/. Do not edit manually.\n#pragma once\n\n#include <Arduino.h>\n\n"""
generated += raw_string("WEB_INDEX_HTML", DATA_DIR / "index.html")
generated += raw_string("WEB_APP_CSS", DATA_DIR / "app.css")
generated += raw_string("WEB_APP_JS", DATA_DIR / "app.js")
generated += byte_array("WEB_CASKLOGIC_MARK", DATA_DIR / "casklogicmark.png")

if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != generated:
    OUTPUT.write_text(generated, encoding="utf-8", newline="\n")
