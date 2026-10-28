import assert from 'node:assert/strict';
import {visualBiome} from './biome-style.mjs';
import {height,landClimate} from './terrain.mjs';
import {makeTile} from './tile-data.mjs';
const cases=[[-5,25,'cold-dry'],[-5,85,'cold-wet'],[35,85,'hot-wet'],[40,20,'hot-dry']];
for(const [t,h,zone] of cases)assert.equal(visualBiome(t,h),zone);
const locations=[[-80,-56,'cold-dry'],[-80,-54,'cold-wet'],[-80,-24,'hot-wet'],[-80,-23,'hot-dry']];
for(const [x,z,zone] of locations){const h=height(x,z);assert.ok(h>.025&&h<.45);assert.equal(landClimate(x,z,h).zone,zone);}
const tile=makeTile(0,0);assert.deepEqual(Object.keys(tile).sort(),['colors','outlines','positions']);
console.log('OK: four climate classifications, decoration-free terrain; all four climates also occur on low terrain.');
