// Dispatch, road topology and delivery eligibility are owned by the shared native simulation.
export function courierAdapter(wasm) {
  const decoder = new TextDecoder();
  function label(address) {
    const bytes = new Uint8Array(wasm.memory.buffer);
    return decoder.decode(bytes.subarray(address, bytes.indexOf(0,address)));
  }
  const roads = Array.from({length:5}, (_,id) => ({
    id, name:label(wasm.road_name(id)), start:wasm.road_info(id,0), length:wasm.road_info(id,1),
    entry:wasm.road_info(id,2), exit:wasm.road_info(id,3)
  }));
  const stops = Array.from({length:6}, (_,id) => ({
    id, name:label(wasm.stop_label(id,0)), client:label(wasm.stop_label(id,1)),
    district:label(wasm.stop_label(id,2)), s:wasm.stop_position(id)
  }));
  const ptr = wasm.shift_ptr();
  const ints = new Int32Array(wasm.memory.buffer,ptr,16);
  const pickedAt = new Float64Array(wasm.memory.buffer,ptr+64,8);
  const onTime = new Int32Array(wasm.memory.buffer,ptr+128,8);
  let catalogNumber = -1, jobs;
  function state() {
    return {status:Array.from(ints.subarray(0,8)),visits:Array.from(ints.subarray(8,14)),
      number:ints[14],finished:!!ints[15],pickedAt:Array.from(pickedAt),onTime:Array.from(onTime),rampOpen:ints[12]>0};
  }
  return {
    roads,stops,state,
    get jobs() {
      if (catalogNumber!==ints[14]) {
        jobs=Array.from({length:8},(_,id)=>({id,parcel:label(wasm.job_label(id)),from:wasm.job_info(id,0),
          to:wasm.job_info(id,1),windowSeconds:wasm.job_info(id,2)}));
        catalogNumber=ints[14];
      }
      return jobs;
    },
    nearby:()=>wasm.shift_nearby(), accept:id=>!!wasm.shift_accept(id),
    handoff:id=>!!wasm.shift_handoff(id), nextShift:()=>!!wasm.shift_next(),
    routeTo(id) {
      const p=wasm.shift_route(id), buffer=wasm.memory.buffer;
      const [meters,turnIn]=new Float64Array(buffer,p,2);
      const steps=new Int32Array(buffer,p+16,12);
      return {meters,turnIn,nextRoad:steps[0],via:Array.from(steps.subarray(1,1+steps[11]))};
    },
    restore(saved) {
      wasm.shift_reset();
      if (!saved || !Array.isArray(saved.status) || saved.status.length!==8) return;
      function copy(source,target) {
        for(let i=0;i<target.length;i++)target[i]=Number.isFinite(source?.[i])?source[i]:0;
      }
      copy(saved.status,ints.subarray(0,8));copy(saved.visits,ints.subarray(8,14));
      copy(saved.pickedAt,pickedAt);copy(saved.onTime,onTime);
      ints[14]=Number.isInteger(saved.number)?saved.number:1;
      wasm.shift_validate();
    }
  };
}
