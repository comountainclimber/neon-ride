// node compare_circuit.mjs <circuit.wasm> <native probe executable>
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';

const [, , wasmPath, executable] = process.argv;
const reference = execFileSync(executable, { encoding: 'utf8' }).trim().split(/\r?\n/);
const { instance } = await WebAssembly.instantiate(readFileSync(wasmPath), {
  env: { atan: Math.atan, atan2: Math.atan2, sin: Math.sin, cos: Math.cos,
    exp: Math.exp, fmod: (a, b) => a % b }
});
const w = instance.exports;
const mem = (ptr, size) => new Float64Array(w.memory.buffer, ptr, size);
const length = w.loop_length();
assert(length > 4000);
const start = [...mem(w.course_frame(0, 2), 11)];
const end = [...mem(w.course_frame(length, 2), 11)];
assert.deepEqual(start, end);
let rows = 0;
for (let scenario = 0; scenario < 3; ++scenario) {
  const ride = mem(w.reset_ride(), 19);
  const truck = mem(w.traffic_car(0, 0), 6);
  ride[1] = scenario === 0 ? 0 : truck[1];
  if (scenario > 0) {
    ride[0] = truck[0] - truck[3] - 2;
    ride[2] = truck[2] + (scenario === 1 ? 28 : 4);
  }
  let crash = false, contact = false, drained = false, recharged = false, recovered = false;
  for (let tick = 0; tick < (scenario === 0 ? 33000 : 500); ++tick) {
    w.drive(1, 0, 0, Number(tick < 600), 1 / 120);
    contact ||= ride[13] > 0;
    crash ||= ride[15] >= 0;
    recovered ||= crash && ride[15] < 0 && ride[2] < 1 && ride[1] * truck[1] < 0;
    drained ||= ride[6] === 0;
    recharged ||= drained && ride[6] === 1;
    if (tick % 30 !== 0) continue;
    const pose = mem(w.crash_pose(), 9);
    const result = [scenario, tick, ride[0], ride[2], ride[6], ride[10], ride[15],
      ride[13], pose[0], pose[2], ride[1]];
    const expected = reference[rows++].split(/\s+/).map(Number);
    result.forEach((n, i) => assert(Math.abs(expected[i] - n) <= 1e-6,
      `Scenario ${scenario}, tick ${tick}, column ${i}: native ${expected[i]}, WASM ${n}`));
  }
  if (scenario === 0) assert(ride[10] >= 2 && drained && recharged && !crash);
  if (scenario === 1) assert(crash && recovered);
  if (scenario === 2) assert(contact && !crash);
}
assert.equal(rows, reference.length);
console.log(`${length.toFixed(1)} m circuit; ${rows} native/WASM samples match. Lap seam, traffic, boost, ejection and recovery passed.`);
