import assert from 'node:assert/strict';
import {stations,weather} from './weather.mjs';
assert.equal(stations.length,500);
assert.equal(new Set(stations.map(s=>s.name)).size,500);
const time=Date.UTC(2026,9,12);
for(const s of stations)for(let hour=0;hour<48;hour++){
  const a=weather(s.id,time+hour*3600000),b=weather(s.id,time+hour*3600000+1000);
  assert.deepEqual(a,weather(s.id,time+hour*3600000));
  assert.ok(a.temperature>-40&&a.temperature<55);
  assert.ok(a.humidity>=5&&a.humidity<=100);
  assert.ok(a.light>=0&&a.light<=1);
  assert.ok(Math.abs(a.temperature-b.temperature)<.01);
  assert.ok(Math.abs(a.humidity-b.humidity)<.01);
  assert.ok(Math.abs(a.light-b.light)<.001);
}
assert.throws(()=>weather(500),RangeError);
console.log('OK: 500 ambienti, determinismo, intervalli e continuità su 48 ore.');
