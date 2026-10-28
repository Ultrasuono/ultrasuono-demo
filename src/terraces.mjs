// Clip each terrain triangle at the contour heights; tops, walls and outlines share intersections.
export function buildTerraces(grid,radius,spacing,offsetX=0,offsetZ=0){
 const positions=[],colors=[],outlines=[],n=grid.length-1,step=radius*2/n;
 function vertex(p,y){positions.push(p[0],y,p[1]);colors.push(0,0,0);}
 function clip(poly,level,above){const result=[];for(let i=0;i<poly.length;i++){
  const a=poly[i],b=poly[(i+1)%poly.length],inside=above?a[2]>=level:a[2]<=level,next=above?b[2]>=level:b[2]<=level;
  if(inside)result.push(a);
  if(inside!==next){const t=(level-a[2])/(b[2]-a[2]);result.push([a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,level]);}
 }return result;}
 function triangle(tri){const lo=Math.min(...tri.map(p=>p[2])),hi=Math.max(...tri.map(p=>p[2]));if(hi<0)return;
  for(let band=Math.max(0,Math.floor(lo/spacing));band<=Math.floor(hi/spacing);band++){
   const level=band*spacing,top=level*.85,poly=clip(clip(tri,level,true),level+spacing,false);
   for(let k=1;k+1<poly.length;k++){vertex(poly[0],top);vertex(poly[k],top);vertex(poly[k+1],top);}
   if(lo>=level||hi<level)continue;
   const cut=[];for(let k=0;k<3;k++){const a=tri[k],b=tri[(k+1)%3];if((a[2]<level)===(b[2]<level))continue;const t=(level-a[2])/(b[2]-a[2]);cut.push([a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t]);}
   if(cut.length!==2)continue;
   const [a,b]=cut,bottom=band===0?-.047:(level-spacing)*.85;
   vertex(a,bottom);vertex(b,bottom);vertex(b,top);vertex(a,bottom);vertex(b,top);vertex(a,top);
   outlines.push(a[0],top+.002,a[1],b[0],top+.002,b[1]);
  }
 }
 for(let j=0;j<n;j++)for(let i=0;i<n;i++){
  const a=[offsetX-radius+i*step,offsetZ-radius+j*step,grid[j][i]],b=[a[0]+step,a[1],grid[j][i+1]],c=[a[0]+step,a[1]+step,grid[j+1][i+1]],d=[a[0],a[1]+step,grid[j+1][i]];
  triangle([a,d,b]);triangle([b,d,c]);
 }
 return {positions,colors,outlines};
}
