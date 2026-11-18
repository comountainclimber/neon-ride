const rideKeys = 's lateral v sideV lean steer charge boost locked cool laps gear rpm impact elapsed crashAge crashSpeed crashSide crashStart headingError severity firstLanding'.split(' ');
const frameKeys = 'x z h fx fz rx rz yaw grade bank curvature'.split(' ');
const crashKeys = 'riderS riderLateral riderHeight riderPitch riderRoll riderYaw bikeRoll cameraDistance cameraHeight bikeS bikeLateral bikeHeight bikePitch bikeYaw riderGround bikeGround riderHit bikeHit riderSpeed bikeSpeed severity clipTime'.split(' ');

export async function loadCircuit(bytes) {
  if (!bytes) {
    const response = await fetch('./circuit.wasm');
    if (!response.ok) throw new Error('Could not load circuit simulation.');
    bytes = await response.arrayBuffer();
  }
  const {instance:{exports: wasm}} = await WebAssembly.instantiate(bytes, {env:{
    atan:Math.atan, atan2:Math.atan2, sin:Math.sin, cos:Math.cos, exp:Math.exp, fmod:(a,b)=>a%b
  }});
  const ptr = wasm.reset_ride(), length = wasm.loop_length();
  const courier = courierAdapter(wasm);
  const fields = (pointer, keys) => {
    const view = new Float64Array(wasm.memory.buffer, pointer, keys.length);
    return Object.fromEntries(keys.map((key,i)=>[key,view[i]]));
  };
  const ride = () => fields(ptr,rideKeys);
  return {
    length, courier, trafficCount:20, ride, frame:(s,l=0)=>fields(wasm.course_frame(s,l),frameKeys),
    car:(i,time)=>fields(wasm.traffic_car(i,time),['s','lane','v','half','width','truck']),
    crash:()=>fields(wasm.crash_pose(),crashKeys),
    drive(gas,brake,steer,nitro,dt) { wasm.drive(gas,brake,steer,Number(nitro),dt); },
    restore(data) {
      wasm.reset_ride();
      const road = data && courier.roads.find(r=>data.s>=r.start && data.s<=r.start+r.length);
      if (!road || !Number.isFinite(data.s)) return;
      const values = new Float64Array(wasm.memory.buffer,ptr,rideKeys.length);
      for (const [index,lo,hi] of [[0,road.start,road.start+road.length],[1,-6.12,6.12],[6,0,1],[10,0,1e9],[14,0,1e10],[19,-Math.PI,Math.PI]]) {
        const v=data[rideKeys[index]];
        if (Number.isFinite(v)) values[index]=Math.min(hi,Math.max(lo,v));
      }
    }
  };
}
import {courierAdapter} from './courier.js';
