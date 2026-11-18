// The same sampled edges drive mesh clipping, rail openings and roadside clearances.
export function roadNetwork(circuit) {
  const roads=circuit.courier?.roads ?? [{id:0,start:0,length:circuit.length,name:'Midnight Circuit'}];
  const stops=circuit.courier?.stops ?? [];
  const segments=[],grid=new Map(),cellSize=20;
  const key=(x,z)=>x+','+z;
  function frame(road,d,l=0) {
    const end=Math.max(0,road.length-.00001);
    return circuit.frame(road.start+Math.min(end,Math.max(0,d)),l);
  }
  for(const road of roads) {
    const count=Math.ceil(road.length/3);
    let a=frame(road,0);
    for(let i=1;i<=count;i++) {
      const b=road.id===0&&i===count?frame(road,0):frame(road,i*road.length/count);
      const segment={road:road.id,a,b,dx:b.x-a.x,dz:b.z-a.z};
      segment.len2=Math.max(.000001,segment.dx**2+segment.dz**2);
      segments.push(segment);
      for(let x=Math.floor((Math.min(a.x,b.x)-12)/cellSize);x<=Math.floor((Math.max(a.x,b.x)+12)/cellSize);x++)
        for(let z=Math.floor((Math.min(a.z,b.z)-12)/cellSize);z<=Math.floor((Math.max(a.z,b.z)+12)/cellSize);z++) {
          const k=key(x,z);if(!grid.has(k))grid.set(k,[]);grid.get(k).push(segment);
        }
      a=b;
    }
  }
  function distance(segment,x,z){
    const dx=x-segment.a.x,dz=z-segment.a.z;
    const t=Math.max(0,Math.min(1,(dx*segment.dx+dz*segment.dz)/segment.len2));
    return {dx:dx-t*segment.dx,dz:dz-t*segment.dz,h:segment.a.h+t*(segment.b.h-segment.a.h)};
  }
  function adjacent(f,road,earlier=false){
    let best=144;
    for(const seg of grid.get(key(Math.floor(f.x/cellSize),Math.floor(f.z/cellSize)))||[]){
      if(seg.road===road || (earlier && seg.road>=road))continue;
      const d=distance(seg,f.x,f.z);
      if(Math.abs(d.h-f.h)<1.85)best=Math.min(best,d.dx*d.dx+d.dz*d.dz);
    }
    return Math.sqrt(best);
  }
  function stopOpening(road,s,side){
    if(side<0)return false;
    return stops.some(stop=>{
      if(stop.s<road.start-.001||stop.s>road.start+road.length+.001)return false;
      let diff=s-stop.s;
      if(road.id===0)diff-=Math.round(diff/road.length)*road.length;
      return diff>=-15&&diff<=15;
    });
  }
  return {roads,stops,segments,distance,frame,adjacent,stopOpening};
}
