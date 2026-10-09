#!/usr/bin/env python3
"""Copy lab settings into an ignored Compose environment without printing secrets."""
import ast
import json
import os
from pathlib import Path
import re
import secrets

ROOT = Path(__file__).resolve().parents[1]


def read_lab_settings():
    text = (ROOT / "include/secrets.h").read_text()
    return {
        key: ast.literal_eval(literal)
        for key, literal in re.findall(r'^\s*#define\s+(LAB_\w+)\s+("(?:[^"\\]|\\.)*")', text, re.M)
    }


def main():
    mapping = {
        "LAB_GROUP_ID": "LAB_GROUP_ID", "INFLUX_URL": "LAB_INFLUXDB_URL",
        "INFLUX_ORG": "LAB_INFLUXDB_ORG", "INFLUX_BUCKET": "LAB_INFLUXDB_BUCKET",
        "INFLUX_TOKEN": "LAB_INFLUXDB_TOKEN", "INFLUX_READ_TOKEN": "LAB_INFLUXDB_TOKEN",
    }
    try:
        lab = read_lab_settings()
    except FileNotFoundError:
        raise SystemExit("Missing include/secrets.h. Use cloud/.env.example to configure manually.")
    missing = [key for key in mapping.values() if not lab.get(key) or "YOUR_" in lab[key]]
    if missing:
        raise SystemExit("Complete these lab settings first: " + ", ".join(sorted(set(missing))))
    values = {key: lab[value] for key, value in mapping.items()}
    values["GRAFANA_ADMIN_PASSWORD"] = secrets.token_urlsafe(24)
    # Single quotes prevent Compose interpolation of '$' inside credential values.
    if any("'" in value or "\n" in value or "\r" in value for value in values.values()):
        raise SystemExit("A setting contains unsupported dotenv characters; configure .env manually.")
    try:
        fd = os.open(ROOT / "cloud/.env", os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise SystemExit("cloud/.env already exists; kept existing credentials and password.")
    with os.fdopen(fd, "w") as out:
        for key, value in values.items():
            out.write(f"{key}='{value}'\n")
    print("Created ignored cloud/.env from existing lab settings (mode 0600).")
    print("Confirm INFLUX_READ_TOKEN can read the bucket. Grafana admin password is in this file.")


if __name__ == "__main__":
    main()
