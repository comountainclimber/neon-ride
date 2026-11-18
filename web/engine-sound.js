const layerNames = ['idle','drive','redline','nitro'];

export class MotorcycleSound {
  constructor() {
    this.voices = [];
    this.enabled = false;
  }

  enable(on) {
    this.enabled = on;
    if (!on) {
      if (this.context) this.context.suspend().catch(()=>{});
      return;
    }
    const Audio = window.AudioContext || window.webkitAudioContext;
    if (!Audio) return;
    if (!this.context) {
      this.context = new Audio();
      this.master = this.context.createGain();
      this.master.gain.value = .93;
      this.master.connect(this.context.destination);
    }
    this.context.resume().catch(()=>{});
    if (!this.preparation) {
      this.preparation = this.loadLoops().catch(error=>{
        this.preparation = null;
        console.warn('Motorcycle audio could not load:',error);
      });
    }
  }

  async loadLoops() {
    const ctx = this.context;
    const buffers = await Promise.all(layerNames.map(async name=>{
      const response = await fetch('./assets/audio/engine-'+name+'.wav');
      if (!response.ok) throw new Error(name+' '+response.status);
      return ctx.decodeAudioData(await response.arrayBuffer());
    }));
    // Start all four loop clocks together; the throttle and gear changes only change the mix.
    const when = ctx.currentTime + .01;
    this.voices = buffers.map((buffer,i)=>{
      const source = ctx.createBufferSource(), amp = ctx.createGain();
      source.buffer=buffer; source.loop=true;
      amp.gain.value=i===0?.26:0;
      source.connect(amp); amp.connect(this.master); source.start(when);
      return {source,amp};
    });
  }

  update(ride, throttle, brake) {
    if (!this.enabled || this.voices.length!==4) return;
    const rev=Math.max(0,Math.min(1,(ride.rpm-.24)/.73));
    const load=brake>.05?0:throttle, cut=ride.crashAge>=0?.20:1;
    const gains=[.38*(1-rev)**2*cut, (.18+.42*load)*(.4+.6*Math.sin(Math.PI*rev))*cut,
      rev**1.6*(.15+.35*load)*cut, .36*ride.boost];
    const pitch=[.86+.66*rev,.78+.78*rev,.79+.48*rev,.9+.23*rev];
    const now=this.context.currentTime;
    for(let i=0;i<this.voices.length;i++){
      const {source,amp}=this.voices[i];
      amp.gain.setTargetAtTime(gains[i],now,.07);
      source.playbackRate.setTargetAtTime(pitch[i],now,.07);
    }
  }
}
