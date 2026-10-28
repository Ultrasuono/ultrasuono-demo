export function climate(temperature,humidity,light=80){
  const t=Math.max(-20,Math.min(40,temperature));
  return {
    color:t<=10?['#65caff','#3282bc',(t+20)/30]:t<=20?['#3282bc','#ad571d',(t-10)/10]:['#ad571d','#ff2105',(t-20)/20],
    speed:.08+1.4*(t+20)/60+4.3*Math.max(0,(t-20)/20)**2,
    trail:Math.max(0,Math.min(1,(humidity-70)/30))*.1,
    frost:Math.max(0,Math.min(1,(12-t)/32)),
    fullness:.12+.88*Math.max(0,Math.min(1,light/80)),
    glow:Math.max(0,Math.min(1,(light-80)/20))**2
  };
}
