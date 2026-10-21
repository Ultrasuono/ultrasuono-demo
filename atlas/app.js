import {createRecording} from './recording.js';
import * as T from './three.module.js';
import {height,climateAt,effectTemperature} from './terrain.mjs';
import {createTiles} from './tiles.js';
import {groundY} from './tile-data.mjs';
import {createClimateLogo} from './logo/view.js';
import {climate as logoClimate} from './logo/climate.mjs';
const $=id=>document.getElementById(id),host=$('map');
function updateMixTrack(){const slider=$('mix');slider.style.setProperty('--fill',`${slider.value}%`);}
$('mix').addEventListener('input',updateMixTrack);updateMixTrack();

const climateHeading=document.querySelector('h1');
function fitClimateHeading(){
 if(!climateHeading.clientWidth)return;
 climateHeading.style.fontSize='20px';
 const range=document.createRange();range.selectNodeContents(climateHeading);
 const width=range.getBoundingClientRect().width;
 if(width)climateHeading.style.fontSize=`${20*climateHeading.clientWidth/width}px`;
}
new ResizeObserver(fitClimateHeading).observe(document.querySelector('aside'));
document.fonts.ready.then(fitClimateHeading);

const scene=new T.Scene();scene.background=new T.Color('#030303');
const camera=new T.OrthographicCamera(-4.6,4.6,3.6,-3.6,.1,100);camera.position.set(0,7,7.6);camera.lookAt(0,.1,0);
const renderer=new T.WebGLRenderer({antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,1.25));renderer.outputColorSpace=T.SRGBColorSpace;host.prepend(renderer.domElement);
scene.add(new T.AmbientLight(0xffffff,1.2));const sun=new T.DirectionalLight(0xffffff,1.6);sun.position.set(-3,7,2);scene.add(sun);
const travelerLight=new T.PointLight(0xffffff,4,2.1,2);scene.add(travelerLight);
const terrainMaterial=new T.MeshStandardMaterial({vertexColors:true,roughness:1,flatShading:true,side:T.DoubleSide});
const cloudTime={value:0};
terrainMaterial.onBeforeCompile=shader=>{
 shader.uniforms.cloudTime=cloudTime;
 shader.uniforms.logoLightPosition={value:travelerLight.position};
 shader.uniforms.logoLightColor={value:travelerLight.color};
 shader.vertexShader='varying float terrainAltitude; varying vec2 cloudLocation;\n'+shader.vertexShader;
 shader.vertexShader=shader.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\nterrainAltitude=position.y; cloudLocation=position.xz;');
 shader.fragmentShader=`varying float terrainAltitude; varying vec2 cloudLocation; uniform float cloudTime; uniform vec3 logoLightPosition; uniform vec3 logoLightColor;
 float cloudHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7))+41.0)*43758.5453);}
 float cloudNoise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(cloudHash(i),cloudHash(i+vec2(1,0)),f.x),mix(cloudHash(i+vec2(0,1)),cloudHash(i+vec2(1,1)),f.x),f.y);}
 `+shader.fragmentShader;
 shader.fragmentShader=shader.fragmentShader.replace('#include <clipping_planes_fragment>','#include <clipping_planes_fragment>\nif(terrainAltitude<0.0) discard;');
 shader.fragmentShader=shader.fragmentShader.replace('#include <lights_fragment_end>','#include <lights_fragment_end>\nreflectedLight.directSpecular=vec3(0.0); reflectedLight.indirectSpecular=vec3(0.0);');
 shader.fragmentShader=shader.fragmentShader.replace('#include <emissivemap_fragment>',`#include <emissivemap_fragment>
 float localLight=max(0.0,1.0-distance(vec3(cloudLocation.x,terrainAltitude,cloudLocation.y),logoLightPosition)/1.6);
 totalEmissiveRadiance+=logoLightColor*localLight*localLight*.065;
 `);
 shader.fragmentShader=shader.fragmentShader.replace('#include <color_fragment>',`#include <color_fragment>
 float cloudCover=smoothstep(.3,.82,cloudNoise(cloudLocation*.7+vec2(cloudTime*.025,7.0+cloudTime*.012)));
 diffuseColor.rgb*=1.0-cloudCover*.7;
 `);
};
const tiles=createTiles(scene,terrainMaterial);
const floorGrid=new T.GridHelper(20,40,0x3c5447,0x25392e);floorGrid.position.y=-.07;floorGrid.material.transparent=true;floorGrid.material.opacity=.28;scene.add(floorGrid);
const spriteMaterials=new Map();
function sprite(text,color,size=64){const cacheKey=`${text}/${color}/${size}`;if(spriteMaterials.has(cacheKey))return new T.Sprite(spriteMaterials.get(cacheKey));const c=document.createElement('canvas');c.width=c.height=size;const ctx=c.getContext('2d');ctx.font=`${size*.7}px monospace`;ctx.textAlign='center';ctx.textBaseline='middle';const ink=new T.Color(color),tone=ink.r*.2126+ink.g*.7152+ink.b*.0722;ctx.fillStyle=new T.Color().setRGB(tone,tone,tone).getStyle();ctx.fillText(text,size/2,size/2);const texture=new T.CanvasTexture(c);const material=new T.SpriteMaterial({map:texture,transparent:true,depthWrite:false});spriteMaterials.set(cacheKey,material);return new T.Sprite(material);}
const marker=sprite('✦','#ff973c');marker.scale.setScalar(.21);marker.material.depthTest=false;marker.renderOrder=10;scene.add(marker);
const logoStage=document.createElement('div');logoStage.className='traveler-logo';logoStage.setAttribute('role','img');logoStage.setAttribute('aria-label','Logo Ultrasuono animato dal clima della posizione');host.append(logoStage);
let climateLogo;
try{climateLogo=createClimateLogo(logoStage);}catch(error){logoStage.textContent='';console.warn('Logo 3D non disponibile',error);}
const ring=new T.Mesh(new T.RingGeometry(.18,.184,64),new T.MeshBasicMaterial({color:0xc6b27a,transparent:true,opacity:.6,side:T.DoubleSide,depthTest:false}));ring.rotation.x=-Math.PI/2;scene.add(ring);
const pathPositions=[],pathGeo=new T.BufferGeometry(),pathLine=new T.Line(pathGeo,new T.LineDashedMaterial({color:0xe6934e,dashSize:.025,gapSize:.035,transparent:true,opacity:.65}));scene.add(pathLine);
floorGrid.visible=ring.visible=pathLine.visible=false;
let pos={x:.15,z:.65},target={...pos},climate=climateAt(pos.x,pos.z),drag=false,dirty=true,lastTrail=0,pointer;
const keys=new Set();
const labelSpecs=[['COSTA',-2.1,1.25],['VALLE UMIDA',.15,.6],['ALTOPIANO SECCO',-1.5,-.7],['M. NEMBRO',1.2,-1.15],['PENDICI ORIENTALI',2,.5]];
const labels=labelSpecs.map(([name,x,z])=>{const node=document.createElement('div');node.className='map-label';node.innerHTML=`${name}<small>${Math.round(Math.max(0,height(x,z))*2100)} m</small>`;$('labels').append(node);return {node,p:new T.Vector3(x,groundY(height(x,z))+.07,z)};});
function resize(){const aspect=host.clientWidth/host.clientHeight;camera.left=-1.35*aspect;camera.right=1.35*aspect;camera.top=1.35;camera.bottom=-1.35;camera.updateProjectionMatrix();renderer.setSize(host.clientWidth,host.clientHeight,false);dirty=true;}
new ResizeObserver(resize).observe(host);
const ray=new T.Raycaster(),mouse=new T.Vector2();
function navigate(event){pointer=event;const b=renderer.domElement.getBoundingClientRect();mouse.set((event.clientX-b.left)/b.width*2-1,-(event.clientY-b.top)/b.height*2+1);ray.setFromCamera(mouse,camera);const hit=ray.intersectObjects(tiles.meshes())[0];const point=hit?.point||ray.ray.intersectPlane(new T.Plane(new T.Vector3(0,1,0),-groundY(height(pos.x,pos.z))),new T.Vector3());if(point)target={x:point.x,z:point.z};}
renderer.domElement.addEventListener('pointerdown',e=>{drag=true;renderer.domElement.setPointerCapture(e.pointerId);host.focus();navigate(e);});renderer.domElement.addEventListener('pointermove',e=>{if(drag)navigate(e);});renderer.domElement.addEventListener('pointerup',()=>drag=false);renderer.domElement.addEventListener('pointercancel',()=>drag=false);
host.addEventListener('keydown',e=>{if(!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(e.key))return;e.preventDefault();keys.add(e.key);});
host.addEventListener('keyup',e=>keys.delete(e.key));host.addEventListener('blur',()=>{keys.clear();drag=false;});
document.querySelectorAll('[data-dx]').forEach(b=>b.onclick=()=>{target={x:target.x+ +b.dataset.dx,z:target.z+ +b.dataset.dz};});
function updateUI(){climate=climateAt(pos.x,pos.z,cloudTime.value);climateLogo?.update({...climate,temperature:effectTemperature(climate.temperature)});const palette=logoClimate(effectTemperature(climate.temperature),climate.humidity,climate.light*100);travelerLight.color.set(palette.color[0]).lerp(new T.Color(palette.color[1]),palette.color[2]);travelerLight.intensity=3+palette.glow*3;travelerLight.position.set(pos.x,groundY(height(pos.x,pos.z))+.45,pos.z);$('temperature').textContent=`${Math.round(climate.temperature)}°`;$('humidity').textContent=`${Math.round(climate.humidity)}%`;$('light').textContent=`${Math.round(climate.light*100)}%`;$('temp-meter').value=climate.temperature;$('humidity-meter').value=climate.humidity;$('light-meter').value=climate.light*100;$('region').textContent=({'Costa':'COAST','Cime fredde':'COLD PEAKS','Montagne calde':'WARM MOUNTAINS','Montagne temperate':'TEMPERATE MOUNTAINS','Foresta umida':'HUMID FOREST','Foresta tropicale':'TROPICAL FOREST','Deserto':'DESERT','Prateria secca':'DRY GRASSLAND','Pendici temperate':'TEMPERATE HILLS'})[climate.region]||climate.region;const coords=`X ${((pos.x+4)*12.5).toFixed(1)} / Y ${((pos.z+4)*12.5).toFixed(1)}`;$('coords').textContent=$('footer-coords').textContent=coords;$('altitude').textContent=`QUOTA ${climate.altitude} m`;applyAudio();}
let recording;
let ctx,worklet,source,stream,buffer,mode='demo',fileBuffer,playing=false,busy=false;
function applyAudio(){if(!worklet)return;for(const [k,v] of Object.entries({temperature:effectTemperature(climate.temperature),humidity:climate.humidity,light:climate.light,mix:+$('mix').value/100,volume:+$('volume').value/100}))worklet.parameters.get(k).setTargetAtTime(v,ctx.currentTime,.3);}
function stop(){recording?.stop();if(source){try{source.stop();}catch{}source.disconnect();source=null;}stream?.getTracks().forEach(t=>t.stop());stream=null;playing=false;$('play-label').textContent='play';$('play').classList.remove('playing');$('play').setAttribute('aria-label','Play audio');}
async function initAudio(){if(worklet)return;ctx??=new AudioContext({sampleRate:48000});await ctx.resume();const r=await fetch('./ultrasuono.wasm');if(!r.ok)throw Error('DSP non disponibile');const module=await WebAssembly.compile(await r.arrayBuffer());await ctx.audioWorklet.addModule('./ultrasuono-worklet.js');worklet=new AudioWorkletNode(ctx,'ultrasuono',{outputChannelCount:[2],processorOptions:{module}});worklet.connect(ctx.destination);worklet.onprocessorerror=()=>{stop();$('status').textContent='Errore DSP: ricarica la pagina.';};applyAudio();}
async function start(){await initAudio();await ctx.resume();if(mode==='mic'){if(!navigator.mediaDevices)throw Error('Microfono non disponibile');stream=await navigator.mediaDevices.getUserMedia({audio:{echoCancellation:false,noiseSuppression:false,autoGainControl:false}});source=ctx.createMediaStreamSource(stream);}else{if(mode==='demo'&&!buffer){const r=await fetch('./demo-loop.wav');if(!r.ok)throw Error('Loop non disponibile');buffer=await ctx.decodeAudioData(await r.arrayBuffer());}const audio=mode==='file'?fileBuffer:buffer;if(!audio)throw Error('Seleziona un file');source=ctx.createBufferSource();source.buffer=audio;source.loop=true;}source.connect(worklet);source.start?.();playing=true;$('play-label').textContent='stop';$('play').classList.add('playing');$('play').setAttribute('aria-label','Stop audio');}
$('record').onclick=async()=>{
 if(busy)return;
 if(recording?.active){recording.stop();return;}
 busy=true;$('record').disabled=true;
 try{
  if(!playing)await start();
  recording??=createRecording(ctx,worklet,(active,finalizing=false)=>{
   $('record').disabled=finalizing;$('record').classList.toggle('recording',active);
   $('record').setAttribute('aria-pressed',String(active));
   $('record-label').textContent=active?'save':finalizing?'saving…':'rec';
  });
  recording.start();
 }catch(error){$('status').textContent=error.message;$('record-label').textContent='record unavailable';}
 finally{busy=false;if(!recording?.active)$('record').disabled=false;}
};
$('play').onclick=async()=>{if(busy)return;busy=true;$('play').disabled=true;try{if(playing)stop();else await start();$('status').textContent=playing?'DSP originale attivo · audio locale. Con il microfono usa le cuffie.':'Naviga per cambiare clima e suono.';}catch(e){stop();$('status').textContent=e.message;}finally{busy=false;$('play').disabled=false;}};
function setMode(next){stop();mode=next;for(const id of ['demo','mic']){$(id).classList.toggle('active',id===mode);$(id).setAttribute('aria-pressed',String(id===mode));}$('file-tab').classList.toggle('active',mode==='file');}
$('demo').onclick=()=>{if(!busy)setMode('demo');};$('mic').onclick=()=>{if(!busy)setMode('mic');};$('file').onchange=async e=>{const f=e.target.files[0];if(!f||busy)return;busy=true;setMode('file');fileBuffer=null;try{if(f.size>50*1024*1024)throw Error('Limite file: 50 MB');await initAudio();fileBuffer=await ctx.decodeAudioData(await f.arrayBuffer());$('status').textContent=`File pronto: ${f.name}`;}catch(e){$('status').textContent=e.message;}finally{busy=false;}};for(const id of ['mix','volume'])$(id).oninput=applyAudio;window.addEventListener('pagehide',stop);
let last=performance.now(),lastClimate=0,cameraGround=height(pos.x,pos.z)*.85;function frame(now){const dt=Math.min((now-last)/1000,.05);last=now;cloudTime.value=now/1000;if(now-lastClimate>150){dirty=true;lastClimate=now;}if(drag&&pointer)navigate(pointer);if(keys.size){target.x+=((keys.has('ArrowRight')?1:0)-(keys.has('ArrowLeft')?1:0))*dt*.8;target.z+=((keys.has('ArrowDown')?1:0)-(keys.has('ArrowUp')?1:0))*dt*.8;}const d=Math.hypot(target.x-pos.x,target.z-pos.z);if(d>.0001){const f=1-Math.exp(-dt*4);pos.x+=(target.x-pos.x)*f;pos.z+=(target.z-pos.z)*f;dirty=true;}
 cameraGround+=(height(pos.x,pos.z)*.85-cameraGround)*(1-Math.exp(-dt*5));camera.position.set(pos.x+2.5,cameraGround+6.5,pos.z+8.3);camera.lookAt(pos.x,cameraGround+.1,pos.z);camera.updateMatrixWorld();tiles.update(pos.x,pos.z,host.clientWidth/host.clientHeight,renderer.domElement.height);floorGrid.position.x=Math.floor(pos.x*2)/2;floorGrid.position.z=Math.floor(pos.z*2)/2;
 if(dirty){const terrainStats=tiles.stats();host.dataset.activeTiles=terrainStats.tiles;host.dataset.terrainTriangles=terrainStats.triangles;host.dataset.pendingTiles=terrainStats.pending;updateUI();const y=groundY(height(pos.x,pos.z))+.055;marker.position.set(pos.x,y+.055,pos.z);ring.position.set(pos.x,y,pos.z);for(const {node,p} of labels){const near=Math.hypot(p.x-pos.x,p.z-pos.z)<1.3;node.style.display=near?'block':'none';const v=p.clone().project(camera);node.style.left=`${(v.x+1)/2*host.clientWidth}px`;node.style.top=`${(1-v.y)/2*host.clientHeight}px`;}if(d>.001&&now-lastTrail>100){pathPositions.push(pos.x,y,pos.z);if(pathPositions.length>750)pathPositions.splice(0,3);pathGeo.setAttribute('position',new T.Float32BufferAttribute(pathPositions,3));pathLine.computeLineDistances();lastTrail=now;}dirty=false;}
 const logoPoint=marker.position.clone().project(camera);logoStage.style.left=`${(logoPoint.x+1)/2*host.clientWidth}px`;logoStage.style.top=`${(1-logoPoint.y)/2*host.clientHeight}px`;marker.visible=logoStage.dataset.loaded!=='true';renderer.render(scene,camera);requestAnimationFrame(frame);}
window.addEventListener('pagehide',()=>tiles.dispose());
scene.traverse(object=>{if(!object.material)return;const materials=Array.isArray(object.material)?object.material:[object.material];for(const material of materials){if(material.color){const c=material.color,g=c.r*.2126+c.g*.7152+c.b*.0722;c.setRGB(g,g,g);}}});
resize();updateUI();$('status').textContent='Terreno a tasselli · esplorazione progressiva. Avvia il DSP per ascoltare.';requestAnimationFrame(frame);
