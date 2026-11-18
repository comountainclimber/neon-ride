import {loadCircuit} from './circuit.js';
import {makeWorld,sector} from './world.js';
import {MotorcycleSound} from './engine-sound.js';

const element=id=>document.getElementById(id),canvas=element('world');
const storage='neon-ride.circuit.v1',keys=new Set(),touchKeys=new Map(),look={yaw:0,pitch:0};
let circuit,world,active=false,began=false,back=null,drag=null,lastTime=0,accumulator=0,lastSave=0;
let sound=true,saved=null,toastTimer,controllerMenu=false,lastCrash=false,lastLap=0,padReset=false;
try{saved=JSON.parse(localStorage.getItem(storage));sound=localStorage.getItem(storage+'.sound')!=='off';}catch{}
const engineSound=new MotorcycleSound();
const tracks=['rain','wind_woosh_loop'].map(name=>{
  const a=new Audio('./assets/audio/'+name+'.wav');a.loop=true;a.preload='auto';a.preservesPitch=false;return a;
});
const impactSound=new Audio('./assets/audio/impact.wav');impactSound.preload='auto';impactSound.preservesPitch=false;
function audioState(){
  engineSound.enable(active&&sound);
  if(!active||!sound)impactSound.pause();
  element('sound').innerHTML='♪ <span>Sound '+(sound?'on':'off')+'</span>';
  element('sound').setAttribute('aria-label','Turn sound '+(sound?'off':'on'));
  element('sound').setAttribute('aria-pressed',String(sound));
  tracks.forEach((a,i)=>{if(active&&sound){a.volume=[.17,.025][i];a.play().catch(()=>{});}else a.pause();});
}
function save(){
  if(!began||!circuit)return;
  const p=circuit.ride();if(p.crashAge>=0)return;
  saved={s:p.s,lateral:p.lateral,laps:p.laps,charge:p.charge,elapsed:p.elapsed,headingError:p.headingError||0};
  try{localStorage.setItem(storage,JSON.stringify(saved));}catch{}
}
function notify(text){clearTimeout(toastTimer);element('notice').textContent=text;toastTimer=setTimeout(()=>element('notice').textContent='',2200);}
function clearInputs(){keys.clear();touchKeys.clear();drag=null;document.querySelectorAll('[data-key]').forEach(b=>b.setAttribute('aria-pressed','false'));}
function stop(){active=false;accumulator=0;clearInputs();audioState();save();}
function play(){
  if(!world)return;
  clearInputs();active=began=true;document.body.dataset.screen='game';lastTime=performance.now();canvas.focus({preventScroll:true});audioState();
}
function open(id,previous=null){
  if(active)stop();document.querySelectorAll('dialog[open]').forEach(d=>d.close());back=previous;element(id).showModal();
}
function close(){
  const d=document.querySelector('dialog[open]');if(!d)return;d.close();
  if(back){const b=back;back=null;open(b);return;}
  if(document.body.dataset.screen==='game')play();else element('enter').focus();
}
function reset(){
  if(!active)return;
  clearInputs();circuit.restore(null);look.yaw=look.pitch=0;accumulator=0;lastTime=performance.now();
  lastCrash=false;lastLap=0;hud(circuit.ride());save();notify('Back at Rainline');
}
function title(){
  stop();document.querySelectorAll('dialog[open]').forEach(d=>d.close());document.body.dataset.screen='title';
  element('enter').innerHTML='Continue ride <span>→</span>';back=null;
}
element('enter').onclick=play;element('how').onclick=()=>open('help');element('pauser').onclick=()=>open('menu');
element('brand').onclick=e=>{e.preventDefault();if(active)open('menu');};element('resume').onclick=close;
element('menuHelp').onclick=()=>open('help','menu');element('menuTitle').onclick=title;
element('sound').onclick=()=>{sound=!sound;audioState();try{localStorage.setItem(storage+'.sound',sound?'on':'off');}catch{}};
element('fullscreen').onclick=async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else await document.documentElement.requestFullscreen();}
  catch{notify('Fullscreen is unavailable here.');}};
