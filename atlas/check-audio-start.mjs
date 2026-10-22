import assert from 'node:assert/strict';
import {resumeAudio} from './audio-start.mjs';
for(const mode of ['demo','file','mic']){
 const events=[],nav={audioSession:{set type(value){events.push(value);}}};
 const promise=resumeAudio({resume(){events.push('resume');return Promise.resolve();}},mode,nav);
 assert.deepEqual(events,[mode==='mic'?'play-and-record':'playback','resume']);await promise;
}
let calls=0;const context={resume(){calls++;return Promise.resolve();}};
await resumeAudio(context,'demo',{});
await resumeAudio(context,'demo',{audioSession:{set type(value){throw Error('Unsupported');}}});
assert.equal(calls,2);
console.log('OK: session configured before synchronous resume, microphone mode, unsupported API fallback.');
