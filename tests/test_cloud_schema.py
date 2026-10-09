#!/usr/bin/env python3
"""Exercise the actual Telegraf parser, including omitted levels on a sensor fault."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "telegraf"
    config = (ROOT / "cloud/telegraf.conf").read_text()
    parser = config.split("  [[inputs.mqtt_consumer.json_v2]]", 1)[1].split("[[outputs.influxdb_v2]]", 1)[0]
    parser = "  [[inputs.file.json_v2]]" + parser.replace("inputs.mqtt_consumer", "inputs.file")
    for kind in ["valid", "fault"]:
        fixture = ROOT / f"tests/telemetry-{kind}.json"
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "parser.conf"
            path.write_text('[agent]\n  omit_hostname = true\n[[inputs.file]]\n  files = ["' + str(fixture) + '"]\n  data_format = "json_v2"\n' + parser)
            result = subprocess.run([binary, "--config", str(path), "--test"], capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise SystemExit(result.stderr)
        lines = [line for line in result.stdout.splitlines() if "noise_monitor," in line]
        assert len(lines) == 1, result.stdout + result.stderr
        line = lines[0]
        assert "device=software-validation" in line and "group=test-group" in line
        assert "warning_db=80" in line and "limit_db=95" in line
        assert 'clipped=false' in line
        if kind == "valid":
            assert "estimated_spl_db=85" in line and "dbfs=-35" in line
            assert "sensor_ok=true" in line and "state_code=1i" in line
        else:
            assert "estimated_spl_db=" not in line and "dbfs=" not in line
            assert "sensor_ok=false" in line and "state_code=3i" in line
        print(f"PASS: actual Telegraf parser, {kind} telemetry -> typed Influx line protocol")


if __name__ == "__main__":
    main()
