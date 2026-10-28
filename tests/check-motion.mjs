import assert from 'node:assert/strict';
import {smoothElevation} from '../src/motion.mjs';
for(const [from,to] of [[0,2],[2,-1]]){
 let y=from;
 for(let i=0;i<60;i++){const next=smoothElevation(y,to,1/60);assert.ok(next>=Math.min(y,to)&&next<=Math.max(y,to));y=next;}
 assert.ok(Math.abs(y-to)<.01);
 assert.ok(Math.abs(smoothElevation(from,to,1/60)-from)<Math.abs(to-from)*.12);
}
let a=0,b=0;for(let i=0;i<30;i++)a=smoothElevation(a,1,1/30);for(let i=0;i<60;i++)b=smoothElevation(b,1,1/60);
assert.ok(Math.abs(a-b)<1e-12);assert.equal(smoothElevation(2,4,0),2);
console.log('OK: gradual ascent/descent, no overshoot, same timing at 30 and 60 fps.');
