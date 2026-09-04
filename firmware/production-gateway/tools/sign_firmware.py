Import("env")

import os
import shutil
import subprocess
from pathlib import Path


def sign_firmware(source, target, env):
    firmware = Path(str(target[0]))
    configured_key = os.environ.get("PESALINK_SIGNING_KEY") or os.environ.get(
        "LAVEGGIO_SIGNING_KEY"
    )
    pesalink_key = Path.home() / ".casklogic" / "pesalink-signing" / "private_key.pem"
    legacy_key = Path.home() / ".casklogic" / "laveggio-signing" / "private_key.pem"
    key_path = Path(configured_key) if configured_key else (
        pesalink_key if pesalink_key.is_file() or not legacy_key.is_file() else legacy_key
    )
    if not key_path.is_file():
        print(
            "CaskLogic PesaLink: firmware.bin creato, ma firmware.signed.bin non generato; "
            f"chiave assente: {key_path}"
        )
        return

    openssl = shutil.which("openssl")
    if not openssl:
        raise RuntimeError("OpenSSL non trovato: impossibile firmare il firmware OTA")
    signed = firmware.with_name("firmware.signed.bin")
    signature = firmware.with_name("firmware.signature.der")
    subprocess.run(
        [
            openssl,
            "dgst",
            "-sha256",
            "-sign",
            str(key_path),
            "-out",
            str(signature),
            str(firmware),
        ],
        check=True,
    )
    signature_bytes = signature.read_bytes()
    if len(signature_bytes) > 512:
        raise RuntimeError("Firma OTA oltre il limite di 512 byte")
    signed.write_bytes(firmware.read_bytes() + signature_bytes + bytes(512 - len(signature_bytes)))
    signature.unlink(missing_ok=True)
    print(f"CaskLogic PesaLink: firmware OTA firmato: {signed}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign_firmware)
