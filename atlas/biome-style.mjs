export function visualBiome(temperature,humidity){
 if(temperature<10)return humidity<55?'cold-dry':'cold-wet';
 if(temperature>=28)return humidity>=65?'hot-wet':humidity<45?'hot-dry':'temperate';
 return 'temperate';
}

export function biomeDetails(zone,x,y,z,seed){
 const plants=[],ground=[];
 const line=(list,a,b)=>list.push(...a,...b);
 if(zone==='hot-wet'&&seed>.2){
  const tall=.18+seed*.13,r=.065+seed*.025;
  line(plants,[x,y,z],[x+.012,y+tall,z]);
  // Wide, layered crowns distinguish tropical trees from the conifers.
  for(let layer=0;layer<3;layer++)for(let k=0;k<8;k++){
   const a=k*Math.PI/4,b=(k+1)*Math.PI/4,scale=1-layer*.24,up=y+tall+layer*.025;
   const p=[x+Math.cos(a)*r*scale,up,z+Math.sin(a)*r*scale];
   line(plants,p,[x+Math.cos(b)*r*scale,up,z+Math.sin(b)*r*scale]);
   if(layer===0)line(plants,[x+.012,y+tall*.72,z],p);
  }
 }else if(zone==='cold-dry'&&seed>.82){
  for(let k=0;k<3;k++){
   const dx=(k-1)*.025,h=.035+seed*.045-k*.007;
   const a=[x+dx-.012,y,z],b=[x+dx+.012,y,z+.012],tip=[x+dx+.005,y+h,z+.006];
   for(const [u,v] of [[a,b],[a,tip],[b,tip]])line(plants,u,v);
  }
 }
 if(zone==='hot-dry'&&seed>.8){
  for(let row=0;row<3;row++)for(let k=0;k<4;k++){
   const a=-.07+k*.035,b=a+.035;
   line(ground,[x+a,y,z+row*.025+Math.sin(a*18)*.012],[x+b,y,z+row*.025+Math.sin(b*18)*.012]);
  }
 }else if(zone==='cold-dry'&&seed>.72){
  const center=[x,y,z];
  for(let k=0;k<3;k++){const a=k*2.1+seed;line(ground,center,[x+Math.cos(a)*.06,y,z+Math.sin(a)*.06]);}
 }else if(zone==='cold-wet'&&seed>.84){
  for(let k=0;k<4;k++){const a=k*Math.PI/2,b=(k+1)*Math.PI/2;line(ground,[x+Math.cos(a)*.035,y,z+Math.sin(a)*.02],[x+Math.cos(b)*.035,y,z+Math.sin(b)*.02]);}
 }
 return {plants,ground};
}
