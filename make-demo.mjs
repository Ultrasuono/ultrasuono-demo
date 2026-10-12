// Original procedural composition; regenerate with node make-demo.mjs.
import {writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
const rate=44100,beat=60/104,duration=beat*64,length=Math.round(duration*rate);
const channels=[new Float32Array(length),new Float32Array(length)];
let seed=42;
const noise=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/2147483648-1;};
const hz=midi=>440*2**((midi-69)/12);
function note(start,seconds,pan,synth){
  const offset=Math.round(start*rate),count=Math.min(Math.round(seconds*rate),length-offset);
  const left=Math.sqrt((1-pan)/2),right=Math.sqrt((1+pan)/2);
  for(let i=0;i<count;i++){const t=i/rate;const value=synth(t)*Math.min(1,t/.004,(seconds-t)/.012);channels[0][offset+i]+=value*left;channels[1][offset+i]+=value*right;}
}
const chords=[[45,52,60,64],[41,48,57,60],[48,55,62,64],[43,50,57,62]];
for(let bar=0;bar<16;bar++){
  const chord=chords[bar%4],start=bar*4*beat;
  chord.forEach((m,i)=>note(start,beat*3.9,(i-1.5)/2,t=>{
    const f=hz(m+12);return .065*Math.sin(Math.PI*t/(beat*3.9))*(Math.sin(2*Math.PI*f*t)+.25*Math.sin(2*Math.PI*f*1.003*t));
  }));
  for(let step=0;step<8;step++){
    const at=start+step*beat/2;
    note(at,.11,step%2?.5:-.5,t=>noise()*.065*Math.exp(-t*65));
    const m=chord[(step+Math.floor(bar/4))%4]+24;
    note(at,beat*.75,Math.sin(step*1.8)*.7,t=>{
      const f=hz(m);return .11*Math.exp(-t*8)*(Math.sin(2*Math.PI*f*t)+.3*Math.sin(2*Math.PI*f*2*t)+.12*Math.sin(2*Math.PI*f*3*t));
    });
    if(step%2===0){const f=hz(chord[0]+(step===6?7:0));note(at,beat*.7,0,t=>.23*Math.exp(-t*7)*(Math.sin(2*Math.PI*f*t)+.2*Math.sin(2*Math.PI*f*2*t)));}
  }
  for(const pulse of [0,2, ...(bar%4===3?[3.5]:[])])note(start+pulse*beat,.28,0,t=>.48*Math.exp(-t*20)*Math.sin(2*Math.PI*(48*t+95*.025*(1-Math.exp(-t/.025)))));
  for(const pulse of [1,3])note(start+pulse*beat,.18,.12,t=>.16*Math.exp(-t*24)*(noise()+.35*Math.sin(2*Math.PI*180*t)));
  if(bar%2===1)note(start+3.5*beat,.2,-.65,t=>.09*Math.exp(-t*18)*(Math.sin(2*Math.PI*1320*t)+Math.sin(2*Math.PI*1847*t)));
}
let peak=0,energy=0;
for(const data of channels)for(const value of data){assert.ok(Number.isFinite(value));peak=Math.max(peak,Math.abs(value));energy+=value*value;}
assert.ok(peak>0&&energy/length/2>.001);
const wav=Buffer.alloc(44+length*4);
wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(2,22);wav.writeUInt32LE(rate,24);wav.writeUInt32LE(rate*4,28);wav.writeUInt16LE(4,32);wav.writeUInt16LE(16,34);wav.write('data',36);wav.writeUInt32LE(length*4,40);
for(let i=0;i<length;i++)for(let ch=0;ch<2;ch++){
  const fade=Math.min(1,i/(rate*.015),(length-1-i)/(rate*.025));
  wav.writeInt16LE(Math.round(channels[ch][i]/peak*.82*fade*32767),44+i*4+ch*2);
}
writeFileSync(new URL('demo-loop.wav',import.meta.url),wav);
console.log(`OK: original stereo loop, ${duration.toFixed(1)}s, peak normalized to 0.82, ${(wav.length/1024/1024).toFixed(1)} MB.`);