document.addEventListener('fullscreenchange',()=>element('fullscreen').setAttribute('aria-label',document.fullscreenElement?'Exit fullscreen':'Enter fullscreen'));
document.querySelectorAll('[data-close]').forEach(b=>b.onclick=close);
document.querySelectorAll('dialog').forEach(d=>d.addEventListener('cancel',e=>{e.preventDefault();close();}));
document.querySelectorAll('[data-key]').forEach(b=>{
  b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);touchKeys.set(e.pointerId,b.dataset.key);b.setAttribute('aria-pressed','true');};
  for(const ev of ['pointerup','pointercancel','lostpointercapture'])b.addEventListener(ev,e=>{touchKeys.delete(e.pointerId);b.setAttribute('aria-pressed',String([...touchKeys.values()].includes(b.dataset.key)));});
});
canvas.onpointerdown=e=>{if(active){canvas.setPointerCapture(e.pointerId);drag={id:e.pointerId,x:e.clientX,y:e.clientY};}};
canvas.onpointermove=e=>{
  if(!drag||drag.id!==e.pointerId||!active)return;
  look.yaw=Math.max(-1.75,Math.min(1.75,look.yaw+(e.clientX-drag.x)*.003));
  look.pitch=Math.max(-.6,Math.min(.65,look.pitch-(e.clientY-drag.y)*.003));
  drag.x=e.clientX;drag.y=e.clientY;
};
canvas.onlostpointercapture=()=>drag=null;
document.addEventListener('keydown',e=>{
  if(!active||e.metaKey||e.ctrlKey||e.altKey)return;
  if(e.code.startsWith('Arrow')||e.code==='Space')e.preventDefault();keys.add(e.code);
  if(e.repeat)return;if(e.code==='Escape'||e.code==='KeyP'){e.preventDefault();open('menu');}
  if(e.code==='KeyC')look.pitch=look.yaw=0;
  if(e.code==='KeyR')reset();
});
document.addEventListener('keyup',e=>keys.delete(e.code));window.addEventListener('blur',()=>{if(active)open('menu');});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&active)open('menu');});
window.addEventListener('pagehide',save);
const down=(...codes)=>codes.some(code=>keys.has(code)||[...touchKeys.values()].includes(code));
function input(pad){
  const buttons=pad?.buttons||[],stick=pad?.axes[0]||0;
  return [Math.max(Number(down('KeyW','ArrowUp')),buttons[7]?.value||0),Math.max(Number(down('KeyS','ArrowDown')),buttons[6]?.value||0),
    Math.max(-1,Math.min(1,Number(down('KeyD','ArrowRight'))-Number(down('KeyA','ArrowLeft'))+(Math.abs(stick)>.12?stick:0))),down('ShiftLeft','ShiftRight')||buttons[2]?.pressed||buttons[0]?.pressed||false];
}
function hud(p){
  element('area').textContent=sector(p.s,circuit.length);element('distance').textContent=(p.s/1000).toFixed(2)+' km';
  element('meter').style.width=(100*p.s/circuit.length).toFixed(3)+'%';element('lap').textContent='Lap '+String(p.laps+1).padStart(2,'0');
  const speed=Math.round(p.v*3.6),charge=Math.round(p.charge*100);
  const stats=element('telemetry');stats.value=speed+' km/h · Gear '+p.gear+' · Nitro '+charge+'%';
  stats.dataset.speed=speed;stats.dataset.charge=charge;stats.dataset.crash=String(p.crashAge>=0);stats.dataset.boost=String(Boolean(p.boost));
  if(p.crashAge>=0&&!lastCrash){
    notify('Impact');
    if(sound){
      const severity=Math.max(0,Math.min(1,p.severity));
      impactSound.volume=.38+.44*severity;impactSound.playbackRate=1.13-.21*severity;impactSound.currentTime=0;
      impactSound.play().catch(()=>{});
    }
  }
  if(p.crashAge<0&&lastCrash)notify('Bike recovered');
  if(p.laps>lastLap)notify('Lap '+(p.laps+1));lastLap=p.laps;lastCrash=p.crashAge>=0;
  if(sound)tracks[1].volume=.025+.28*Math.min(1,p.v/82)+.1*p.boost;
}
function update(now){
  requestAnimationFrame(update);
  const pad=Array.from(navigator.getGamepads?.()||[]).find(g=>g?.connected);
  const menu=Boolean(pad?.buttons[9]?.pressed||pad?.buttons[8]?.pressed);
  if(menu&&!controllerMenu){if(active)open('menu');else if(element('menu').open)close();}
  controllerMenu=menu;
  if(!active||!world)return;
  const elapsed=Math.min(.2,Math.max(0,(now-lastTime)/1000));accumulator+=elapsed;lastTime=now;
  if(pad){
    if(pad.buttons[3]?.pressed&&!padReset)reset();
    padReset=Boolean(pad.buttons[3]?.pressed);
    if(pad.buttons[11]?.pressed)look.yaw=look.pitch=0;
    else{
      const [x=0,y=0]=pad.axes.slice(2,4);
      if(Math.abs(x)>.12)look.yaw=Math.max(-1.75,Math.min(1.75,look.yaw+x*elapsed*2.1));
      if(Math.abs(y)>.12)look.pitch=Math.max(-.6,Math.min(.65,look.pitch-y*elapsed*1.65));
    }
  }
  const controls=input(pad);
  while(accumulator>=1/120){circuit.drive(...controls,1/120);accumulator-=1/120;}
  const p=circuit.ride();hud(p);engineSound.update(p,controls[0],controls[1]);world.draw(p,look,controls[1]);
  if(now-lastSave>4000){lastSave=now;save();}
}
async function start(){
  audioState();circuit=await loadCircuit();circuit.restore(saved);lastLap=circuit.ride().laps;
  world=await makeWorld(canvas,circuit,(n,total)=>element('progress').textContent=Math.round(98*n/total)+'%');
  element('progress').textContent='99%';world.draw(circuit.ride(),look);
  element('enter').innerHTML=(saved?'Continue ride':'Start ride')+' <span>→</span>';element('enter').disabled=false;
  requestAnimationFrame(update);
}
start().catch(e=>{console.error(e);element('error').textContent='The circuit could not load. Enable WebGL 2 and reload to retry. '+e.message;});
