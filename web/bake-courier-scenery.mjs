// node bake-courier-scenery.mjs [prepared original GLB directory]
import {readFileSync,writeFileSync} from 'node:fs';
import {dirname,join,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {loadCircuit} from './circuit.js';
import {roadNetwork} from './courier-roads.js';
const dir=dirname(fileURLToPath(import.meta.url));
const art=resolve(process.argv[2]??join(dir,'../NeonRide/SourceArt/Models'));
const source=JSON.parse(readFileSync(join(dir,'scenery.json')));
const c=await loadCircuit(readFileSync(join(dir,'circuit.wasm'))),network=roadNetwork(c);
const roads=network.roads.filter(r=>r.id>0);
if(roads.length!==4)throw Error('The shared courier simulation must be built first.');
const footprint=new Map();
function propRadius(name,size){
  if(!footprint.has(name)){
    const b=readFileSync(join(art,name+'.glb')),g=JSON.parse(b.subarray(20,20+b.readUInt32LE(12)));
    const a=g.accessors[g.meshes[0].primitives[0].attributes.POSITION];
    const extent=Math.max(...a.max.map((v,i)=>v-a.min[i]));
    footprint.set(name,Math.hypot(Math.max(-a.min[0],a.max[0]),Math.max(-a.min[2],a.max[2]))/extent+.018);
  }
  return footprint.get(name)*size;
}
const addresses=network.stops.map(stop=>c.frame(stop.s,12));
function crowdingBay(x,y,r,minH,maxH){
  return addresses.some(f=>{
    if(f.h>maxH+1||f.h+10<minH)return false;
    const dx=x-f.x,dy=y-f.z,along=Math.abs(dx*f.fx+dy*f.fz),across=Math.abs(dx*f.rx+dy*f.rz);
    return Math.hypot(Math.max(0,along-16),Math.max(0,across-5.9))<r+1;
  });
}
function clearance(x,y,r,minH,maxH,additionalOnly=false){
  let limit=Infinity;
  for(const seg of network.segments){
    if(additionalOnly&&seg.road===0)continue;
    const d=network.distance(seg,x,y);
    if(d.h+3<minH||d.h-2>maxH)continue;
    limit=Math.min(limit,Math.hypot(d.dx,d.dz)-r-6.55);
  }
  return limit;
}
const removedItems=[],removedInfill=[],items=[],infill=[],occupied=[];
function keep(x,y,z,height,r,old){
  const cl=clearance(x,y,r,z,z+height,old);
  if(cl<1.7||crowdingBay(x,y,r,z,z+height))return false;
  return true;
}
source.items.forEach(([name,x,y,z,size],i)=>{
  const r=propRadius(name,size);if(!keep(x,y,z,size,r,true))removedItems.push(i);
  else occupied.push([x,y,r,z,z+size]);
});
source.infill.forEach(([x,y,z,w,d,h],i)=>{
  const r=Math.hypot(w,d)/2;if(!keep(x,y,z,h,r,true))removedInfill.push(i);
  else occupied.push([x,y,r,z,z+h]);
});
function place(x,y,z,h,r){
  if(!keep(x,y,z,h,r,false))return false;
  if(occupied.some(([ox,oy,or,lo,hi])=>z+h>lo&&z<hi&&Math.hypot(x-ox,y-oy)<r+or+1.25))return false;
  occupied.push([x,y,r,z,z+h]);return true;
}
let seed=45683;
const random=()=>((seed=(Math.imul(seed,1664525)+1013904223)>>>0)/4294967296);
const round=values=>values.map(v=>typeof v==='number'?Math.round(v*1000)/1000:v);
for(const road of roads){
  for(let d=22,i=0;d<road.length-18;d+=road.id===4?45:32,i++){
    for(const side of [-1,1]){
      const f=network.frame(road,d,side*12.0),key=road.id===3||i%7<3?'lamp-a':'lamp-b',size=road.id===3?8.3:6.4;
      const r=propRadius(key,size);
      if(place(f.x,f.z,f.h-.1,size,r))items.push(round([key,f.x,f.z,f.h-.1,size,f.yaw+(side>0?Math.PI:0)]));
    }
    if(i%5===2&&road.id!==4){
      const side=i%2?1:-1,f=network.frame(road,d,side*(17+random()*5)),key=['sign-a','sign-b','sign-c'][i%3],size=8.3+random()*3.2;
      if(place(f.x,f.z,f.h,size,propRadius(key,size)))items.push(round([key,f.x,f.z,f.h,size,f.yaw+Math.PI/2]));
    }
  }
  for(let d=45,i=0;d<road.length-45;i++,d+=37+random()*22){
    for(const side of [-1,1]){
      const f=network.frame(road,d),near=road.id!==4&&i%3!==2;
      const w=near?13+random()*12:22+random()*20,depth=12+random()*13;
      const offset=side*((near?27:63)+random()*(near?22:72));
      const x=f.x+f.rx*offset,y=f.z+f.rz*offset,base=-85;
      const h=f.h-base+6+random()*(road.id===1?30:road.id===2?22:58);
      if(place(x,y,base,h,Math.hypot(w,depth)/2))infill.push(round([x,y,base,w,depth,h,f.yaw,i+road.id*250+(side>0?1:0),Math.floor(random()*4)]));
      if(i%2===0 && road.id!==4){
        const off=side*(85+random()*135);
        const x=f.x+f.rx*off,y=f.z+f.rz*off,key=['tower-a','tower-b','tower-c'][(i+road.id)%3],size=151+random()*154;
        if(place(x,y,base,size,propRadius(key,size)))items.push(round([key,x,y,base,size,random()*6]));
      }
    }
  }
}
const output={version:1,removedItems,removedInfill,items,infill};
writeFileSync(join(dir,'courier-scenery.json'),JSON.stringify(output)+'\n');
console.log({removedItems:removedItems.length,removedInfill:removedInfill.length,items:items.length,infill:infill.length});
