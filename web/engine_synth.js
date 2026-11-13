/* Procedural, phone-local V8 impression. No samples, network or microphone. */
(()=>{
'use strict';
const clamp=(n,lo,hi)=>Math.min(hi,Math.max(lo,n));
window.StormEngineSynth={create(context){
  const nodes=[],sources=[];
  const node=n=>{nodes.push(n);return n;};
  const oscillator=()=>{const n=node(context.createOscillator());sources.push(n);return n;};
  const gain=value=>{const n=node(context.createGain());n.gain.value=value;return n;};
  let started=false,stopped=false,lastGear=null,lastSample=-Infinity,firstSample=-Infinity;
  let volume=.3;
  const fire=oscillator(),rumble=oscillator(),flutter=oscillator();
  const fireGain=gain(.30),rumbleGain=gain(.08),noiseGain=gain(.012);
  const mix=gain(1),shift=gain(1),limiter=gain(1),limiterDepth=gain(0);
  const volumeGain=gain(volume*.65),safety=gain(0);
  const filter=node(context.createBiquadFilter());filter.type='lowpass';filter.frequency.value=650;filter.Q.value=.65;
  const compressor=node(context.createDynamicsCompressor());
  compressor.threshold.value=-14;compressor.knee.value=12;compressor.ratio.value=4;
  compressor.attack.value=.004;compressor.release.value=.09;

  // Four firing events/revolution for a four-stroke V8. Harmonics carry the
  // exhaust timbre; a softer half-rate component keeps idle from sounding pure.
  const real=new Float32Array(13),imag=new Float32Array(13);
  for(let i=1;i<imag.length;i++)imag[i]=(i%2?1:.67)/Math.pow(i,1.05);
  fire.setPeriodicWave(context.createPeriodicWave(real,imag));fire.frequency.value=53.333;
  rumble.type='sine';rumble.frequency.value=26.667;
  flutter.type='square';flutter.frequency.value=12;
  fire.connect(fireGain).connect(mix);rumble.connect(rumbleGain).connect(mix);
  const noise=node(context.createBufferSource());sources.push(noise);
  const buffer=context.createBuffer(1,Math.max(1,Math.floor(context.sampleRate)),context.sampleRate);
  const samples=buffer.getChannelData(0);let seed=0x7135,previous=0;
  for(let i=0;i<samples.length;i++){
    seed=(Math.imul(seed,1664525)+1013904223)>>>0;
    previous=.82*previous+.18*(seed/2147483648-1);samples[i]=previous;
  }
  noise.buffer=buffer;noise.loop=true;noise.connect(noiseGain).connect(mix);
  flutter.connect(limiterDepth).connect(limiter.gain);
  mix.connect(filter).connect(shift).connect(limiter).connect(volumeGain).connect(compressor).connect(safety).connect(context.destination);

  function target(param,value,now,seconds=.045){
    // cancelAndHoldAtTime is absent on some captive-portal WebKit versions.
    if(typeof param.cancelAndHoldAtTime==='function')param.cancelAndHoldAtTime(now);
    else {const current=param.value;param.cancelScheduledValues(now);param.setValueAtTime(current,now);}
    param.setTargetAtTime(value,now,seconds);
  }
  function freshGain(now){
    const age=now-lastSample;
    if(age<0||!Number.isFinite(age))return 0;
    if(age<.035)return age/.035;
    if(age<=.85)return 1;
    return clamp((1.15-age)/.30,0,1);
  }
  function armFreshness(now){
    const current=freshGain(now);
    safety.gain.cancelScheduledValues(now);safety.gain.setValueAtTime(current,now);
    safety.gain.linearRampToValueAtTime(1,now+.035);
    safety.gain.setValueAtTime(1,now+.85);
    safety.gain.linearRampToValueAtTime(0,now+1.15);
    lastSample=now;
  }
  function setVolume(value){
    if(stopped||!Number.isFinite(value))return;
    volume=clamp(value,0,1);target(volumeGain.gain,volume*.65,context.currentTime,.02);
    // Volume changes never refresh the sample watchdog.
  }
  function start(){
    if(stopped||started)return;
    started=true;for(const source of sources)source.start();
    // Starting a context alone must remain silent until a valid PWM snapshot.
  }
  function update(sample,nextVolume){
    if(stopped||!started||!sample||!Number.isFinite(sample.rpm)||
       !Number.isInteger(sample.gear)||sample.gear<-1||sample.gear>5||sample.rpm<0)return false;
    if(Number.isFinite(nextVolume))setVolume(nextVolume);
    const now=context.currentTime,rpm=clamp(sample.rpm,800,8000),load=(rpm-800)/7200;
    const pitch=rpm/60*4;
    if(firstSample===-Infinity){
      firstSample=now;
      fire.frequency.setValueAtTime(18,now);fire.frequency.linearRampToValueAtTime(pitch,now+.55);
      rumble.frequency.setValueAtTime(9,now);rumble.frequency.linearRampToValueAtTime(pitch*.5,now+.55);
    }else if(now-firstSample<.55){
      // Replace only the future crank target, retaining the current ramp value.
      for(const [param,value]of [[fire.frequency,pitch],[rumble.frequency,pitch*.5]]){
        if(typeof param.cancelAndHoldAtTime==='function')param.cancelAndHoldAtTime(now);
        else{const current=param.value;param.cancelScheduledValues(now);param.setValueAtTime(current,now);}
        param.linearRampToValueAtTime(value,firstSample+.55);
      }
    }else{
      target(fire.frequency,pitch,now);target(rumble.frequency,pitch*.5,now);
    }
    target(filter.frequency,650+load*2600,now,.07);
    target(fireGain.gain,.30+load*.10,now);target(noiseGain.gain,.012+load*.065,now);
    // Only forward-to-forward gear changes cut the sound; N/R transitions do
    // not invent a gearshift. RPM comes from the same scenic data as the TFT.
    if(lastGear>0&&sample.gear>0&&lastGear!==sample.gear){
      shift.gain.cancelScheduledValues(now);shift.gain.setValueAtTime(1,now);
      shift.gain.linearRampToValueAtTime(.24,now+.018);
      shift.gain.linearRampToValueAtTime(1,now+.16);
    }
    lastGear=sample.gear;
    const limiting=sample.gear===5&&rpm>=6950;
    target(limiter.gain,limiting?.52:1,now,.015);
    target(limiterDepth.gain,limiting?.48:0,now,.015);
    armFreshness(now);return true;
  }
  function stop(){
    if(stopped)return;stopped=true;
    safety.gain.cancelScheduledValues(context.currentTime);safety.gain.setValueAtTime(0,context.currentTime);
    for(const source of sources){try{source.stop()}catch(_){}}
    for(const n of nodes){try{n.disconnect()}catch(_){}}
  }
  return {start,update,setVolume,stop};
}};
})();
