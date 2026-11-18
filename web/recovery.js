const el = id => document.getElementById(id);
const clamp = value => Math.max(-1,Math.min(1,value));
const axis = value => Math.abs(value||0)>.14?value:0;
export const distanceLabel = meters => (meters<10?meters.toFixed(1):Math.round(meters))+' m';

export function recoveryControls(isActive) {
  const pad=el('walkPad'), panel=el('recovery');
  let pointer=null,forward=0,right=0,dragTurn=0;
  function point(event) {
    const box=pad.getBoundingClientRect(), radius=(box.width-25)/2;
    const x=(event.clientX-box.left-box.width/2)/radius;
    const y=(event.clientY-box.top-box.height/2)/radius;
    const limit=Math.max(1,Math.hypot(x,y));
    right=x/limit;forward=-y/limit;
    pad.style.setProperty('--walk-x',right*radius+'px');
    pad.style.setProperty('--walk-y',-forward*radius+'px');
  }
  function reset() {
    const id=pointer;pointer=null;forward=right=dragTurn=0;
    if(id!==null&&pad.hasPointerCapture(id))pad.releasePointerCapture(id);
    pad.removeAttribute('data-held');pad.style.removeProperty('--walk-x');pad.style.removeProperty('--walk-y');
  }
  pad.addEventListener('pointerdown',event=>{
    if(!isActive()||pointer!==null||event.button!==0)return;
    event.preventDefault();pointer=event.pointerId;pad.setPointerCapture(pointer);pad.setAttribute('data-held','');point(event);
  });
  pad.addEventListener('pointermove',event=>{if(pointer===event.pointerId)point(event);});
  for(const type of ['pointerup','pointercancel','lostpointercapture'])pad.addEventListener(type,event=>{if(pointer===event.pointerId)reset();});
  pad.addEventListener('contextmenu',event=>event.preventDefault());
  function step(circuit,down,gamepad,dt) {
    const axes=gamepad?.axes||[];
    const f=Number(down('KeyW','ArrowUp'))-Number(down('KeyS','ArrowDown'))+forward-axis(axes[1]);
    const r=Number(down('KeyD'))-Number(down('KeyA'))+right+axis(axes[0]);
    const turn=Number(down('ArrowRight'))-Number(down('ArrowLeft'))+axis(axes[2]);
    circuit.walk(clamp(f),clamp(r),dragTurn+clamp(turn)*2.1*dt,dt);
    dragTurn=0;
  }
  function show(circuit,p,gamepad) {
    const foot=p.foot;
    const phase=foot?.active?'foot':p.crashAge>=0?'crash':'ride';
    document.body.dataset.phase=phase;panel.hidden=phase!=='foot';
    if(panel.hidden)return;
    const rider=circuit.frame(foot.s,foot.lateral), bike=circuit.frame(p.s,p.lateral);
    const bearing=Math.atan2(bike.z-rider.z,bike.x-rider.x)-foot.yaw;
    el('bikeBearing').style.transform='rotate('+bearing+'rad)';
    el('bikeDistance').textContent=distanceLabel(foot.distance);
    const close=foot.canMount ?? (foot.distance<=2.4);
    el('remount').hidden=!close;
    const touch=matchMedia('(pointer:coarse)').matches;
    el('walkHint').textContent=close?(gamepad?'Press A / Cross to remount':touch?'Tap to get back on':'Press E to remount'):
      (gamepad?'Left stick to walk · Right stick to look':touch?'Slide to walk · Drag the road to look':'WASD to walk · Drag or use ← → to look');
  }
  return {step,show,reset,turn:delta=>{dragTurn+=delta;}};
}
