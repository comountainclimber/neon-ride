import {loadCircuit} from './circuit.js';
import {makeWorld} from './world.js';
import {MotorcycleSound} from './engine-sound.js';
import {touchControls} from './touch-controls.js';
import {createCourierDesk} from './courier-desk.js';
import {recoveryControls,distanceLabel} from './recovery.js';

const element=id=>document.getElementById(id),canvas=element('world');
const storage='neon-ride.circuit.v1',keys=new Set(),look={yaw:0,pitch:0};
let circuit,world,active=false,began=false,back=null,drag=null,lastTap=null,lastTime=0,accumulator=0,lastSave=0;
let sound=true,saved=null,toastTimer,controllerMenu=false,lastCrash=false,padReset=false;
let desk, firstDispatch=false, padHeld=[],onFoot=false;
try{saved=JSON.parse(localStorage.getItem(storage));sound=localStorage.getItem(storage+'.sound')!=='off';}catch{}
const engineSound=new MotorcycleSound();
const touch=touchControls(element('touch'),()=>active);
const recovery=recoveryControls(()=>active&&onFoot);
const tracks=['rain','wind_woosh_loop'].map(name=>{
  const a=new Audio('./assets/audio/'+name+'.wav');a.loop=true;a.preload='auto';a.preservesPitch=false;return a;
});
const impactSound=new Audio('./assets/audio/impact.wav');impactSound.preload='auto';impactSound.preservesPitch=false;
function audioState(){
  engineSound.enable(active&&sound&&!onFoot);
  if(!active||!sound)impactSound.pause();
  element('sound').innerHTML='♪ <span>Sound '+(sound?'on':'off')+'</span>';
  element('sound').setAttribute('aria-label','Turn sound '+(sound?'off':'on'));
  element('sound').setAttribute('aria-pressed',String(sound));
  element('pauseSound').textContent='Sound '+(sound?'on':'off');
  element('pauseSound').setAttribute('aria-pressed',String(sound));
  tracks.forEach((a,i)=>{if(active&&sound){a.volume=[.17,.025][i];a.play().catch(()=>{});}else a.pause();});
}
function save(){
  if(!began||!circuit)return;
  const p=circuit.ride();if(p.crashAge>=0&&!p.foot?.active)return;
  saved={s:p.s,lateral:p.lateral,laps:p.laps,charge:p.charge,elapsed:p.elapsed,headingError:p.headingError||0,
    courier:circuit.courier.state(),navigation:desk?.preferences()};
  if(p.foot?.active){saved.foot=p.foot;saved.bikeFall=p.bikeFall;}
  try{localStorage.setItem(storage,JSON.stringify(saved));}catch{}
}
function notify(text){clearTimeout(toastTimer);element('notice').textContent=text;toastTimer=setTimeout(()=>element('notice').textContent='',2200);}
function clearInputs(){
  keys.clear();touch.reset();recovery.reset();
  if(drag&&canvas.hasPointerCapture(drag.id))canvas.releasePointerCapture(drag.id);
  drag=null;lastTap=null;
}
function stop(){active=false;accumulator=0;clearInputs();audioState();save();}
function play(){
  if(!world)return;
  clearInputs();active=began=true;document.body.dataset.screen='game';lastTime=performance.now();canvas.focus({preventScroll:true});
  const p=circuit.ride();onFoot=Boolean(p.foot?.active);recovery.show(circuit,p);audioState();
  if(!onFoot)desk?.refresh(true);
  if(firstDispatch){firstDispatch=false;desk.open('jobs');}
}
function open(id,previous=null){
  if(id==='dispatch'&&recovering())return;
  if(active)stop();document.querySelectorAll('dialog[open]').forEach(d=>d.close());back=previous;element(id).showModal();
}
function close(){
  const d=document.querySelector('dialog[open]');if(!d)return;d.close();
  if(back){const b=back;back=null;open(b);return;}
  if(document.body.dataset.screen==='game')play();else element('enter').focus();
}
function reset(){
  if(!active||recovering())return;
  clearInputs();const p=circuit.ride(), shift=circuit.courier.state();circuit.restore({s:0,lateral:2.65,laps:p.laps,charge:p.charge,headingError:0,elapsed:p.elapsed});circuit.courier.restore(shift);look.yaw=look.pitch=0;accumulator=0;lastTime=performance.now();
  lastCrash=false;hud(circuit.ride());save();notify('Back at Rainline');
}
function recovering(){
  if(!circuit)return false;
  const p=circuit.ride();return Boolean(p.foot?.active)||p.crashAge>=0;
}
function interact(){
  if(!active)return;
  const p=circuit.ride();
  if(p.foot?.active){
    if(circuit.remount()){
      clearInputs();look.yaw=look.pitch=0;
      hud(circuit.ride());desk.refresh(true);save();notify('Back on the bike');
    }
  }else if(p.crashAge<0)desk.interact();
}
element('remount').onclick=interact;
function title(){
  stop();document.querySelectorAll('dialog[open]').forEach(d=>d.close());document.body.dataset.screen='title';
  element('enter').innerHTML='Continue shift <span>→</span>';back=null;
}
element('enter').onclick=play;element('how').onclick=()=>open('help');element('pauser').onclick=()=>open('menu');
element('brand').onclick=e=>{e.preventDefault();if(active)open('menu');};element('resume').onclick=close;
element('menuHelp').onclick=()=>open('help','menu');element('menuTitle').onclick=title;
element('menuDesk').onclick=()=>desk.open();
element('sound').onclick=()=>{sound=!sound;audioState();try{localStorage.setItem(storage+'.sound',sound?'on':'off');}catch{}};
element('pauseSound').onclick=()=>element('sound').click();
const canFullscreen=Boolean(document.fullscreenEnabled && document.documentElement.requestFullscreen);
element('fullscreen').hidden=element('pauseFullscreen').hidden=!canFullscreen;
element('pauseFullscreen').onclick=()=>element('fullscreen').click();
element('fullscreen').onclick=async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else await document.documentElement.requestFullscreen();}
  catch{notify('Fullscreen is unavailable here.');}};
