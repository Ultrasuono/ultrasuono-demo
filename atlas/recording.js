export function createRecording(context,output,onChange){
 let recorder,chunks=[],destination;
 return {
  get active(){return recorder?.state==='recording';},
  start(){
   if(!globalThis.MediaRecorder)throw Error('Recording is not supported in this browser.');
   destination??=context.createMediaStreamDestination();
   const mimeType=['audio/webm;codecs=opus','audio/mp4','audio/webm'].find(type=>MediaRecorder.isTypeSupported(type));
   recorder=new MediaRecorder(destination.stream,mimeType?{mimeType}:undefined);chunks=[];
   recorder.ondataavailable=event=>{if(event.data.size)chunks.push(event.data);};
   recorder.onstop=()=>{
    output.disconnect(destination);onChange(false);
    if(!chunks.length)return;
    const type=recorder.mimeType||chunks[0].type,blob=new Blob(chunks,{type});
    const url=URL.createObjectURL(blob),link=document.createElement('a');
    const extension=type.includes('mp4')?'m4a':type.includes('ogg')?'ogg':'webm';
    link.href=url;link.download=`ultrasuono-${new Date().toISOString().replace(/[:.]/g,'-')}.${extension}`;
    document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),60000);
    chunks=[];
   };
   recorder.onerror=()=>{onChange(false);};
   output.connect(destination);recorder.start(1000);onChange(true);
  },
  stop(){if(recorder?.state==='recording'){recorder.stop();onChange(false,true);}}
 };
}
