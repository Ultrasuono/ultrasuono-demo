import assert from 'node:assert/strict';
import {buildTerraces} from '../src/terraces.mjs';
const {positions,colors,outlines}=buildTerraces([[-.05,.15],[-.05,.15]],1,.1);
assert.equal(colors.length,positions.length);assert.ok(positions.length>0&&outlines.length>0);
assert.ok(colors.every(value=>value===0));
for(const x of [...positions,...outlines])assert.ok(Number.isFinite(x));
for(let i=0;i<outlines.length;i+=3){
 const [x,y]=outlines.slice(i,i+2);
 const level=Math.round((y-.002)/.085)*.1;
 // On this test slope h(x)=.05+.1*x, every contour must lie on its exact boundary.
 assert.ok(Math.abs(x-(level-.05)/.1)<1e-9);
 assert.ok(positions.some((v,j)=>j%3===0&&Math.abs(v-x)<1e-9&&Math.abs(positions[j+1]-(y-.002))<1e-9));
}
assert.equal(buildTerraces([[-1,-1],[-1,-1]],1,.1).positions.length,0);
console.log('OK: tops, walls and outlines share exact contour intersections; submerged cells omitted.');
