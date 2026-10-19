const fract=x=>x-Math.floor(x),mix=(a,b,t)=>a+(b-a)*t;
function hash(x,y){return fract(Math.sin(x*127.1+y*311.7+41)*43758.5453);}
function noise(x,y){const i=Math.floor(x),j=Math.floor(y);let u=fract(x),v=fract(y);u=u*u*(3-2*u);v=v*v*(3-2*v);return mix(mix(hash(i,j),hash(i+1,j),u),mix(hash(i,j+1),hash(i+1,j+1),u),v);}
function fbm(x,z){let n=0,a=.5;for(let i=0;i<5;i++){n+=a*noise(x,z);x=x*2.03+8.4;z=z*2.03-2.7;a*=.5;}return n;}
export function height(x,z){
 const radial=Math.sqrt((x/3.65)**2+(z/2.9)**2);
 const land=.56-radial*.53+(fbm(x*.8+10,z*.8+10)-.45)*.42;
 const mountain=Math.exp(-((x-1.2)**2/1.5+(z+1.1)**2/1.1))*1.85;
 const ridge=Math.pow(1-Math.abs(2*noise(x*2.6,z*2.6)-1),3);
 const plateau=Math.exp(-((x+1.5)**2/1.7+(z+.7)**2/1.3))*.36;
 const river=Math.exp(-((x-Math.sin(z*1.65)*.28+.12)**2)/.026)*.17;
 const local=land+mountain*(.56+ridge*.44)+plateau-river-.15;
 const blend=Math.max(0,Math.min(1,(radial-.8)/.9));
 const mountainField=noise(x*.17+83,z*.17-46);
 const mountainAmount=Math.max(0,Math.min(1,(mountainField-.48)/.32));
 const ranges=mountainAmount*mountainAmount*(3-2*mountainAmount)*1.65*(.65+ridge*.35);
 const beyond=(fbm(x*.36+30,z*.36+30)-.43)*2.1+(fbm(x*1.3,z*1.3)-.45)*.3+ranges;
 return mix(local,beyond,blend*blend*(3-2*blend));
}
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
export function cloudAt(x,z,time=0){const n=noise(x*.7+time*.025,z*.7+7+time*.012);const f=clamp((n-.3)/.52,0,1);return f*f*(3-2*f);}
export function exposureAt(x,z){const h=height(x,z),dx=(height(x+.04,z)-height(x-.04,z))/.08,dz=(height(x,z+.04)-height(x,z-.04))/.08;let shade=0;
 for(let i=1;i<=7;i++){const distance=i*.15;shade=Math.max(shade,(height(x-distance,z-distance*.7)-h)/distance);}
 const facing=clamp((.8+dx*.45+dz*.3)/(.8*Math.sqrt(1+dx*dx+dz*dz)),.12,1);
 return facing*(1-.8*clamp((shade-.18)/.65,0,1));}
export function landClimate(x,z,h=height(x,z)){
 const water=Math.exp(-((x-Math.sin(z*1.65)*.28+.12)**2)/.2);
 // Broad climate regions keep deserts and forests apart, with a temperate belt between them.
 const distance=clamp((Math.hypot(x,z)-6)/8,0,1),farBlend=distance*distance*(3-2*distance);
 const localRegion=x+.65*Math.sin(z*.38)+(noise(x*.13+45,z*.13+45)-.5)*1.4;
 const distantRegion=(noise(x*.1+61,z*.1-27)-.5)*12-1.5;
 const regionX=mix(localRegion,distantRegion,farBlend);
 const transition=clamp((regionX+3.5)/2.2,0,1),dryness=1-transition*transition*(3-2*transition);
 const heat=mix(-clamp(x,-8,8)*1.5,(noise(x*.09-17,z*.09+39)-.5)*16,farBlend);
 const tropical=clamp((noise(x*.12-53,z*.12+71)-.35)/.35,0,1)*(1-dryness);
 const humidity=clamp(74-54*dryness+water*22+noise(x+20,z+20)*6-h*18+tropical*14,12,98),temperature=clamp(29-h*16+heat+dryness*14+tropical*12,-20,50);
 const biome=h<.025?'water':h>.72&&temperature<10?'alpine':humidity>=72&&temperature>3?'forest':humidity<=35&&temperature>22?'desert':humidity<55&&temperature>10?'drygrassland':'grassland';
 return {humidity,temperature,biome};
}
export function effectTemperature(temperature){return clamp(temperature<=20?temperature:20+(temperature-20)*2/3,-20,40);}
export function climateAt(x,z,time=0){const h=height(x,z),{humidity,temperature,biome}=landClimate(x,z,h),cloud=cloudAt(x,z,time),exposure=exposureAt(x,z);
 return {temperature,humidity,biome,light:clamp(.025+.975*exposure*(1-cloud*.92),0,1),cloud,exposure,altitude:Math.round(Math.max(0,h)*2100),region:h<.035?'Costa':h>.55?(temperature<10?'Cime fredde':temperature>22?'Montagne calde':'Montagne temperate'):biome==='forest'?(temperature>=30?'Foresta tropicale':'Foresta umida'):biome==='desert'?'Deserto':biome==='drygrassland'?'Prateria secca':'Pendici temperate'};}