document.addEventListener('fullscreenchange',()=>{
  const title=document.fullscreenElement?'Exit fullscreen':'Enter fullscreen';
  element('fullscreen').setAttribute('aria-label',title);
  element('pauseFullscreen').textContent=document.fullscreenElement?'Exit fullscreen':'Fullscreen';
});
document.querySelectorAll('[data-close]').forEach(b=>b.onclick=close);
document.querySelectorAll('dialog').forEach(d=>d.addEventListener('cancel',e=>{e.preventDefault();close();}));

canvas.onpointerdown=e=>{
  if(active && !drag){canvas.setPointerCapture(e.pointerId);drag={id:e.pointerId,x:e.clientX,y:e.clientY,originX:e.clientX,originY:e.clientY,moved:false};}
};
canvas.onpointermove=e=>{
  if(!drag||drag.id!==e.pointerId||!active)return;
  if(Math.hypot(e.clientX-drag.originX,e.clientY-drag.originY)>9)drag.moved=true;
  const yaw=(e.clientX-drag.x)*.003;
  if(onFoot)recovery.turn(yaw);else look.yaw=Math.max(-1.75,Math.min(1.75,look.yaw+yaw));
  look.pitch=Math.max(-.6,Math.min(.65,look.pitch-(e.clientY-drag.y)*.003));
  drag.x=e.clientX;drag.y=e.clientY;
};
canvas.onpointerup=e=>{
  if(!drag||drag.id!==e.pointerId)return;
  const now=performance.now();
  if(e.pointerType==='touch'&&!drag.moved&&active){
    if(lastTap && now-lastTap.time<320 && Math.hypot(lastTap.x-e.clientX,lastTap.y-e.clientY)<32){
      look.yaw=look.pitch=0; lastTap=null;
    }else lastTap={time:now,x:e.clientX,y:e.clientY};
  }else lastTap=null;
  drag=null;
};
canvas.onlostpointercapture=e=>{if(drag?.id===e.pointerId)drag=null;};
document.addEventListener('keydown',e=>{
  if(!active||e.metaKey||e.ctrlKey||e.altKey)return;
  if(e.code.startsWith('Arrow')||e.code==='Space')e.preventDefault();keys.add(e.code);
  if(e.repeat)return;if(e.code==='Escape'||e.code==='KeyP'){e.preventDefault();open('menu');}
  if(e.code==='KeyC')look.pitch=look.yaw=0;
  if(e.code==='KeyR')reset();
  if(e.code==='KeyM'&&!recovering())desk.open();
  if(e.code==='KeyE')interact();
});
document.addEventListener('keydown',e=>{
  if(e.code==='KeyM' && element('dispatch').open && !e.repeat){e.preventDefault();e.stopImmediatePropagation();close();}
},true);
document.addEventListener('keyup',e=>keys.delete(e.code));window.addEventListener('blur',()=>{if(active)open('menu');});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&active)open('menu');});
window.addEventListener('pagehide',save);
const down=(...codes)=>codes.some(code=>keys.has(code));
function input(pad){
  const buttons=pad?.buttons||[],stick=pad?.axes[0]||0;
  const steering=Number(down('KeyD','ArrowRight'))-Number(down('KeyA','ArrowLeft'))+touch.state.steer+(Math.abs(stick)>.12?stick:0);
  return [Math.max(Number(down('KeyW','ArrowUp')),buttons[7]?.value||0,touch.state.gas),
    Math.max(Number(down('KeyS','ArrowDown')),buttons[6]?.value||0,touch.state.brake),Math.max(-1,Math.min(1,steering)),
    down('ShiftLeft','ShiftRight')||touch.state.boost||buttons[2]?.pressed||false];
}
function hud(p,pad){
  const wasOnFoot=onFoot;onFoot=Boolean(p.foot?.active);
  if(wasOnFoot!==onFoot){
    clearInputs();look.yaw=look.pitch=0;audioState();save();
  }
  recovery.show(circuit,p,pad);
  if(!onFoot&&p.crashAge<0)desk.refresh();
  const speed=Math.round(p.v*3.6),charge=Math.round(p.charge*100);
  const stats=element('telemetry');stats.value=onFoot?'On foot · Motorcycle '+distanceLabel(p.foot.distance):speed+' km/h · Gear '+p.gear+' · Nitro '+charge+'%';
  stats.dataset.foot=String(onFoot);
  stats.dataset.speed=speed;stats.dataset.charge=charge;stats.dataset.crash=String(p.crashAge>=0);stats.dataset.boost=String(Boolean(p.boost));
  if(p.crashAge>=0&&!lastCrash&&!onFoot){
    notify('Impact');
    if(sound){
      const severity=Math.max(0,Math.min(1,p.severity));
      impactSound.volume=.38+.44*severity;impactSound.playbackRate=1.13-.21*severity;impactSound.currentTime=0;
      impactSound.play().catch(()=>{});
    }
  }
  lastCrash=p.crashAge>=0;
  if(sound)tracks[1].volume=onFoot?.025:.025+.28*Math.min(1,p.v/82)+.1*p.boost;
}
function update(now){
  requestAnimationFrame(update);
  const pad=Array.from(navigator.getGamepads?.()||[]).find(g=>g?.connected);
  const menu=Boolean(pad?.buttons[9]?.pressed);
  const held=pad?.buttons.map(b=>b.pressed)??[], pressed=i=>held[i]&&!padHeld[i];
  if(menu&&!controllerMenu){if(active)open('menu');else if(document.querySelector('dialog[open]'))close();}
  else if(active && pressed(8)&&!recovering())desk.open();
  else if(active && pressed(0))interact();
  else if(!active && document.querySelector('dialog[open]')){
    const dialog=document.querySelector('dialog[open]');
    if(pressed(1)||pressed(8))close();
    else if(pressed(0))document.activeElement?.click();
    else if([12,13,14,15].some(pressed)){
      const buttons=[...dialog.querySelectorAll('button:not(:disabled),input:not(:disabled)')].filter(b=>b.getClientRects().length);
      const delta=pressed(12)||pressed(14)?-1:1;
      const next=buttons[(Math.max(0,buttons.indexOf(document.activeElement))+delta+buttons.length)%buttons.length];
      next?.focus();
    }
  }else if(document.body.dataset.screen==='title' && pressed(0) && !element('enter').disabled)play();
  padHeld=held;
  controllerMenu=menu;
  if(!active||!world)return;
  const elapsed=Math.min(.2,Math.max(0,(now-lastTime)/1000));accumulator+=elapsed;lastTime=now;
  if(pad){
    if(pad.buttons[3]?.pressed&&!padReset)reset();
    padReset=Boolean(pad.buttons[3]?.pressed);
    if(pad.buttons[11]?.pressed)look.yaw=look.pitch=0;
    else{
      const [x=0,y=0]=pad.axes.slice(2,4);
      if(!onFoot&&Math.abs(x)>.12)look.yaw=Math.max(-1.75,Math.min(1.75,look.yaw+x*elapsed*2.1));
      if(Math.abs(y)>.12)look.pitch=Math.max(-.6,Math.min(.65,look.pitch-y*elapsed*1.65));
    }
  }
  const controls=input(pad);
  let p=circuit.ride();
  while(accumulator>=1/120){
    if(p.foot?.active)recovery.step(circuit,down,pad,1/120);
    else{
      circuit.drive(...controls,1/120);
      if(p.crashAge>=0)p=circuit.ride();
    }
    accumulator-=1/120;
  }
  p=circuit.ride();hud(p,pad);
  if(!onFoot)engineSound.update(p,controls[0],controls[1]);
  world.draw(p,look,onFoot?0:controls[1],desk.view());
  if(now-lastSave>4000){lastSave=now;save();}
}
async function start(){
  audioState();circuit=await loadCircuit();circuit.restore(saved);
  if(saved?.courier)circuit.courier.restore(saved.courier);
  desk=createCourierDesk(circuit,{open:()=>open('dispatch'),close,save,notify},saved?.navigation);
  firstDispatch=!saved?.courier;
  world=await makeWorld(canvas,circuit,(n,total)=>{
    const progress=Math.round(98*n/total)+'%';
    element('progress').textContent=progress;element('enter').style.setProperty('--loading',progress);
  });
  element('progress').textContent='99%';world.draw(circuit.ride(),look);
  element('enter').innerHTML=(saved?.courier?'Continue shift':'Start shift')+' <span>→</span>';element('enter').disabled=false;
  requestAnimationFrame(update);
}
start().catch(e=>{
  console.error(e);
  element('error').textContent='The circuit could not load. Check your connection and that WebGL 2 is available.';
  const retry=element('enter');retry.textContent='Reload circuit';retry.disabled=false;retry.onclick=()=>location.reload();
});
