// Run a complete night using the production throttle, brakes, steering and bay actions.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const url=pathToFileURL(resolve(process.argv[2]));
const {loadCircuit}=await import(url);
const circuit=await loadCircuit(await readFile(new URL('circuit.wasm',url)));
const desk=circuit.courier;
const roadAt=s=>desk.roads.find(r=>s>=r.start && s<=r.start+r.length);
const dt=1/60;
let driven=0, crashes=0;
const visited=new Set();
function rideTo(id) {
  let direction, previous=-1, crossing=false;
  for(let tick=0;tick<60*950;tick++){
    const p=circuit.ride(),road=roadAt(p.s);
    assert(road,'Bike left its road');
    if(desk.nearby()===id){assert.equal(desk.routeTo(id).meters,0);return;}
    visited.add(road.id);
    if(p.crashAge>=0){if(!crossing)crashes++;crossing=true;circuit.drive(0,0,0,false,dt);continue;}
    crossing=false;
    if(!direction || previous!==road.id || tick%20===0)direction=desk.routeTo(id);
    previous=road.id;
    let remaining=desk.stops[id].s-p.s;
    if(road.id!==roadAt(desk.stops[id].s).id)remaining=Infinity;
    if(road.id===0 && remaining < -30) remaining+=road.length;
    const atBay=remaining>=-30 && remaining<120;
    const turn=direction.nextRoad>0?desk.roads[direction.nextRoad]:null;
    const entering=turn && roadAt(turn.entry).id===road.id && turn.entry>=p.s && turn.entry-p.s<100;
    const lane=atBay?3.3:entering?1.8:0;
    const f=circuit.frame(p.s);
    const limit=Math.min(31,Math.sqrt(3.2/Math.max(.0001,Math.abs(f.curvature))));
    const target=atBay?Math.min(limit,Math.sqrt(11*Math.max(0,remaining+4))):limit;
    const rate=Math.min(21/(p.v+2),p.v*.15,1.12);
    const steer=(f.curvature*p.v-2.8*p.headingError-.24*(p.lateral-lane))/Math.max(.04,rate);
    circuit.drive(p.v<target-.1?1:0,p.v>target+.15?.85:0,steer,false,dt);
    driven+=p.v*dt;
  }
  throw Error('Could not reach '+desk.stops[id].name+' '+JSON.stringify(circuit.ride()));
}
assert.equal(desk.nearby(),0);
assert(desk.accept(0)&&desk.accept(1)&&!desk.accept(2));
assert(desk.handoff(0)&&desk.handoff(1));
assert(!desk.handoff(0));
rideTo(2);assert(desk.handoff(0));
rideTo(4);assert(desk.handoff(1)&&desk.state().rampOpen);
const openRoute=()=>{const r=desk.roads[4];const p=circuit.ride(),save=desk.state();
  circuit.restore({...p,s:r.entry-100});const nav=desk.routeTo(4);assert.equal(nav.nextRoad,4);
  circuit.restore(p);desk.restore(save);};
openRoute();
for(const id of [6,2,7,4,5,3]){
  const j=desk.jobs[id];assert(desk.accept(id));rideTo(j.from);assert(desk.handoff(id));
  rideTo(j.to);assert(desk.handoff(id));
}
assert(desk.state().finished);assert(visited.has(4),'service ramp was driven');
const save=desk.state();desk.restore(save);assert.deepEqual(desk.state(),save);
assert(desk.nextShift() && desk.state().number===2 && desk.state().rampOpen);
assert.equal(desk.jobs[0].parcel,'Turbine brushes');
for(const night of [2,3]){
  assert.equal(desk.state().number,night);
  for(const [a,b] of [[0,1],[2,7],[3,6],[4,5]]){
    assert(desk.accept(a)&&desk.accept(b));
    for(const id of [a,b]){
      const j=desk.jobs[id];rideTo(j.from);assert(desk.handoff(id));
    }
    for(const id of [a,b]){
      const j=desk.jobs[id];rideTo(j.to);assert(desk.handoff(id));
    }
  }
  assert(desk.state().finished);assert(desk.nextShift());
}
assert.equal(desk.state().number,4);
assert.equal(desk.state().visits.reduce((a,b)=>a+b,0),24);
console.log('All 24 campaign deliveries driven, six bays, five roads, two panniers, shift rollover and persistent client access.',
            {km:(driven/1000).toFixed(1),crashes});

// A hard rail strike leaves both the urgent film reel and the ordinary parcel
// aboard the parked bike. No scene fixture or direct velocity edit is needed.
circuit.restore(null);desk.restore(null);
for(const id of [0,1]){assert(desk.accept(id));assert(desk.handoff(id));}
for(let i=0;i<1200&&!circuit.ride().foot.active;i++)circuit.drive(1,0,i>135?1:0,true,dt);
const fall=circuit.ride();
assert(fall.foot.active && fall.foot.distance>8 && !circuit.remount());
assert.equal(desk.nearby(),-1);
assert.deepEqual(desk.state().status.slice(0,2),[2,2]);
const onFootSave=desk.state();circuit.restore(fall);desk.restore(onFootSave);
assert.deepEqual(circuit.ride().bikeFall,fall.bikeFall);
const parked=circuit.frame(fall.s,fall.lateral);
for(let tick=0;tick<1400 && !circuit.ride().foot.canMount;tick++){
  const p=circuit.ride(),foot=circuit.frame(p.foot.s,p.foot.lateral);
  const aim=Math.atan2(parked.z-foot.z,parked.x-foot.x);
  circuit.walk(1,0,aim-p.foot.yaw,dt);
  assert.equal(circuit.ride().s,fall.s);
}
assert(circuit.ride().elapsed>fall.elapsed);
assert(circuit.remount());assert(!circuit.ride().foot.active&&!circuit.remount());
assert.deepEqual(desk.state(),onFootSave);
circuit.drive(1,0,0,false,dt);assert(circuit.ride().v>0);
console.log('Hard crash, saved standing position, walking, range-gated remount and continued delivery passed.',
            {walkDistance:fall.foot.distance.toFixed(1)});
