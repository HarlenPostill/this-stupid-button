#!/usr/bin/env python3
"""Send clearly labelled synthetic MQTT data; verify its storage through Telegraf.

Requires the running cloud/telegraf.conf agent. Diagnostic points use
location=software_test, so the real-building Grafana dashboard excludes them.
No actual microphone measurement is claimed.
"""
import csv
import io
import json
from pathlib import Path
import socket
import ssl
import struct
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[1]


def environment():
    # Matches the single-quoted dotenv format produced by configure_cloud.py.
    return dict(line.split("=", 1) for line in (ROOT / "cloud/.env").read_text().splitlines()
                if line and not line.startswith("#"))


def text(value):
    data = value.encode()
    return struct.pack("!H", len(data)) + data


def packet(header, body):
    length = len(body)
    remaining = bytearray()
    while True:
        digit = length % 128
        length //= 128
        remaining.append(digit | (128 if length else 0))
        if not length:
            break
    return bytes([header]) + remaining + body


def receive(sock):
    def exact(size):
        data = bytearray()
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                raise ConnectionError("MQTT connection closed")
            data.extend(chunk)
        return bytes(data)
    header = exact(1)[0]
    length, multiplier = 0, 1
    for _ in range(4):
        digit = exact(1)[0]
        length += (digit & 127) * multiplier
        if not digit & 128:
            return header, exact(length)
        multiplier *= 128
    raise ValueError("Malformed MQTT remaining length")


def main():
    env = {key: value.strip("'") for key, value in environment().items()}
    run = uuid.uuid4().hex[:12]
    device = "software-validation-" + run
    fixture = json.loads((ROOT / "tests/telemetry-valid.json").read_text())
    fixture.update(group=env["LAB_GROUP_ID"], device=device, boot_id=run)
    topic = f'iot2026/{env["LAB_GROUP_ID"]}/noise/{device}/telemetry'
    with socket.create_connection(("broker.emqx.io", 1883), timeout=10) as sock:
        sock.settimeout(10)
        body = text("MQTT") + bytes([4, 0xc2]) + struct.pack("!H", 30)
        body += text("noise-validation-" + run) + text("emqx") + text("public")
        sock.sendall(packet(0x10, body))
        assert receive(sock) == (0x20, b"\x00\x00"), "Broker rejected MQTT CONNECT"
        for number, state, spl in [(1, "safe", 65), (2, "warning", 85), (3, "high", 100), (4, "sensor_fault", None)]:
            sample = dict(fixture, state=state, state_code=number-1, sequence=number, sensor_ok=spl is not None)
            if spl is None:
                del sample["estimated_spl_db"]
                del sample["dbfs"]
            else:
                sample.update(estimated_spl_db=float(spl), dbfs=float(spl-120))
            # QoS 1 diagnostic publisher verifies broker acknowledgement. ESP32
            # PubSubClient telemetry uses QoS 0, as documented in README.
            sock.sendall(packet(0x32, text(topic) + struct.pack("!H", number) + json.dumps(sample).encode()))
            assert receive(sock) == (0x40, struct.pack("!H", number)), "Missing PUBACK"
            time.sleep(0.2)
        sock.sendall(b"\xe0\x00")
    print("PASS: broker acknowledged 4 labelled synthetic MQTT messages")
    query = f'''from(bucket: {json.dumps(env["INFLUX_BUCKET"])})
      |> range(start: -5m)
      |> filter(fn: (r) => r._measurement == "noise_monitor" and r.device == {json.dumps(device)})
      |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")'''
    context = ssl.create_default_context(cafile="/etc/ssl/cert.pem") if Path("/etc/ssl/cert.pem").exists() else ssl.create_default_context()
    request = urllib.request.Request(env["INFLUX_URL"].rstrip("/") + "/api/v2/query?org=" + urllib.parse.quote(env["INFLUX_ORG"]), data=json.dumps({"query":query,"type":"flux"}).encode(), headers={"Authorization":"Token " + env["INFLUX_READ_TOKEN"],"Content-Type":"application/json","Accept":"application/csv"})
    for _ in range(20):
        with urllib.request.urlopen(request, context=context, timeout=15) as response:
            rows = list(csv.DictReader(io.StringIO(response.read().decode())))
        by_state = {row.get("state"): row for row in rows if row.get("device") == device}
        if {"safe", "warning", "high", "sensor_fault"}.issubset(by_state):
            assert by_state["high"]["estimated_spl_db"] == "100"
            assert by_state["sensor_fault"].get("estimated_spl_db", "") == ""
            assert by_state["sensor_fault"]["sensor_ok"] == "false"
            print("PASS: MQTT -> Telegraf -> InfluxDB stored all 4 test states and omitted fault sound values")
            print("Diagnostic device: " + device + " (software_test; excluded from building dashboard)")
            return
        time.sleep(1)
    raise SystemExit("No complete test series found. Check Telegraf logs and bucket write permission.")


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as error:
        raise SystemExit(f"InfluxDB returned HTTP {error.code}; check bucket read/write permission.")
