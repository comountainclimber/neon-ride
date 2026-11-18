// node compare_circuit.mjs <circuit.wasm> <native probe executable>
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
const [, , wasmPath, executable] = process.argv;
const reference = execFileSync(executable,{encoding:'utf8'}).trim().split(/\r?\n/);
const {instance:{exports:w}} = await WebAssembly.instantiate(readFileSync(wasmPath),{
  env:{atan:Math.atan,atan2:Math.atan2,sin:Math.sin,cos:Math.cos,exp:Math.exp,fmod:(a,b)=>a%b}
});
const mem=(ptr,size)=>new Float64Array(w.memory.buffer,ptr,size);
const frame=s=>[...mem(w.course_frame(s,0),11)];
const length=w.loop_length();
assert.deepEqual(frame(length),frame(0));
let rows=0;
for(let scenario=0;scenario<6;scenario++){
  const p=mem(w.reset_ride(),22), car=mem(w.traffic_car(0,0),6);
  p[1]=0;
  if(scenario===1||scenario===2){p[0]=car[0]-car[3]-2;p[1]=car[1];p[2]=car[2]+(scenario===1?28:4);}
  if(scenario===3){p[0]=410;p[1]=-4.9;p[2]=38;p[19]=-.42;}
  if(scenario===4){p[0]=car[0];p[1]=car[1]-2.3;p[2]=25;p[19]=.56;}
  if(scenario===5){p[0]=850;p[2]=22;}
  const initialYaw=frame(p[0])[7]+p[19];
  let crash=false, contact=false, recovered=false, drained=false, full=false;
  let peak=0,separation=0,grounded=false,bounce=false,lastHeight=null;
  const ticks=scenario===0?44000:(scenario===5?100:940);
  for(let tick=0;tick<ticks;tick++){
    let turn=0;
    if(scenario===0){
      const rate=Math.min(21/(p[2]+2),p[2]*.15,1.12);
      turn=(frame(p[0]+4)[10]*p[2]-2.8*p[19]-.20*p[1])/Math.max(.02,rate);
    }
    w.drive(Number(scenario===0&&p[2]<37),0,turn,Number(scenario===0&&tick<600),1/120);
    contact ||= p[13]>0; crash ||= p[15]>=0; recovered ||= crash&&p[15]<0&&p[2]<1;
    drained ||= p[6]===0; full ||= drained&&p[6]===1;
    const e=mem(w.crash_pose(),22);
    if(p[15]>=0){
      peak=Math.max(peak,e[2]);separation=Math.max(separation,Math.hypot(e[0]-e[9],e[1]-e[10]));
      grounded ||= e[14]===1;
      bounce ||= lastHeight!==null && lastHeight<.52 && e[2]>lastHeight+.001;
      assert(e[2]>=.29 && e[11]>=.25 && e.every(Number.isFinite));
      lastHeight=e[2];
    }
    if(scenario===5){
      const yaw=frame(p[0])[7]+p[19];
      assert(Math.abs(Math.atan2(Math.sin(yaw-initialYaw),Math.cos(yaw-initialYaw)))<1e-9,'Neutral input changed world heading');
    }
    if(tick%30!==0)continue;
    const actual=[scenario,tick,p[0],p[2],p[6],p[10],p[15],p[13],p[19],p[1],
      e[0],e[1],e[2],e[3],e[9],e[11],e[6],e[13],e[14],e[15],e[20],e[21]];
    const expected=reference[rows++].split(/\s+/).map(Number);
    actual.forEach((n,i)=>assert(Math.abs(expected[i]-n)<1e-6,
      `Case ${scenario}, tick ${tick}, column ${i}: native ${expected[i]}, web ${n}`));
  }
  if(scenario===0)assert(p[10]>=2&&drained&&full&&!crash);
  if([1,3,4].includes(scenario)) assert(crash&&recovered&&peak>1.3&&separation>2&&grounded&&bounce,
      JSON.stringify({scenario,crash,recovered,peak,separation,grounded,bounce}));
  if(scenario===2)assert(contact&&!crash);
  if(scenario===5)assert(Math.abs(p[1])>.05&&!crash,'Unsteered bike must drift off the line around a bend');
}
assert.equal(rows,reference.length);
console.log(`${rows} native/WASM samples matched: two manually steered laps, exact neutral heading, recharge, soft contact, rear/side impacts, rail ejection, bounce and recovery.`);
