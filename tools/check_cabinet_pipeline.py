#!/usr/bin/env python3
"""Labelled, isolated cabinet MQTT -> Telegraf -> InfluxDB smoke test.

Does not publish to the microphone's topic or issue servo commands. Grafana's
real cabinet dashboard excludes location=software_test. This verifies cloud
transport and parsing, not physical distance sensing or button contact.
"""
import csv
import io
import json
from pathlib import Path
import socket
import ssl
import struct
import time
import urllib.parse
import urllib.request
import uuid
from check_pipeline import ROOT, environment, text, packet, receive


def main():
    env = {key: value.strip("'") for key, value in environment().items()}
    run = uuid.uuid4().hex[:12]
    device = "software-cabinet-" + run
    base = json.loads((ROOT / 'tests/cabinet-valid.json').read_text())
    base.update(group=env['LAB_GROUP_ID'], device=device, boot_id=run)
    topic = f"iot2026/{env['LAB_GROUP_ID']}/cabinet/{device}/telemetry"
    with socket.create_connection(('broker.emqx.io',1883),timeout=10) as sock:
        sock.settimeout(10)
        body = text('MQTT') + bytes([4,0xc2]) + struct.pack('!H',30)
        body += text('cabinet-test-'+run) + text('emqx') + text('public')
        sock.sendall(packet(0x10,body))
        assert receive(sock) == (0x20,b'\x00\x00')
        for number,(state,code,cm,phase) in enumerate([('closed',0,10,'idle'),('open',1,30,'holding'),('unknown',2,None,'returning')],1):
            point = dict(base,door_state=state,door_state_code=code,sensor_ok=cm is not None,servo_phase=phase,servo_phase_code={"idle":0,"holding":2,"returning":3}[phase],sequence=number)
            if cm is None:
                point.pop('distance_cm');point.pop('noise_db');point['noise_fresh']=False
            else:point['distance_cm']=cm
            sock.sendall(packet(0x32,text(topic)+struct.pack('!H',number)+json.dumps(point).encode()))
            assert receive(sock) == (0x40,struct.pack('!H',number))
            time.sleep(.2)
        sock.sendall(b'\xe0\x00')
    print('PASS: broker acknowledged labelled closed/open/unknown cabinet test points')
    query = f'''from(bucket: {json.dumps(env['INFLUX_BUCKET'])})
      |> range(start: -5m)
      |> filter(fn: (r) => r._measurement == "cabinet_monitor" and r.device == {json.dumps(device)})
      |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")'''
    context=ssl.create_default_context(cafile='/etc/ssl/cert.pem') if Path('/etc/ssl/cert.pem').exists() else ssl.create_default_context()
    req=urllib.request.Request(env['INFLUX_URL'].rstrip('/')+'/api/v2/query?org='+urllib.parse.quote(env['INFLUX_ORG']),data=json.dumps({'query':query,'type':'flux'}).encode(),headers={'Authorization':'Token '+env['INFLUX_READ_TOKEN'],'Content-Type':'application/json','Accept':'application/csv'})
    for _ in range(20):
        with urllib.request.urlopen(req,context=context,timeout=15) as response:
            rows=list(csv.DictReader(io.StringIO(response.read().decode())))
        states={r.get('door_state'):r for r in rows if r.get('device')==device}
        if {'closed','open','unknown'}.issubset(states):
            assert states['open']['distance_cm']=='30' and states['closed']['distance_cm']=='10'
            assert states['unknown'].get('distance_cm','')=='' and states['unknown']['noise_fresh']=='false'
            print('PASS: MQTT -> Telegraf -> InfluxDB stored independent door states and omitted unavailable values')
            print('Diagnostic device: '+device+' (software_test, excluded from real dashboard)')
            return
        time.sleep(1)
    raise SystemExit('Cabinet test series missing; check running Telegraf and Influx write permission.')

if __name__ == '__main__':
    main()
