#!/usr/bin/env python3
"""Exercise both real Telegraf parsers, including missing measurements on faults."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "telegraf"
    config = (ROOT / "cloud/telegraf.conf").read_text()
    inputs = config.split("[[inputs.mqtt_consumer]]")[1:]
    assert len(inputs) == 2
    for prefix, measurement, block in zip(["telemetry", "cabinet"], ["noise_monitor", "cabinet_monitor"], inputs):
        parser = block.split("  [[inputs.mqtt_consumer.json_v2]]", 1)[1].split("[[outputs.influxdb_v2]]", 1)[0]
        parser = "  [[inputs.file.json_v2]]" + parser.replace("inputs.mqtt_consumer", "inputs.file")
        for kind in ["valid", "fault"]:
            fixture = ROOT / f"tests/{prefix}-{kind}.json"
            with tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "parser.conf"
                path.write_text('[agent]\n  omit_hostname = true\n[[inputs.file]]\n  files = ["' + str(fixture) + '"]\n  data_format = "json_v2"\n' + parser)
                result = subprocess.run([binary, "--config", str(path), "--test"], capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, result.stderr
            lines = [line for line in result.stdout.splitlines() if measurement + "," in line]
            assert len(lines) == 1, result.stdout + result.stderr
            line = lines[0]
            assert "device=software-validation" in line and "group=test-group" in line
            if prefix == "telemetry":
                assert "warning_db=80" in line and "limit_db=95" in line and 'clipped=false' in line
                if kind == "valid":
                    assert "estimated_spl_db=85" in line and "dbfs=-35" in line
                    assert "sensor_ok=true" in line and "state_code=1i" in line
                else:
                    assert "estimated_spl_db=" not in line and "dbfs=" not in line
                    assert "sensor_ok=false" in line and "state_code=3i" in line
            else:
                assert "trigger_db=90" in line and "door_threshold_cm=20" in line
                assert "press_count=1i" in line and "sustain_ms=3000i" in line
                if kind == "valid":
                    assert 'door_state="open"' in line and "distance_cm=30" in line
                    assert "noise_db=92" in line and "noise_fresh=true" in line
                else:
                    assert 'door_state="unknown"' in line and "door_state_code=2i" in line
                    assert "distance_cm=" not in line and "noise_db=" not in line
                    assert "sensor_ok=false" in line and "noise_fresh=false" in line
            print(f"PASS: actual Telegraf parser, {measurement} {kind} -> typed Influx line protocol")

if __name__ == "__main__":
    main()
