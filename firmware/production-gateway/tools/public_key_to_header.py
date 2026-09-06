import argparse
import hashlib
import shutil
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Converte una chiave pubblica PEM nel formato OTA Arduino")
    parser.add_argument("pem", type=Path)
    parser.add_argument("header", type=Path)
    args = parser.parse_args()

    openssl = shutil.which("openssl")
    if not openssl:
        raise RuntimeError("OpenSSL non trovato: impossibile validare la chiave pubblica")
    public_der = subprocess.run(
        [openssl, "pkey", "-pubin", "-in", str(args.pem), "-outform", "DER"],
        check=True,
        capture_output=True,
    ).stdout
    fingerprint = hashlib.sha256(public_der).hexdigest()
    data = args.pem.read_bytes() + b"\0"
    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "// Chiave pubblica ECDSA-P256. La chiave privata non deve entrare nel repository.",
        f"// SHA-256 (SubjectPublicKeyInfo DER): {fingerprint}",
        "const uint8_t PUBLIC_KEY[] PROGMEM = {",
    ]
    for offset in range(0, len(data), 16):
        lines.append("  " + ", ".join(f"0x{value:02x}" for value in data[offset : offset + 16]) + ",")
    lines.extend(["};", f"const size_t PUBLIC_KEY_LEN = {len(data)};", ""])
    args.header.write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
