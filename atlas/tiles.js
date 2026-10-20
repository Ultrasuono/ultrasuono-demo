import * as T from './three.module.js';
import {TILE_SIZE,tileKeys} from './tile-data.mjs';

export function createTiles(scene,terrainMaterial){
 const tiles=new Map(),pending=new Set(),wanted=new Set(),worker=new Worker(new URL('./tile-worker.js',import.meta.url),{type:'module'});
 const linesMaterial=new T.LineBasicMaterial({color:0xc4c6bf,transparent:true,opacity:.65});
 const detailMaterial=new T.LineBasicMaterial({color:0xc5c9c2,transparent:true,opacity:.66});
 const canvas=document.createElement('canvas');canvas.width=320;canvas.height=64;const ctx=canvas.getContext('2d');ctx.font='36px monospace';ctx.fillStyle='#fff';ctx.textAlign='center';ctx.textBaseline='middle';['~','^',':','.','+'].forEach((glyph,i)=>ctx.fillText(glyph,i*64+32,32));
 const asciiMaterial=new T.ShaderMaterial({transparent:true,depthWrite:false,uniforms:{atlas:{value:new T.CanvasTexture(canvas)},pixelSize:{value:12}},vertexShader:'attribute float glyph; varying float character; uniform float pixelSize; void main(){character=glyph;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);gl_PointSize=pixelSize;}',fragmentShader:'uniform sampler2D atlas; varying float character; void main(){vec2 uv=vec2((gl_PointCoord.x+character)/5.0,1.0-gl_PointCoord.y);float alpha=texture2D(atlas,uv).a*.45;if(alpha<.01)discard;gl_FragColor=vec4(vec3(.62),alpha);}'});
 function geometry(points){const g=new T.BufferGeometry();g.setAttribute('position',new T.BufferAttribute(points,3));return g;}
 function remove(key){const group=tiles.get(key);if(!group)return;group.traverse(o=>o.geometry?.dispose());scene.remove(group);tiles.delete(key);}
 worker.onmessage=({data:{key,tile,error}})=>{pending.delete(key);if(error){console.error('Terreno:',error);return;}if(!wanted.has(key))return;
  const group=new T.Group(),geo=geometry(tile.positions);geo.setAttribute('color',new T.BufferAttribute(tile.colors,3));geo.computeVertexNormals();group.add(new T.Mesh(geo,terrainMaterial));
  for(const [data,material] of [[tile.outlines,linesMaterial],[tile.forest,detailMaterial],[tile.rocks,detailMaterial],[tile.cacti,detailMaterial]])if(data.length)group.add(new T.LineSegments(geometry(data),material));
  if(tile.ascii.length){const g=geometry(tile.ascii);g.setAttribute('glyph',new T.BufferAttribute(tile.glyphs,1));group.add(new T.Points(g,asciiMaterial));}

  tiles.set(key,group);scene.add(group);
 };
 let last='';
 return {
  update(x,z,aspect,pixels){asciiMaterial.uniforms.pixelSize.value=.06*pixels/2.7;const radius=Math.max(1,Math.ceil(1.35*aspect/TILE_SIZE)),signature=`${Math.floor(x/TILE_SIZE)},${Math.floor(z/TILE_SIZE)},${radius}`;if(signature===last)return;last=signature;
   const keys=tileKeys(x,z,radius);wanted.clear();keys.forEach(key=>wanted.add(key));
   for(const key of [...tiles.keys()])if(!wanted.has(key))remove(key);
   for(const key of keys)if(!tiles.has(key)&&!pending.has(key)){const [tx,tz]=key.split(',').map(Number);pending.add(key);worker.postMessage({key,tx,tz});}
  },
  meshes(){return [...tiles.values()].map(group=>group.children[0]);},

  stats(){let triangles=0;for(const group of tiles.values())triangles+=group.children[0].geometry.attributes.position.count/3;return {tiles:tiles.size,pending:pending.size,triangles};},
  dispose(){worker.terminate();for(const key of [...tiles.keys()])remove(key);linesMaterial.dispose();detailMaterial.dispose();asciiMaterial.uniforms.atlas.value.dispose();asciiMaterial.dispose();}
 };
}
