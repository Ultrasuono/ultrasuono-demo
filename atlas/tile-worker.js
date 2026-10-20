import {makeTile} from './tile-data.mjs';
self.onmessage=({data:{key,tx,tz}})=>{
 try{const tile=makeTile(tx,tz);self.postMessage({key,tile},Object.values(tile).map(array=>array.buffer));}
 catch(error){self.postMessage({key,error:error.message});}
};
