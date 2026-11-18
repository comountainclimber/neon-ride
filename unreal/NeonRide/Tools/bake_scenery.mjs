// node bake_scenery.mjs [web directory] [prepared film GLB directory]
// Reusing the source height and footprint keeps both renderers clear of every bend.
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const project = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const web = resolve(process.argv[2] ?? join(project, '../../web'));
const art = resolve(process.argv[3] ?? join(project, '../../models'));
const { instance } = await WebAssembly.instantiate(readFileSync(join(web, 'circuit.wasm')), {
  env: { sin: Math.sin, cos: Math.cos, exp: Math.exp, atan: Math.atan, atan2: Math.atan2, fmod: (x,y) => x%y }
});
const core = instance.exports;
const length = core.loop_length();
const frame = (s, offset=0) => [...new Float64Array(core.memory.buffer, core.course_frame(s, offset), 11)];
const segments = Math.ceil(length / 3);
const line = Array.from({length: segments}, (_, i) => frame(i * length / segments));

function clearance(x, y) {
  let squared = Infinity;
  for (let i = 0; i < line.length; i++) {
    const a = line[i], b = line[(i + 1) % line.length];
    const dx = b[0] - a[0], dy = b[1] - a[1];
    const t = Math.min(1, Math.max(0, ((x-a[0])*dx + (y-a[1])*dy) / (dx*dx + dy*dy)));
    squared = Math.min(squared, (x-a[0]-t*dx)**2 + (y-a[1]-t*dy)**2);
  }
  return Math.sqrt(squared) - 6.55;
}

const footprint = new Map();
function radiusPerMetre(name) {
  if (!footprint.has(name)) {
    const bytes = readFileSync(join(art, name + '.glb'));
    const doc = JSON.parse(bytes.subarray(20, 20 + bytes.readUInt32LE(12)));
    const bounds = doc.accessors[doc.meshes[0].primitives[0].attributes.POSITION];
    const extent = Math.max(...bounds.max.map((v,i) => v - bounds.min[i]));
    const radius = Math.hypot(Math.max(-bounds.min[0], bounds.max[0]), Math.max(-bounds.min[2], bounds.max[2]));
    footprint.set(name, radius / extent + .018);
  }
  return footprint.get(name);
}
let seed = 31482;
function random() {
  seed = (Math.imul(1664525, seed) + 1013904223) >>> 0;
  return seed / 4294967296;
}
const items = [];
let minimumClearance = Infinity;
function place(name, s, offset, height, size, yaw, elevated) {
  const radius = radiusPerMetre(name) * size;
  for (let attempt = 0; attempt < 12; attempt++) {
    const f = frame(s, offset);
    const gap = clearance(f[0], f[1]) - radius;
    if (gap >= 1.7) {
      minimumClearance = Math.min(minimumClearance, gap);
      const z = (elevated ? frame(s)[2] : -80) + height;
      items.push([name, ...[f[0], f[1], z, size, f[7] + yaw].map(v => Math.round(v*1000)/1000)]);
      return;
    }
    offset += Math.sign(offset) * Math.max(2, 2-gap);
  }
}
for (let s = 12, i = 0; s < length; s += 32, i++) {
  for (const side of [-1, 1]) {
    const name = i%17 < 9 ? 'lamp-a' : 'lamp-b';
    place(name, s, side*10.6, -.15, name === 'lamp-a' ? 7.2 : 6.5, side > 0 ? Math.PI : 0, true);
  }
}
for (let s = 110, i = 0; s < length; s += 111, i++) {
  const side = i%2 ? 1 : -1;
  place(['sign-a','sign-b','sign-c'][i%3], s, side*16, 0, 8+i%3*1.8, Math.PI*.5, true);
}
for (let s = 8, i = 0; s < length; s += 68, i++) {
  for (const side of [-1, 1]) {
    place(['tower-c','tower-a','tower-b'][i%3], s, side*(52+random()*118), 0, 133+random()*151, random()*6, false);
    if (i%2 === 0)
      place(['tower-a','tower-b','tower-c'][i%3], s, side*(270+random()*350), 0, 280+random()*210, random()*6, false);
    if (i%8 === 1)
      place(i%3 ? 'water-tower' : 'building-crown', s, side*39, -9, i%3 ? 11 : 25, random()*5, true);
  }
}
// Aircraft stay above the circuit's highest point, including their full body radius.
const aircraft = Array.from({length: 8}, (_, i) => ({
  model:['car','car-a','car-b','car-c'][i % 4], size:5 + i % 3,
  altitude:110 + 7*i, radius:310 + 60*i, phase:i*.98, rate:.016 + .001*i
}));
const airspace = {
  origin:[100,-560], aspect:.77, aircraft,
  jelly:{position:[760,-1090,106],size:25,amplitude:1.4,rate:.45}
};
// Midrises use the film tower silhouettes as exclusion volumes. Every office footprint
// is checked against the entire closed road, including the return side of a bend.
const infill = [];
const towerRadius = {'tower-a':.217, 'tower-b':.194, 'tower-c':.224};
const occupied = items.filter(([name]) => name in towerRadius)
  .map(([name,x,y,,size]) => [x, y, size*towerRadius[name]]);
function officeNoise(i) {
  const n = Math.sin(i*73.291 + 2.51) * 47393.27;
  return n - Math.floor(n);
}
for (let s = 16, i = 0; s < length; i++, s += 30 + 21*officeNoise(i+1)) {
  for (let j = 0; j < 4; j++) {
    const id = 8*i+j+1, side = j%2 ? 1 : -1, near = j<2;
    const offset = near ? 32 + officeNoise(id*14+4)*48 : 98 + officeNoise(id*42+6)*112;
    const [x,y,roadZ,,,,,yaw] = frame(s + (near ? 0 : 18), side*offset);
    const width = 12 + officeNoise(id*9+2)*21, depth = 11 + officeNoise(id*10+3)*16;
    const radius = Math.hypot(width,depth)/2;
    if (clearance(x,y)-radius < 7.05 || occupied.some(([ox,oy,or])=>Math.hypot(x-ox,y-oy)<radius+or+2)) continue;
    const base = -95, height = roadZ-base+(near?6:28)+(near?55:96)*officeNoise(id*81+113);
    infill.push([x,y,base,width,depth,height,yaw,id,Math.round(officeNoise(id*21+99)*3)]
      .map(v=>Math.round(v*1000)/1000));
    occupied.push([x,y,radius]);
  }
}
const output = JSON.stringify({version:2, units:'m', items, minimumClearance, airspace, infill}) + '\n';
for (const path of [join(project, 'Content/Circuit/scenery.json'), join(web, 'scenery.json')])
  writeFileSync(path, output);
console.log(`${items.length} objects, minimum road clearance ${minimumClearance.toFixed(2)} m`);
