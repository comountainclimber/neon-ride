const colors = ['#ffc57a', '#f38bbd', '#63d5ed', '#f1af70', '#d79bf5', '#98e7ba'];
export const stopColor = id => colors[id] || '#d1e3de';

// Draw the same centerlines used by the bike simulation. Local streets occupy
// their own distance intervals in the shared road graph.
export function createCityMap(canvas, circuit, network, selectStop) {
  const ctx = canvas.getContext('2d');
  const streets = network.roads.map(road => {
    const points = [];
    const count = Math.max(8, Math.ceil(road.length / 14));
    for (let i = 0; i <= count; i++) {
      const s = road.start + Math.min(road.length - .05, road.length * i / count);
      points.push(circuit.frame(s));
    }
    return {road, points};
  });
  const points = streets.flatMap(street => street.points);
  const xs = points.map(p => p.x), zs = points.map(p => p.z);
  const extent = [Math.min(...xs), Math.min(...zs), Math.max(...xs), Math.max(...zs)];
  let markers = [], latest;
  function draw(ride, target, selected, state, route) {
    latest = [ride, target, selected, state, route];
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (!w || !h) return;
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.round(w * ratio); canvas.height = Math.round(h * ratio);
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    const scale = Math.min((w-64)/(extent[2]-extent[0] || 1), (h-46)/(extent[3]-extent[1] || 1));
    function project(p) {
      return [w/2 + (p.x-(extent[0]+extent[2])/2)*scale, h/2 + (p.z-(extent[1]+extent[3])/2)*scale];
    }
    const path = coords => {
      ctx.beginPath();
      coords.forEach(([x,y],i) => i ? ctx.lineTo(x,y) : ctx.moveTo(x,y));
    };
    ctx.lineWidth = 1;ctx.strokeStyle = '#2a3c3c';
    for(let x=18;x<w;x+=32){path([[x,0],[x,h]]);ctx.stroke();}
    for(let y=14;y<h;y+=32){path([[0,y],[w,y]]);ctx.stroke();}
    ctx.lineJoin='round';ctx.lineCap='round';
    for(const {road,points} of streets){
      const locked = road.id===4 && !state.rampOpen;
      path(points.map(project));
      ctx.lineWidth=road.id===0?7:5;ctx.strokeStyle='#142121';ctx.stroke();
      ctx.lineWidth=road.id===0?3:2;ctx.strokeStyle=locked?'#667370':'#adc1b9';
      ctx.setLineDash(locked?[3,5]:[]);ctx.stroke();ctx.setLineDash([]);
    }
    // Trace only the player-to-stop segment of each road on the selected route.
    const end = network.stops.find(stop=>stop.id===target);
    if(end&&route?.meters>=1&&route?.via?.length){
      ctx.strokeStyle='#bbf7d7';ctx.lineWidth=3;ctx.setLineDash([4,5]);
      route.via.forEach((id,index)=>{
        const street=streets.find(item=>item.road.id===id);
        if(!street)return;
        const next=streets.find(item=>item.road.id===route.via[index+1]);
        const prev=streets.find(item=>item.road.id===route.via[index-1]);
        const lo=street.road.start, hi=lo+street.road.length;
        const onThisRoad=position=>Number.isFinite(position)&&position>=lo&&position<=hi;
        const entry=index===0?ride.s:(onThisRoad(prev?.road.exit)?prev.road.exit:lo);
        const exit=index===route.via.length-1?end.s:(onThisRoad(next?.road.entry)?next.road.entry:hi-.05);
        if(!Number.isFinite(entry)||!Number.isFinite(exit))return;
        const segments=exit>=entry?[[entry,exit]]:[[entry,street.road.start+street.road.length-.05],[street.road.start,exit]];
        for(const [a,b] of segments){
          if(b<a||b-a>street.road.length+1)continue;
          const coords=[],count=Math.max(1,Math.ceil((b-a)/12));
          for(let i=0;i<=count;i++)coords.push(project(circuit.frame(a+(b-a)*i/count)));
          path(coords);ctx.stroke();
        }
      });
      ctx.setLineDash([]);
    }
    markers=network.stops.map(stop=>{
      const [x,y]=project(circuit.frame(stop.s));
      ctx.beginPath();ctx.arc(x,y,stop.id===selected?9:6,0,2*Math.PI);
      ctx.fillStyle=stopColor(stop.id);ctx.fill();
      ctx.lineWidth=2;ctx.strokeStyle='#112021';ctx.stroke();
      if(stop.id===target){ctx.beginPath();ctx.arc(x,y,12,0,2*Math.PI);ctx.strokeStyle='#c5fddb';ctx.stroke();}
      return {id:stop.id,x,y};
    });
    const rider=circuit.frame(ride.s,ride.lateral||0), [x,y]=project(rider);
    const direction=Math.atan2(rider.fz,rider.fx);
    ctx.save();ctx.translate(x,y);ctx.rotate(direction);
    path([[9,0],[-5,-5],[-3,0],[-5,5],[9,0]]);ctx.fillStyle='#fff';ctx.fill();
    ctx.lineWidth=1.5;ctx.strokeStyle='#091713';ctx.stroke();ctx.restore();
  }
  canvas.addEventListener('click',event=>{
    const rect=canvas.getBoundingClientRect();
    const nearest=markers.map(p=>({...p,d:Math.hypot(p.x-event.clientX+rect.left,p.y-event.clientY+rect.top)})).sort((a,b)=>a.d-b.d)[0];
    if(nearest && nearest.d<25)selectStop(nearest.id);
  });
  new ResizeObserver(()=>{if(latest && canvas.clientWidth)draw(...latest);}).observe(canvas);
  return draw;
}
