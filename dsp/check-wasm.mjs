import {readFileSync} from 'node:fs';
import assert from 'node:assert/strict';
const module=await WebAssembly.compile(readFileSync('ultrasuono.wasm'));
console.log('WASM imports:',WebAssembly.Module.imports(module));
const {exports:dsp}=await WebAssembly.instantiate(module,{
  wasi_snapshot_preview1:{proc_exit:()=>{throw Error('DSP exit');}},
  env:{emscripten_notify_memory_growth:()=>{}}
});
dsp._initialize?.();dsp.ultra_init(48000);
const memory=new Float32Array(dsp.memory.buffer),input=dsp.ultra_input()/4,output=dsp.ultra_output()/4;
let energy=0;
for(let block=0;block<2000;block++){
  for(let i=0;i<128;i++)memory[input+i]=memory[input+128+i]=.1*Math.sin((block*128+i)*.035);
  dsp.ultra_process(block/2000,-20+block/2000*60,block/2000*100,.65,.35);
  for(const x of memory.subarray(output,output+256)){assert.ok(Number.isFinite(x)&&Math.abs(x)<=.9951);energy+=x*x;}
}
assert.ok(energy>1);
console.log('OK: WASM initialization, 256,000 stereo samples, finite output and safety limits.');
