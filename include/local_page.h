#pragma once
#include <Arduino.h>

const char LOCAL_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Building noise monitor</title><style>
body{font:16px system-ui;background:#f1f4f6;color:#172b38;max-width:650px;margin:32px auto;padding:0 20px}
section{background:white;padding:24px;border-radius:12px;margin:18px 0}h1{font-size:26px}
#level{font-size:44px;font-weight:700;margin:8px 0}label{display:block;margin:16px 0 6px}
input[type=number]{padding:10px;font:inherit;width:90%;max-width:240px;border:1px solid #9baeb9;border-radius:5px}
button{font:inherit;background:#163f55;color:white;border:0;padding:12px 18px;border-radius:6px;margin-top:20px}
#message{min-height:24px}small{color:#506574}#state{font-weight:700}
</style><h1>Building noise monitor</h1>
<p>Nearby sound feedback and local configuration.</p>
<section aria-live="polite"><div id="state">Waiting for microphone</div><div id="level">--</div>
<small>Estimated dB SPL &middot; unweighted &middot; 500 ms RMS</small><p id="health"></p><small id="device"></small></section>
<section><h2>Configure this board</h2><form id="settings">
<label for="warning">Orange threshold (dB)</label><input id="warning" type="number" min="30" max="117.9" step="0.1" required>
<label for="limit">Red threshold (dB)</label><input id="limit" type="number" min="32.1" max="120" step="0.1" required>
<label for="offset">Calibration offset (dB)</label><input id="offset" type="number" min="-30" max="30" step="0.1" required>
<label><input id="calibrated" type="checkbox"> Compared with a reference sound meter</label>
<p><small>Offset = reference reading minus this board's reading (with offset zero). These thresholds describe your chosen range; they are not a hearing-safety standard. Red must exceed orange by more than 2 dB. Settings survive a restart.</small></p>
<button>Save settings</button><p id="message" role="status"></p></form></section>
<script>
const el=id=>document.getElementById(id);let initialized=false,pending=null;
async function refresh(){try{
 const response=await fetch('/api/status',{cache:'no-store'});if(!response.ok)throw Error('HTTP '+response.status);
 const s=await response.json();el('level').textContent=s.sensor_ok?s.estimated_spl_db.toFixed(1)+' dB':'--';
 const labels={safe:'Within configured range',warning:'Close to noise limit',high:'Noise limit exceeded',sensor_fault:'Microphone fault / no varying audio'};
 el('state').textContent=labels[s.state]||s.state;el('state').style.color={safe:'#167144',warning:'#9c5a00',high:'#bf2424',sensor_fault:'#69399d'}[s.state];
 el('health').textContent=(s.mqtt_connected?'MQTT connected':'MQTT offline')+(s.clipped?' | Audio clipping':'')+(s.calibrated?' | Reference offset applied':' | Uncalibrated');
 el('device').textContent=s.device+' | Sample '+s.sequence+' | Age '+s.sample_age_ms+' ms';
 if(!initialized){el('warning').value=s.warning_db;el('limit').value=s.limit_db;el('offset').value=s.offset_db;el('calibrated').checked=s.calibrated;initialized=true;}
 const near=(a,b)=>Math.abs(a-b)<0.001;
 if(pending&&near(s.warning_db,pending.warning_db)&&near(s.limit_db,pending.limit_db)&&near(s.offset_db,pending.offset_db)&&s.calibrated===pending.calibrated){el('message').textContent='Settings applied and saved.';pending=null;}
 else if(pending&&Date.now()-pending.queuedAt>10000){el('message').textContent='Save has not been confirmed. Check the current settings or retry.';pending=null;}
}catch(e){el('state').textContent='Board unreachable';el('level').textContent='--';el('health').textContent='Reconnect to the same Wi-Fi network.';}}
el('settings').addEventListener('submit',async e=>{e.preventDefault();const values={warning_db:Number(el('warning').value),limit_db:Number(el('limit').value),offset_db:Number(el('offset').value),calibrated:el('calibrated').checked};
 if(values.limit_db-values.warning_db<=2){el('message').textContent='Red must exceed orange by more than 2 dB.';return;}
 try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(values)});const body=await r.json();el('message').textContent=body.message;if(r.ok)pending={...values,queuedAt:Date.now()};}catch(e){el('message').textContent='Could not save. Check the Wi-Fi connection.';}});
refresh();setInterval(refresh,1000);
</script></html>)HTML";
