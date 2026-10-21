import assert from 'node:assert/strict';
import {createRecording} from './recording.js';
const downloads=[],connections=[],changes=[];
let supported='audio/mp4';
globalThis.document={body:{append(){}},createElement(){return {click(){downloads.push(this.download);},remove(){}};}};
URL.createObjectURL=blob=>{assert.ok(blob.size>0);assert.equal(blob.type,supported);return 'blob:test';};URL.revokeObjectURL=()=>{};
globalThis.setTimeout=fn=>fn();
globalThis.MediaRecorder=class{
 static isTypeSupported(type){return type===supported;}
 constructor(stream,options){assert.equal(stream,'processed-stream');this.mimeType=options.mimeType;this.state='inactive';}
 start(){this.state='recording';}
 stop(){this.state='inactive';queueMicrotask(()=>{this.ondataavailable({data:new Blob(['audio'],{type:this.mimeType})});this.onstop();});}
};
const destination={stream:'processed-stream'},context={createMediaStreamDestination:()=>destination};
const output={connect:d=>connections.push(['connect',d]),disconnect:d=>connections.push(['disconnect',d])};
const recording=createRecording(context,output,(...args)=>changes.push(args));
for(const type of ['audio/mp4','audio/webm;codecs=opus']){supported=type;recording.start();assert.ok(recording.active);recording.stop();assert.ok(!recording.active);await new Promise(resolve=>queueMicrotask(resolve));}
assert.ok(downloads[0].endsWith('.m4a'));assert.ok(downloads[1].endsWith('.webm'));
assert.deepEqual(connections.map(c=>c[0]),['connect','disconnect','connect','disconnect']);
assert.deepEqual(changes.slice(0,3),[[true],[false,true],[false]]);
globalThis.MediaRecorder=undefined;assert.throws(()=>recording.start(),/not supported/);
console.log('OK: processed audio routing, repeated recording, finalization, MP4/WebM download and unsupported browser.');
