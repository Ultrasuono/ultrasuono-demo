const climates = [
  {name:'Costa mediterranea',type:'Mediterraneo',temp:21,humidity:62,swing:6,offset:2},
  {name:'Foresta equatoriale',type:'Tropicale',temp:28,humidity:85,swing:3,offset:-5},
  {name:'Dune del deserto',type:'Arido',temp:31,humidity:22,swing:12,offset:3},
  {name:'Fiordo del nord',type:'Subartico',temp:4,humidity:77,swing:4,offset:1},
  {name:'Altopiano andino',type:'Montano',temp:9,humidity:43,swing:9,offset:-5},
  {name:'Isola nel Pacifico',type:'Oceanico tropicale',temp:26,humidity:76,swing:3,offset:11},
  {name:'Pianura continentale',type:'Continentale',temp:16,humidity:56,swing:8,offset:8},
  {name:'Bosco temperato',type:'Temperato',temp:14,humidity:73,swing:5,offset:-7},
  {name:'Tundra polare',type:'Polare',temp:-12,humidity:68,swing:4,offset:-3},
  {name:'Costa atlantica',type:'Oceanico',temp:17,humidity:80,swing:4,offset:0}
];
export const stations=Array.from({length:500},(_,id)=>({...climates[id%10],id,name:`${climates[id%10].name} ${String(Math.floor(id/10)+1).padStart(2,'0')}`}));
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
export function weather(id,time=Date.now()){
  const s=stations[id];
  if(!s || !Number.isFinite(time)) throw new RangeError('Ambiente o orario non valido');
  const hours=time/3600000+s.offset, phase=id*2.399963;
  const day=Math.sin((hours-9)*Math.PI/12);
  const drift=Math.sin(hours/19+phase)*.65+Math.sin(hours/7.3+phase)*.35;
  const cloud=clamp(.45+.25*Math.sin(hours/4.8+phase)+.15*Math.sin(hours/13+phase),0,1);
  return {temperature:s.temp+s.swing*day+drift*2+Math.sin(phase)*3,
    humidity:clamp(s.humidity-day*7+cloud*8+drift*4,5,100),
    light:clamp(Math.max(0,Math.sin((hours-6)*Math.PI/12))*(1-cloud*.75),0,1),
    hour:((hours%24)+24)%24};
}
