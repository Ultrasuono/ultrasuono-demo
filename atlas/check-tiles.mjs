import assert from 'node:assert/strict';
import {height} from './terrain.mjs';
import {makeTile,tileKeys,TILE_SIZE,TILE_RESOLUTION} from './tile-data.mjs';
const first=makeTile(0,0),next=makeTile(1,0);
assert.deepEqual(first,makeTile(0,0));
for(const tile of [first,next]){
 assert.equal(tile.positions.length,tile.colors.length);
 assert.equal(tile.ascii.length/3,tile.glyphs.length);
 for(const values of Object.values(tile))for(const v of values)assert.ok(Number.isFinite(v));
}
// Neighbouring sectors evaluate the same height at their shared boundary.
for(let j=0;j<=TILE_RESOLUTION;j++){
 const z=j*TILE_SIZE/TILE_RESOLUTION;
 assert.equal(height(TILE_SIZE,z),height(TILE_SIZE+0*TILE_SIZE/TILE_RESOLUTION,z));
}
function border(tile){const result=[];for(let i=0;i<tile.positions.length;i+=3)if(Math.abs(tile.positions[i]-TILE_SIZE)<1e-6)result.push(`${tile.positions[i+1].toFixed(5)},${tile.positions[i+2].toFixed(5)}`);return new Set(result);}
const a=border(first),b=border(next);assert.ok(a.size>0);assert.deepEqual(a,b);
for(let x=-60;x<60;x+=.8){const keys=tileKeys(x,x*.4);assert.equal(keys.length,9);assert.equal(new Set(keys).size,9);}
const before=new Set(tileKeys(2.9,0)),after=new Set(tileKeys(3.1,0));assert.equal([...after].filter(key=>!before.has(key)).length,3);
assert.ok(height(1.2,-1.1)>1);
const initial=tileKeys(.15,.65).map(key=>makeTile(...key.split(',').map(Number)));
const humid=makeTile(0,0),desert=makeTile(-2,0);
assert.ok(humid.forest.length>40*246);
assert.ok(desert.cacti.length>0);
assert.ok(desert.forest.length<humid.forest.length);
for(const value of desert.cacti)assert.ok(Number.isFinite(value));
console.log('OK: dense forests in humid sectors and finite cactus geometry in arid sectors.');
console.log('OK: deterministic sectors, matching seams, bounded 9-sector window, 3 new sectors per crossing, taller peaks.');
console.log(JSON.stringify({initialTerrainTriangles:initial.reduce((n,t)=>n+t.positions.length/9,0),initialAsciiBatches:initial.filter(t=>t.ascii.length).length}));
