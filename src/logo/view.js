import * as THREE from '../vendor/three.module.js';
import {climate} from './climate.mjs';
export function createClimateLogo(stage){
let active={temperature:20,humidity:50,light:1};
let paused=false,mesh,base,sharp,soft,positions,lastHumidity=NaN;
const scene=new THREE.Scene(),camera=new THREE.PerspectiveCamera(32,1,.1,100);
camera.position.set(0,0,20);
const renderer=new THREE.WebGLRenderer({antialias:true,alpha:true});
renderer.setClearColor(0x000000,0);renderer.setPixelRatio(Math.min(devicePixelRatio,2));stage.prepend(renderer.domElement);
const trailCanvas=document.createElement('canvas'),trailContext=trailCanvas.getContext('2d');
stage.prepend(trailCanvas);renderer.domElement.style.opacity='0';
let visual=climate(20,50);
const crystalUniforms={cold:{value:0},time:{value:0},glow:{value:0},center:{value:new THREE.Vector3()},radius:{value:1}};
renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=.85;
scene.add(new THREE.AmbientLight(0xffffff,.8));
const key=new THREE.DirectionalLight(0xffffff,1.6);key.position.set(-3,5,7);scene.add(key);
const fill=new THREE.DirectionalLight(0xffffff,.7);fill.position.set(3,-2,1);scene.add(fill);
function resize(){camera.aspect=stage.clientWidth/stage.clientHeight;camera.updateProjectionMatrix();renderer.setSize(stage.clientWidth,stage.clientHeight,false);trailCanvas.width=renderer.domElement.width;trailCanvas.height=renderer.domElement.height;}
new ResizeObserver(resize).observe(stage);
function change(){
  const h=active.humidity,t=active.temperature,l=active.light*100;
  visual=climate(t,h,l);
  if(!mesh)return;
  mesh.material.color.set(visual.color[0]).lerp(new THREE.Color(visual.color[1]),visual.color[2]);
  mesh.material.emissive.copy(mesh.material.color);
  crystalUniforms.glow.value=visual.glow;
  mesh.material.roughness=.72-.5*visual.frost;
  crystalUniforms.cold.value=visual.frost;
  if(!Number.isFinite(lastHumidity)||Math.abs(h-lastHumidity)>.03){
    const amount=Math.abs(h-50)/50,target=h<50?sharp:soft;
    for(let i=0;i<positions.length;i++)positions[i]=base[i]+(target[i]-base[i])*amount;
    mesh.geometry.attributes.position.needsUpdate=true;lastHumidity=h;
  }
  mesh.geometry.setDrawRange(0,Math.floor(mesh.geometry.index.count/3*visual.fullness)*3);
}
async function load(){
  const [metaResponse,dataResponse]=await Promise.all([fetch(new URL('../../assets/logo/logo.json',import.meta.url)),fetch(new URL('../../assets/logo/logo.bin',import.meta.url))]);
  if(!metaResponse.ok||!dataResponse.ok)throw Error('Geometria non disponibile');
  const meta=await metaResponse.json(),buffer=await dataResponse.arrayBuffer(),count=meta.vertices*3;
  if(buffer.byteLength!==(count*3+meta.triangles*3)*4)throw Error('Dimensioni del modello non valide');
  base=new Float32Array(buffer,0,count);sharp=new Float32Array(buffer,count*4,count);soft=new Float32Array(buffer,count*8,count);
  positions=new Float32Array(base);const indices=new Uint32Array(buffer,count*12,meta.triangles*3);
  // Reveal whole triangles in a deterministic shuffled order, rather than slicing vertices.
  let seed=42;for(let i=meta.triangles-1;i>0;i--){seed=(Math.imul(seed,1664525)+1013904223)>>>0;const j=seed%(i+1);for(let k=0;k<3;k++){const a=indices[i*3+k];indices[i*3+k]=indices[j*3+k];indices[j*3+k]=a;}}
  const geometry=new THREE.BufferGeometry();geometry.setAttribute('position',new THREE.BufferAttribute(positions,3));geometry.setIndex(new THREE.BufferAttribute(indices,1));
  geometry.computeBoundingBox();const box=geometry.boundingBox,center=box.getCenter(new THREE.Vector3()),size=box.getSize(new THREE.Vector3());
  crystalUniforms.center.value.copy(center);crystalUniforms.radius.value=Math.max(size.x,size.y,size.z)*.5;
  const material=new THREE.MeshStandardMaterial({color:0xeb7008,emissive:0xc54b00,emissiveIntensity:.13,roughness:.72,metalness:0,flatShading:true,side:THREE.DoubleSide});
  material.onBeforeCompile=shader=>{
    shader.uniforms.crystalCold=crystalUniforms.cold;shader.uniforms.crystalTime=crystalUniforms.time;
    shader.uniforms.coreGlow=crystalUniforms.glow;shader.uniforms.coreCenter=crystalUniforms.center;shader.uniforms.coreRadius=crystalUniforms.radius;
    shader.vertexShader='varying vec3 crystalPosition;\n'+shader.vertexShader;
    shader.vertexShader=shader.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\ncrystalPosition = position;');
    shader.fragmentShader='varying vec3 crystalPosition; uniform float crystalCold; uniform float crystalTime; uniform float coreGlow; uniform vec3 coreCenter; uniform float coreRadius;\n'+shader.fragmentShader;
    shader.fragmentShader=shader.fragmentShader.replace('#include <emissivemap_fragment>',`#include <emissivemap_fragment>
      vec3 crystalGrid = crystalPosition * 14.0;
      float crystalSeed = fract(sin(dot(floor(crystalGrid), vec3(127.1,311.7,74.7))) * 43758.5453);
      float crystalSpot = 1.0 - smoothstep(0.08,0.24,length(fract(crystalGrid)-0.5));
      float crystalPulse = pow(max(0.0,sin(crystalTime*1.8+crystalSeed*70.0)),18.0);
      totalEmissiveRadiance += vec3(0.65,0.9,1.0)*crystalCold*step(0.87,crystalSeed)*crystalSpot*crystalPulse*9.0;
      float coreDistance = length((crystalPosition-coreCenter).xy)/coreRadius;
      float coreFalloff = exp(-coreDistance*coreDistance*2.0);
      totalEmissiveRadiance += mix(diffuse,vec3(1.0),0.2)*coreGlow*coreFalloff*1.1;
    `);
  };
  mesh=new THREE.Mesh(geometry,material);
  const group=new THREE.Group();group.add(mesh);mesh.position.copy(center).negate();group.scale.setScalar(3.7/Math.max(size.x,size.y,size.z));scene.add(group);
  mesh.userData.group=group;
  change();stage.dataset.loaded='true';
}
let last=performance.now();function frame(now){if(now-last<1000/30){requestAnimationFrame(frame);return;}const dt=Math.min((now-last)/1000,.1);last=now;crystalUniforms.time.value=now/1000;if(mesh&&!paused){const speed=visual.speed;mesh.userData.group.rotation.y+=dt*speed;mesh.userData.group.rotation.x+=dt*speed*.32;mesh.userData.group.rotation.z+=dt*speed*.15;}renderer.render(scene,camera);
  trailContext.fillStyle=`rgba(0,0,0,${visual.trail?1-Math.exp(-dt/visual.trail):1})`;
  trailContext.globalCompositeOperation='destination-out';
  trailContext.fillRect(0,0,trailCanvas.width,trailCanvas.height);
  trailContext.globalCompositeOperation='source-over';
  if(visual.glow>0){
    trailContext.save();
    trailContext.globalCompositeOperation='lighter';
    trailContext.globalAlpha=visual.glow*.55;
    trailContext.filter=`blur(${(14+visual.glow*24)*renderer.getPixelRatio()}px)`;
    trailContext.drawImage(renderer.domElement,0,0);
    trailContext.globalAlpha=visual.glow*.4;
    trailContext.filter=`blur(${(5+visual.glow*7)*renderer.getPixelRatio()}px)`;
    trailContext.drawImage(renderer.domElement,0,0);
    trailContext.restore();
  }
  trailContext.drawImage(renderer.domElement,0,0);
  requestAnimationFrame(frame);}
load().catch(error=>{stage.textContent=`Logo non disponibile: ${error.message}`;});resize();requestAnimationFrame(frame);
return {update(values){active=values;change();}};
}
