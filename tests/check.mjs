import assert from 'node:assert/strict';
import {height,climateAt} from '../src/terrain.mjs';
let min=Infinity,max=-Infinity;
for(let x=-4;x<=4;x+=.1)for(let z=-4;z<=4;z+=.1){const h=height(x,z),c=climateAt(x,z);assert.ok(Number.isFinite(h));assert.deepEqual(c,climateAt(x,z));assert.ok(c.temperature>=-20&&c.temperature<=50&&c.humidity>=0&&c.humidity<=100&&c.light>=0&&c.light<=1);assert.ok(Math.abs(height(x+.001,z)-h)<.02);min=Math.min(min,h);max=Math.max(max,h);}
assert.ok(min<0&&max>.65);assert.ok(climateAt(1.2,-1.1).temperature<climateAt(-2,1).temperature);
let distantLand=0,distantMountains=0,distantLowlands=0;
for(let x=-60;x<=60;x++)for(let z=-60;z<=60;z++){
 if(Math.hypot(x,z)<14)continue;
 const h=height(x,z);
 if(h>0){distantLand++;if(h>.72)distantMountains++;else distantLowlands++;}
}
assert.ok(distantMountains/distantLand>.1,'Mountains must recur beyond the starting area');
assert.ok(distantLowlands/distantLand>.4,'Keep valleys and lowlands between mountain ranges');
const lightSamples=[];
for(let t=0;t<=120;t++){
 const a=climateAt(.15,.65,t),b=climateAt(.15,.65,t+.15);
 lightSamples.push(a.light);
 assert.ok(Math.abs(a.light-b.light)<.02);
 assert.equal(a.temperature,b.temperature);
 assert.equal(a.humidity,b.humidity);
}
assert.ok(Math.max(...lightSamples)-Math.min(...lightSamples)>.2);
let landSamples=0,fullSun=0,shade=0;
for(let x=-20;x<=20;x+=.5)for(let z=-20;z<=20;z+=.5){
 if(height(x,z)<.025)continue;
 const {light}=climateAt(x,z);landSamples++;
 if(light===1)fullSun++;
 if(light<.2)shade++;
}
assert.ok(fullSun/landSamples>.04,'Open terrain must regularly reach full sunlight');
assert.ok(shade/landSamples>.1,'Keep shaded and cloudy areas dark');
console.log('OK: moving clouds vary light at a fixed location, continuously, without changing temperature/humidity.');
console.log('OK: terrain determinism, continuity, sea/mountains and climate limits.');
