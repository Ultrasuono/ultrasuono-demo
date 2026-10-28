class UltrasuonoProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors(){return [
    {name:'light',defaultValue:1,minValue:0,maxValue:1,automationRate:'k-rate'},
    {name:'temperature',defaultValue:20,minValue:-50,maxValue:60,automationRate:'k-rate'},
    {name:'humidity',defaultValue:50,minValue:0,maxValue:100,automationRate:'k-rate'},
    {name:'mix',defaultValue:.65,minValue:0,maxValue:1,automationRate:'k-rate'},
    {name:'volume',defaultValue:.35,minValue:0,maxValue:1,automationRate:'k-rate'}];}
  constructor(options){
    super();
    const instance=new WebAssembly.Instance(options.processorOptions.module,{
      wasi_snapshot_preview1:{proc_exit:()=>{throw new Error('DSP initialization failed');}},
      env:{emscripten_notify_memory_growth:()=>{}}
    });
    this.dsp=instance.exports;
    this.dsp._initialize?.();
    this.dsp.ultra_init(sampleRate);
    this.memory=new Float32Array(this.dsp.memory.buffer);
    this.input=this.dsp.ultra_input()/4;
    this.output=this.dsp.ultra_output()/4;
  }
  process(inputs,outputs,p){
    const out=outputs[0],input=inputs[0];
    if(!out.length)return true;
    if(out[0].length!==128){out.forEach(ch=>ch.fill(0));return true;}
    for(let ch=0;ch<2;ch++){
      const data=input[ch]||input[0];
      const target=this.memory.subarray(this.input+ch*128,this.input+(ch+1)*128);
      if(data)target.set(data);else target.fill(0);
    }
    this.dsp.ultra_process(p.light[0],p.temperature[0],p.humidity[0],p.mix[0],p.volume[0]);
    out.forEach((ch,i)=>ch.set(this.memory.subarray(this.output+i*128,this.output+(i+1)*128)));
    return true;
  }
}
registerProcessor('ultrasuono',UltrasuonoProcessor);
