export function visualBiome(temperature,humidity){
 if(temperature<10)return humidity<55?'cold-dry':'cold-wet';
 if(temperature>=28)return humidity>=65?'hot-wet':humidity<45?'hot-dry':'temperate';
 return 'temperate';
}

