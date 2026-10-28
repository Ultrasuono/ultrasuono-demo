import {height} from './terrain.mjs';
import {buildTerraces} from './terraces.mjs';
export const TILE_SIZE=3,TILE_RESOLUTION=24,TERRACE=.06;
export const groundY=h=>h<0?-.047:Math.floor(h/TERRACE)*TERRACE*.85;
export function tileKeys(x,z,radius=1){const cx=Math.floor(x/TILE_SIZE),cz=Math.floor(z/TILE_SIZE),keys=[];for(let j=-radius;j<=radius;j++)for(let i=-radius;i<=radius;i++)keys.push(`${cx+i},${cz+j}`);return keys.sort((a,b)=>{const [ax,az]=a.split(',').map(Number),[bx,bz]=b.split(',').map(Number);return (ax-cx)**2+(az-cz)**2-(bx-cx)**2-(bz-cz)**2;});}
export function makeTile(tx,tz){
 const minX=tx*TILE_SIZE,minZ=tz*TILE_SIZE,step=TILE_SIZE/TILE_RESOLUTION;
 const grid=Array.from({length:TILE_RESOLUTION+1},(_,j)=>Array.from({length:TILE_RESOLUTION+1},(_,i)=>height(minX+i*step,minZ+j*step)));
 const mesh=buildTerraces(grid,TILE_SIZE/2,TERRACE,minX+TILE_SIZE/2,minZ+TILE_SIZE/2);
 return Object.fromEntries(Object.entries(mesh).map(([key,data])=>[key,new Float32Array(data)]));
}
