export function smoothElevation(current,target,dt){
 return current+(target-current)*(1-Math.exp(-7*Math.max(0,dt)));
}
