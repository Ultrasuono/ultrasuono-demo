import * as T from './vendor/three.module.js';
import {TILE_SIZE,tileKeys} from './tile-data.mjs';

export function createTiles(scene,terrainMaterial){
 const tiles=new Map(),pending=new Set(),wanted=new Set(),worker=new Worker(new URL('./tile-worker.js',import.meta.url),{type:'module'});
 const linesMaterial=new T.LineBasicMaterial({color:0xc4c6bf,transparent:true,opacity:.65});
 function geometry(points){const g=new T.BufferGeometry();g.setAttribute('position',new T.BufferAttribute(points,3));return g;}
 function remove(key){const group=tiles.get(key);if(!group)return;group.traverse(o=>o.geometry?.dispose());scene.remove(group);tiles.delete(key);}
 worker.onmessage=({data:{key,tile,error}})=>{pending.delete(key);if(error){console.error('Terreno:',error);return;}if(!wanted.has(key))return;
  const group=new T.Group(),geo=geometry(tile.positions);geo.setAttribute('color',new T.BufferAttribute(tile.colors,3));geo.computeVertexNormals();group.add(new T.Mesh(geo,terrainMaterial));
  if(tile.outlines.length)group.add(new T.LineSegments(geometry(tile.outlines),linesMaterial));

  tiles.set(key,group);scene.add(group);
 };
 let last='';
 return {
  update(x,z,aspect,pixels){const radius=Math.max(1,Math.ceil(1.35*aspect/TILE_SIZE)),signature=`${Math.floor(x/TILE_SIZE)},${Math.floor(z/TILE_SIZE)},${radius}`;if(signature===last)return;last=signature;
   const keys=tileKeys(x,z,radius);wanted.clear();keys.forEach(key=>wanted.add(key));
   for(const key of [...tiles.keys()])if(!wanted.has(key))remove(key);
   for(const key of keys)if(!tiles.has(key)&&!pending.has(key)){const [tx,tz]=key.split(',').map(Number);pending.add(key);worker.postMessage({key,tx,tz});}
  },
  meshes(){return [...tiles.values()].map(group=>group.children[0]);},

  stats(){let triangles=0;for(const group of tiles.values())triangles+=group.children[0].geometry.attributes.position.count/3;return {tiles:tiles.size,pending:pending.size,triangles};},
  dispose(){worker.terminate();for(const key of [...tiles.keys()])remove(key);linesMaterial.dispose();}
 };
}
