#pragma once
#include <Arduino.h>
const char CABINET_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Breaker box controller</title><style>
body{font:16px system-ui;background:#10202d;color:#e9f1f7;margin:0}main{max-width:760px;margin:auto;padding:24px}h1{font-size:26px}h2{font-size:20px;margin-top:28px}.stats{display:flex;flex-wrap:wrap;gap:24px}.stats p{margin:6px 0}strong{font-size:23px}form{display:grid;grid-template-columns:1fr 1fr;gap:18px}label{display:block}input{display:block;box-sizing:border-box;width:100%;margin-top:6px;padding:10px;border:1px solid #73899b;border-radius:4px;font:inherit;background:#182f40;color:inherit}button{font:inherit;padding:12px 18px;background:#70d0bb;border:0;border-radius:4px;color:#10202d}button:disabled{opacity:.5}small{color:#c2cfdb}#message{min-height:24px}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:13px}form button{justify-self:start}@media(max-width:520px){main{padding:18px}form{grid-template-columns:1fr}}
</style></head><body><main><h1>Breaker box controller</h1>
<div class="stats"><p>Door<br><strong id="door">Connecting…</strong></p><p>Distance<br><strong id="distance">—</strong></p><p>Noise source<br><strong id="noise">—</strong></p><p>Servo<br><strong id="servo">—</strong></p></div>
<p id="connection">Connecting to board…</p><p id="gate">—</p>
<h2>Configuration</h2><p>Door sensing is independent of button pressing. Open above the threshold; close 2 cm below it to avoid flicker.</p>
<form id="config">
<label>Noise trigger (30–120 dB)<input name="trigger_db" type="number" min="30" max="120" step="0.1" required></label>
<label>Continuous high noise (1–30 seconds)<input name="sustain_ms" type="number" min="1" max="30" step="0.1" required></label>
<label>Rest / reset angle (0–180°)<input name="rest_angle" type="number" min="0" max="180" step="1" required></label>
<label>Button press angle (0–180°)<input name="press_angle" type="number" min="0" max="180" step="1" required></label>
<label>Time each way (0.2–3 seconds)<input name="travel_ms" type="number" min="0.2" max="3" step="0.1" required></label>
<label>Hold button (0.1–3 seconds)<input name="hold_ms" type="number" min="0.1" max="3" step="0.1" required></label>
<label>Door open above (5–350 cm)<input name="door_threshold_cm" type="number" min="5" max="350" step="0.1" required></label>
<button id="save" type="submit">Save configuration</button></form>
<p><small>Angles are servo command values. Check actual travel without the horn attached; mechanical travel varies. Settings persist after restart.</small></p>
<h2>Servo test</h2><p>This physically performs one press and return. Use an unloaded servo first, then fit the horn to a harmless demonstration button.</p>
<button id="test" type="button">Test one button press</button><p id="message" role="status" aria-live="polite"></p>
<p><small>One automatic press per noise episode. Rearms after 3 seconds below trigger − 2 dB, after the servo returns. A brief peak will not trigger it. Stale noise cannot trigger it.</small></p>
<pre id="topics"></pre></main><script>
const form=document.getElementById('config'),msg=document.getElementById('message');
const fields=['trigger_db','sustain_ms','rest_angle','press_angle','travel_ms','hold_ms','door_threshold_cm'];
const times=new Set(['sustain_ms','travel_ms','hold_ms']);let loaded=false,pending=null,polling=false;
function values(){const out={};for(const key of fields)out[key]=Number(form.elements[key].value)*(times.has(key)?1000:1);return out;}
async function poll(){if(polling)return;polling=true;try{
const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('Board unavailable');const s=await r.json();
document.getElementById('door').textContent=s.door_state.toUpperCase();
document.getElementById('distance').textContent=s.sensor_ok?Number(s.distance_cm).toFixed(1)+' cm':'No echo';
document.getElementById('noise').textContent=s.noise_fresh?Number(s.noise_db).toFixed(1)+' dB':'STALE';
document.getElementById('servo').textContent=s.servo_phase+' ('+s.press_count+')';
document.getElementById('connection').textContent='MQTT '+(s.mqtt_connected?'connected':'disconnected')+' · settings storage '+(s.storage_ready?'ready':'unavailable');
document.getElementById('gate').textContent=(s.armed?'Armed':'Waiting to rearm')+' · high-noise qualification '+(s.qualified_ms/1000).toFixed(1)+' / '+(s.sustain_ms/1000)+' s';
document.getElementById('topics').textContent='Listening: '+s.source_topic+'\nPublishing: '+s.telemetry_topic;
if(!loaded){for(const key of fields)form.elements[key].value=s[key]/(times.has(key)?1000:1);loaded=true;}
if(pending&&s.command_id===pending.id){let text=s.command_result;if(text==='saved'&&fields.some(k=>Math.abs(s[k]-pending.values[k])>.01))text='Save verification failed';msg.textContent=text;pending=null;}
else if(pending&&Date.now()-pending.at>10000){msg.textContent='Command response timed out; check board status before retrying.';pending=null;}
document.getElementById('save').disabled=!!pending||!s.storage_ready||s.servo_phase!=='idle';
document.getElementById('test').disabled=!!pending||s.servo_phase!=='idle';
}catch(e){document.getElementById('connection').textContent=e.message;document.getElementById('save').disabled=true;document.getElementById('test').disabled=true;}finally{polling=false;}}
async function send(path,body){try{document.getElementById('save').disabled=true;document.getElementById('test').disabled=true;
const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const data=await r.json();if(!r.ok)throw Error(data.message);pending={id:data.command_id,values:body,at:Date.now()};msg.textContent='Waiting for board…';
}catch(e){msg.textContent=e.message;}await poll();}
form.addEventListener('submit',e=>{e.preventDefault();send('/api/config',values());});
document.getElementById('test').addEventListener('click',()=>send('/api/press',{}));poll();setInterval(poll,500);
</script></body></html>)HTML";
