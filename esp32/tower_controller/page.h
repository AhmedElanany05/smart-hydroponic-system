// Status page served at "/". It loads the numbers from /status every 3 s and sends the buttons to /cmd.
#pragma once

static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tower</title>
<style>
:root{--bg:#f3f5f2;--card:#fff;--ink:#1d2320;--muted:#66706a;--line:#e1e6e2;--ok:#2e8b57;--warn:#c98a12;--bad:#c4402f;--accent:#2f6f5e}
@media(prefers-color-scheme:dark){:root{--bg:#141816;--card:#1e2421;--ink:#e6ebe8;--muted:#9aa59f;--line:#2e3632;--accent:#6cc3a4}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,"Segoe UI",sans-serif}
header{display:flex;justify-content:space-between;align-items:center;padding:16px;max-width:980px;margin:auto}
h1{font-size:20px;margin:0}.sub{color:var(--muted);font-size:13px}
main{max-width:980px;margin:auto;padding:0 16px 32px;display:grid;gap:14px;grid-template-columns:repeat(auto-fit,minmax(280px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px}
.card h2{font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);margin:0 0 10px}
.row{display:flex;justify-content:space-between;align-items:baseline;padding:6px 0;border-bottom:1px solid var(--line)}
.row:last-child{border-bottom:0}.v{font-weight:600;font-variant-numeric:tabular-nums}
.big{font-size:26px;font-weight:650}
.pill{display:inline-block;padding:2px 10px;border-radius:99px;font-size:13px;font-weight:600;background:var(--line)}
.on{background:#d8f0e2;color:#1d6b41}.off{background:var(--line);color:var(--muted)}.alert{background:#f8dcd7;color:#9b2c1f}.mid{background:#f7e9cc;color:#86590a}
@media(prefers-color-scheme:dark){.on{background:#1f4632;color:#9fe3bd}.alert{background:#4a221c;color:#f2a99c}.mid{background:#453516;color:#f1cd85}}
.btns{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
button{border:1px solid var(--line);background:var(--bg);color:var(--ink);border-radius:8px;padding:8px 12px;font:inherit;cursor:pointer}
button.p{background:var(--accent);border-color:var(--accent);color:#fff}button:disabled{opacity:.5}
.events{grid-column:1/-1}pre{margin:0;white-space:pre-wrap;font:13px/1.6 ui-monospace,Consolas,monospace;color:var(--muted)}
#msg{color:var(--muted);font-size:13px;min-height:18px;margin-top:8px}
</style></head><body>
<header><div><h1>Strawberry tower</h1><div class="sub" id="time">connecting...</div></div><span class="pill" id="link">-</span></header>
<main>
<section class="card"><h2>Water</h2>
<div class="row"><span>pH</span><span class="v big" id="ph">-</span></div>
<div class="row"><span>Nutrients (TDS)</span><span class="v" id="tds">-</span></div>
<div class="row"><span>Tank</span><span id="tank" class="pill">-</span></div></section>

<section class="card"><h2>Climate</h2>
<div class="row"><span>Air temperature</span><span class="v big" id="at">-</span></div>
<div class="row"><span>Humidity</span><span class="v" id="ah">-</span></div>
<div class="row"><span>Soil moisture</span><span class="v" id="soil">-</span></div></section>

<section class="card"><h2>Power</h2>
<div class="row"><span>Battery</span><span class="v big" id="bat">-</span></div>
<div class="row"><span>Load protection</span><span id="shed" class="pill">-</span></div>
<div class="row"><span>Running for</span><span class="v" id="up">-</span></div></section>

<section class="card"><h2>Pumps</h2>
<div class="row"><span>Tower pump</span><span id="tp" class="pill">-</span></div>
<div class="row"><span>Nutrient A</span><span id="na" class="pill">-</span></div>
<div class="row"><span>Nutrient B</span><span id="nb" class="pill">-</span></div>
<div class="row"><span>Dosing</span><span class="v" id="dose">-</span></div>
<div class="btns"><button onclick="cmd('pump=tower&sec=60')">Tower 60 s</button><button onclick="cmd('pump=a&sec=5')">A 5 s</button>
<button onclick="cmd('pump=b&sec=5')">B 5 s</button><button onclick="cmd('pump=all&sec=0')">Stop all</button></div></section>

<section class="card"><h2>Jetson</h2>
<div class="row"><span>Power</span><span id="js" class="pill">-</span></div>
<div class="row"><span>Last message</span><span class="v" id="jseen">-</span></div>
<div class="row"><span>Last plant check</span><span class="v" id="res">-</span></div>
<div class="btns"><button class="p" onclick="cmd('maintenance=1')">Power on (stay on)</button>
<button onclick="cmd('jetson=off')">Ask to shut down</button><button onclick="cmd('maintenance=0')">End stay-on</button></div>
<div id="msg"></div></section>

<section class="card events"><h2>Events</h2><pre id="ev">-</pre></section>
</main>
<script>
const $=id=>document.getElementById(id);
const f=(v,d,u)=>v===null||v===undefined?'-':Number(v).toFixed(d)+(u||'');
function pill(el,text,cls){el.textContent=text;el.className='pill '+cls}
function dur(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?h+' h '+m+' min':m+' min'}
async function load(){
 try{const r=await fetch('/status',{cache:'no-store'});const s=await r.json();
  $('time').textContent=s.time;pill($('link'),'online','on');
  $('ph').textContent=f(s.ph,2);$('tds').textContent=f(s.tds_ppm,0,' ppm');
  pill($('tank'),s.tank_ok?'water OK':'tank low - pumps blocked',s.tank_ok?'on':'alert');
  $('at').textContent=f(s.air_temp_c,1,' °C');$('ah').textContent=f(s.air_humidity,0,' %');$('soil').textContent=f(s.soil_pct,0,' %');
  $('bat').textContent=s.battery_v===null?'USB only':f(s.battery_v,2,' V');
  const sh=Object.keys(s.shed).filter(k=>s.shed[k]);pill($('shed'),sh.length?'cut: '+sh.join(', '):'all loads allowed',sh.length?'mid':'on');
  $('up').textContent=dur(s.uptime_s);
  pill($('tp'),s.tower_pump?'ON':'off',s.tower_pump?'on':'off');pill($('na'),s.nutrient_a?'ON':'off',s.nutrient_a?'on':'off');
  pill($('nb'),s.nutrient_b?'ON':'off',s.nutrient_b?'on':'off');$('dose').textContent=s.dosing;
  pill($('js'),s.jetson+(s.maintenance?' (stay on)':''),s.jetson==='off'?'off':'on');
  $('jseen').textContent=s.jetson_seen_s===null?'never':s.jetson_seen_s+' s ago';
  $('res').textContent=s.last_result+(s.last_result_time!=='-'?' ('+s.last_result_time+')':'');
  $('ev').textContent=s.events.join('\n')||'-';
 }catch(e){pill($('link'),'offline','alert')}
}
async function cmd(q){
 let k=localStorage.getItem('towerKey');
 if(!k){k=prompt('Control key (API_KEY from secrets.h)');if(!k)return;localStorage.setItem('towerKey',k)}
 const r=await fetch('/cmd?key='+encodeURIComponent(k)+'&'+q);const j=await r.json().catch(()=>({}));
 if(r.status===403){localStorage.removeItem('towerKey');$('msg').textContent='Wrong key - try again.'}
 else $('msg').textContent=j.done?'Done: '+j.done:'Error: '+(j.error||r.status);
 load();
}
load();setInterval(load,3000);
</script></body></html>)rawliteral";
