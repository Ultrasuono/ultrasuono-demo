import {height,landClimate} from './terrain.mjs';
import {buildTerraces} from './terraces.mjs';
import {biomeDetails} from './biome-style.mjs';
export const TILE_SIZE=3,TILE_RESOLUTION=24,TERRACE=.06;
export const groundY=h=>h<0?-.047:Math.floor(h/TERRACE)*TERRACE*.85;
export function tileKeys(x,z,radius=1){const cx=Math.floor(x/TILE_SIZE),cz=Math.floor(z/TILE_SIZE),keys=[];for(let j=-radius;j<=radius;j++)for(let i=-radius;i<=radius;i++)keys.push(`${cx+i},${cz+j}`);return keys.sort((a,b)=>{const [ax,az]=a.split(',').map(Number),[bx,bz]=b.split(',').map(Number);return (ax-cx)**2+(az-cz)**2-(bx-cx)**2-(bz-cz)**2;});}
export function makeTile(tx,tz){
 const minX=tx*TILE_SIZE,minZ=tz*TILE_SIZE,step=TILE_SIZE/TILE_RESOLUTION;
 const grid=Array.from({length:TILE_RESOLUTION+1},(_,j)=>Array.from({length:TILE_RESOLUTION+1},(_,i)=>height(minX+i*step,minZ+j*step)));
 const mesh=buildTerraces(grid,TILE_SIZE/2,TERRACE,minX+TILE_SIZE/2,minZ+TILE_SIZE/2);
 const forest=[],rocks=[],cacti=[],details=[],ascii=[],glyphs=[];
 const segment=(list,a,b)=>list.push(...a,...b);
 const plantSpacing=.18;
 for(let j=Math.ceil(minZ/plantSpacing);j<Math.ceil((minZ+TILE_SIZE)/plantSpacing);j++)for(let i=Math.ceil(minX/plantSpacing);i<Math.ceil((minX+TILE_SIZE)/plantSpacing);i++){
  const seed=Math.abs(Math.sin(i*127.1+j*311.7)),x=i*plantSpacing+Math.sin(j*12.7)*.045,z=j*plantSpacing+Math.cos(i*7.3)*.045,h=height(x,z);if(h<.025)continue;
  const {biome,zone}=landClimate(x,z,h);
  const cluster=Math.sin(x*2.3+Math.cos(z*1.7))+Math.cos(z*2.1-x*.5),y=groundY(h)+.012;
  const marks=biomeDetails(zone,x,y,z,seed);details.push(...marks.ground);
  if(zone==='hot-wet'&&h<1.1){forest.push(...marks.plants);}
  else if(zone==='cold-dry'){details.push(...marks.plants);}
  else if((zone==='cold-wet'&&seed>.4&&h<1.4)||(h<.72&&((biome==='forest'&&seed>.26)||(biome==='grassland'&&cluster>.85&&seed>.9)))){const tall=.12+seed*.12;segment(forest,[x,y,z],[x,y+tall,z]);
   for(let tier=0;tier<5;tier++){const up=tall*(.3+tier*.13),width=(1-tier/6)*tall*.29;for(const [dx,dz] of [[1,0],[-1,0],[0,1],[0,-1]]){segment(forest,[x,y+up+tall*.15,z],[x+dx*width,y+up-tall*.06,z+dz*width]);segment(forest,[x+dx*width,y+up-tall*.06,z+dz*width],[x,y+up,z]);}}
  }else if(biome==='desert'&&seed>.63&&Math.abs(Math.sin(i*73.9+j*191.3))<.1564){
   const tall=.13+seed*.11,r=.012;
   const stroke=points=>{for(let k=1;k<points.length;k++)segment(cacti,[x+points[k-1][0],y+points[k-1][1],z],[x+points[k][0],y+points[k][1],z]);};
   stroke([[-r,0],[-r,tall-r],[-r*.6,tall],[r*.6,tall],[r,tall-r],[r,0]]);
   stroke([[-r,tall*.42],[-tall*.24,tall*.42],[-tall*.3,tall*.48],[-tall*.3,tall*.76],[-tall*.23,tall*.76],[-tall*.23,tall*.53],[-r,tall*.53]]);
   stroke([[r,tall*.63],[tall*.24,tall*.63],[tall*.3,tall*.69],[tall*.3,tall*.93],[tall*.23,tall*.93],[tall*.23,tall*.74],[r,tall*.74]]);
  }else if(seed>.975){const r=.025+seed*.015,top=y+r*.7,a=[x-r,y,z-r*.6],b=[x+r,y,z-r*.5],c=[x+r*.8,y,z+r*.6],d=[x-r*.7,y,z+r*.7],e=[x-r*.4,top,z-r*.2],f=[x+r*.5,top,z+r*.1];for(const [u,v] of [[a,b],[b,c],[c,d],[d,a],[a,e],[b,f],[c,f],[d,e],[e,f]])segment(rocks,u,v);}
 }
 for(let j=Math.ceil(minZ/.15);j<Math.ceil((minZ+TILE_SIZE)/.15);j++)for(let i=Math.ceil(minX/.15);i<Math.ceil((minX+TILE_SIZE)/.15);i++){
  const x=i*.15,z=j*.15,h=height(x,z),patch=Math.sin(x*2.3+Math.cos(z*1.7))+Math.cos(z*2.1-x*.5),random=(Math.sin(i*127.1+j*311.7)*43758.5453)%1;
  if(patch<.55||Math.abs(random)>.68||h>.8)continue;
  const {biome,zone}=landClimate(x,z,h);
  ascii.push(x,groundY(h)+.025,z);glyphs.push(h<0?0:zone==='cold-dry'?4:zone==='cold-wet'?1:zone==='hot-wet'?2:zone==='hot-dry'?3:h>.48?1:biome==='forest'?2:4);
 }
 return Object.fromEntries(Object.entries({...mesh,forest,rocks,cacti,details,ascii,glyphs}).map(([key,data])=>[key,new Float32Array(data)]));
}
