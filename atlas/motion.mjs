export function smoothElevation(current,target,dt){
 return current+(target-current)*(1-Math.exp(-6*Math.max(0,dt)));
}
