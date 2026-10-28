// Call before the first await so resuming still belongs to the play gesture.
export function resumeAudio(context,mode,navigator){
 if(navigator.audioSession){
  try{navigator.audioSession.type=mode==='mic'?'play-and-record':'playback';}catch{}
 }
 return context.resume();
}
