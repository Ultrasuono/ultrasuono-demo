import assert from 'node:assert/strict';
import {visualBiome,biomeDetails} from './biome-style.mjs';
import {height,landClimate} from './terrain.mjs';
import {makeTile} from './tile-data.mjs';
const cases=[[-5,25,'cold-dry'],[-5,85,'cold-wet'],[35,85,'hot-wet'],[40,20,'hot-dry']];
const shapes=[];
for(const [t,h,zone] of cases){
 assert.equal(visualBiome(t,h),zone);
 const detail=biomeDetails(zone,0,.2,0,.9);assert.deepEqual(detail,biomeDetails(zone,0,.2,0,.9));
 for(const data of Object.values(detail))for(const v of data)assert.ok(Number.isFinite(v));
 assert.ok(detail.plants.length+detail.ground.length>0);shapes.push(JSON.stringify(detail));
}
assert.equal(new Set(shapes).size,4);
const locations=[[-80,-56,'cold-dry'],[-80,-54,'cold-wet'],[-80,-24,'hot-wet'],[-80,-23,'hot-dry']];
for(const [x,z,zone] of locations){const h=height(x,z);assert.ok(h>.025&&h<.45);assert.equal(landClimate(x,z,h).zone,zone);}
const tile=makeTile(0,0);assert.ok(tile.details instanceof Float32Array);assert.ok(tile.forest.length>0);
console.log('OK: four distinct deterministic visual biomes; all four climates also occur on low terrain.');
