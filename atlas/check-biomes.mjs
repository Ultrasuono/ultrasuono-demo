import assert from 'node:assert/strict';
import {landClimate,effectTemperature,height} from './terrain.mjs';
assert.equal(effectTemperature(-20),-20);
assert.equal(effectTemperature(0),0);
assert.equal(effectTemperature(20),20);
assert.equal(effectTemperature(50),40);
for(let t=-20;t<50;t+=.1)assert.ok(effectTemperature(t+.1)>effectTemperature(t));
let hotDesert=false,buffer=false,forest=false;
for(let x=-9;x<=4;x+=.1){const c=landClimate(x,.65);
 if(c.biome==='desert'){assert.ok(c.humidity<=35);if(c.temperature>=49)hotDesert=true;}
 if(c.biome==='forest'){assert.ok(c.humidity>=72);forest=true;}
 if(c.biome==='grassland'&&c.humidity>35&&c.humidity<72)buffer=true;
}
assert.ok(hotDesert&&buffer&&forest);
let warmMountain=false,coldMountain=false,dryPrairie=false,tropicalForest=false;
for(let x=-40;x<=40;x+=.5)for(let z=-40;z<=40;z+=.5){
 const h=height(x,z),c=landClimate(x,z,h);
 if(c.biome==='forest'&&c.temperature>=32&&c.humidity>=80)tropicalForest=true;
 if(h>.72&&c.temperature>22)warmMountain=true;
 if(c.biome==='alpine'&&c.temperature<10)coldMountain=true;
 if(h>.025&&h<.55&&c.biome==='drygrassland'&&c.humidity<55)dryPrairie=true;
}
assert.ok(warmMountain&&coldMountain&&dryPrairie,'Include warm mountains, cold peaks and dry prairies');
assert.ok(tropicalForest,'Include hot, humid forests above 32°C and 80% humidity');
for(const [dx,dz] of [[-1,0],[1,0],[0,-1],[0,1]]){
 const types=new Set();
 for(let d=20;d<500;d+=.5){const c=landClimate(dx*d,dz*d);if(c.biome!=='water')types.add(c.biome);}
 assert.ok(types.has('desert')&&types.has('forest')&&types.has('grassland'));
}
for(const x of [-14,-6,6,14]){
 const a=landClimate(x,.65),b=landClimate(x+.001,.65);
 assert.ok(Math.abs(a.temperature-b.temperature)<.1&&Math.abs(a.humidity-b.humidity)<.1);
}
console.log('OK: all four travel directions alternate deserts, forests and transition biomes far from origin.');
console.log('OK: hot desert reaches 49–50°C, humid forests separated by transition belt, monotonic DSP remapping preserves ≤20°C.');
