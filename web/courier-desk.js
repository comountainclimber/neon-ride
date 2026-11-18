import {createCityMap, stopColor} from './courier-map.js';

const $ = id => document.getElementById(id);
const meters = n => n < 1000 ? Math.max(0, Math.round(n / 5) * 5) + ' m' : (n / 1000).toFixed(1) + ' km';
const clock = seconds => Math.floor(seconds/60) + ':' + String(Math.floor(seconds%60)).padStart(2,'0');
const escape = value => String(value ?? '').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const option = (action,id,label) => '<button class="courier-action" data-do="'+action+'" data-id="'+id+'">'+escape(label)+'</button>';
const phase = ['Available','Reserved','On board','Delivered'];
const panel = $('dispatch');
const clientNotes = [
  ['Leave it on the lift. I’ll sign for it.','Your usual stand is clear, Vesper. Bring it round.'],
  ['Locker six. The shutter sticks, so give it a push.','Same locker, Vesper. I fixed that shutter for you.'],
  ['Good timing. Pump Nine has been on reserve all night.','I heard your bike from the sluice. Come on through.'],
  ['Pass it across the rail. I’ve got a boat to catch.','Back again? The last ferry waited for your parcel.'],
  ['I put your plate on the service gate. Take the ramp next time.','The booth lights are on for you. The service ramp is clear.'],
  ['Thank you. I’ll take it straight to the night ward.','Hello again, Vesper. The night ward remembers you.']
];
const campaign = [
  ['First Night','Inez has two parcels waiting at Rainline Repair.'],
  ['Regular Service','The night crews are calling you back. Sol has a dawn print on order.'],
  ['First Light','The city is bringing Aurora Cinema back for its dawn screening.']
];
const shiftTitle = number => campaign[number-1]?.[0] || 'Night service';
const closing = 'Aurora lit its screen at dawn. The city knows your name.';

export function createCourierDesk(circuit, hooks, preferences) {
  const network = circuit.courier;
  let manual = network.stops.some(s=>s.id===preferences?.waypoint) ? preferences.waypoint : -1;
  let selected = manual >= 0 ? manual : 0, tab='map', lastRefresh=-Infinity;
  let targetId=-1, near=-1, state=network.state(), lastFinished=state.finished;
  let lastClient=network.stops.some(s=>s.id===preferences?.lastClient)?preferences.lastClient:-1;
  const slots=[-1,-1].map((empty,i)=>Number.isInteger(preferences?.panniers?.[i])?preferences.panniers[i]:empty);
  const stop = id => network.stops.find(s=>s.id===id);
  const road = id => network.roads.find(r=>r.id===id);
  const campaignComplete = () => state.number>3||(state.number===3&&state.finished);
  const active = () => network.jobs.filter(j=>[1,2].includes(state.status[j.id]));
  const jobStop = job => state.status[job.id]===1 ? job.from : job.to;
  const actionable = () => active().filter(job=>jobStop(job)===near);
  function fitPanniers(){
    const ids=active().map(j=>j.id);
    for(let i=0;i<slots.length;i++){
      if(!ids.includes(slots[i])||slots.indexOf(slots[i])!==i)slots[i]=-1;
    }
    for(const id of ids){
      if(!slots.includes(id))slots[slots.indexOf(-1)]=id;
    }
  }
  const goal = () => {
    if(manual>=0)return manual;
    const runs=active().map(j=>({id:jobStop(j),meters:network.routeTo(jobStop(j)).meters,onboard:state.status[j.id]===2,
      urgent:state.status[j.id]===2&&j.windowSeconds>0}));
    runs.sort((a,b)=>Number(b.id===near)-Number(a.id===near)||Number(b.urgent)-Number(a.urgent)||Number(b.onboard)-Number(a.onboard)||a.meters-b.meters);
    return runs[0]?.id ?? -1;
  };
  const drawMap=createCityMap($('cityMap'),circuit,network,id=>{selected=id;render();});

  function guidance(ride,destination,route) {
    const current=road(route.via[0]);
    if(!current)return 'Follow this road';
    const onRoad=s=>s>=current.start&&s<=current.start+current.length;
    function ahead(s){
      const d=s-ride.s;
      return d>=0?d:current.id===0?d+current.length:Infinity;
    }
    const delta=Math.abs(destination.s-ride.s);
    const beside=onRoad(destination.s)&&(delta<30||(current.id===0&&current.length-delta<30));
    if(near===destination.id)return 'At the bay';
    if(beside||route.meters<65)return 'Brake below 10 km/h in the right bay';
    const forks=network.roads.filter(r=>r.id>0&&(r.id!==4||state.rampOpen)&&onRoad(r.entry))
      .map(r=>({road:r,distance:ahead(r.entry)})).filter(f=>f.distance<route.meters)
      .sort((a,b)=>a.distance-b.distance);
    if(forks.length){
      const fork=forks[0],chosen=fork.road.id===route.via[1];
      // Keeping right before an earlier optional fork would send the rider off route.
      return (chosen?'Take right for ':'Stay left past ')+fork.road.name+' · '+meters(fork.distance);
    }
    if(current.id!==0 && route.via.length>1)return 'Continue to '+road(route.via[1]).name;
    return 'Follow '+current.name;
  }

  function refresh(force=false){
    const ride=circuit.ride();
    if(!force && Math.abs(ride.elapsed-lastRefresh)<.14)return;
    lastRefresh=ride.elapsed;
    state=network.state();fitPanniers();near=ride.crashAge>=0?-1:network.nearby();targetId=goal();
    const destination=stop(targetId), route=destination?network.routeTo(targetId):null;
    const tasks=actionable(), handoff=tasks[0], orders=active();
    const aimed=orders.find(j=>jobStop(j)===targetId);
    const marked=destination && manual>=0 && !aimed;
    $('navPhase').textContent=state.finished?'Shift complete':aimed?(state.status[aimed.id]===1?'Pickup':'Delivery'):marked?'Waypoint':shiftTitle(state.number);
    $('area').textContent=destination?.name || stop(near)?.name || (circuit.courier.roads.find(r=>r.id!==0 && ride.s>=r.start && ride.s<r.start+r.length)?.name ?? 'Midnight Circuit');
    $('distance').textContent=route?meters(route.meters):'';
    $('navHint').textContent=destination?guidance(ride,destination,route):state.finished?'View your shift log':'Open dispatch to take a run';
    const carried=orders.filter(j=>state.status[j.id]===2);
    $('lap').textContent=carried.length+'/2 loaded';
    const timed=carried.find(j=>j.windowSeconds>0);
    if(timed){const remaining=timed.windowSeconds-(ride.elapsed-state.pickedAt[timed.id]);$('lap').textContent=remaining>0?'Due '+clock(remaining):'Window passed';}
    $('meter').style.width=(destination?Math.max(2,100*(1-Math.min(1,route.meters/1600))):0)+'%';
    const atBay=near>=0;
    $('bayAction').hidden=!atBay;
    if(atBay)$('bayAction').textContent=handoff?(state.status[handoff.id]===1?'Collect ':'Deliver ')+handoff.parcel:'Open bay dispatch';
    if(state.finished&&!lastFinished){manual=-1;tab='shift';hooks.open();render();}
    lastFinished=state.finished;
  }
  function open(view='map',chooseTarget=true){
    refresh(true);tab=view;
    if(chooseTarget)selected=targetId>=0?targetId:near>=0?near:selected;
    $('dispatchMessage').textContent='';hooks.open();render();
  }
  function nav(id){manual=id;selected=id;hooks.save();refresh(true);render();hooks.close();}
  function handoff(id){
    const job=network.jobs.find(j=>j.id===id);
    if(!job)return;
    if(network.nearby()!==jobStop(job)||!network.handoff(id)){
      $('dispatchMessage').textContent='Stop at the marked bay to exchange the parcel.';return;
    }
    const was=state.status[id],previousVisits=state.visits[job.to],oldRamp=state.rampOpen;
    if(was===2)lastClient=job.to;
    // The waypoint may have been set at pickup. Continue directly to delivery.
    if(manual===jobStop(job))manual=-1;
    refresh(true);hooks.save();
    let msg=(was===1?'Loaded ':'Delivered ')+job.parcel+'.';
    if(was===2){
      const client=stop(job.to).client;
      msg+=' '+(oldRamp!==state.rampOpen?'Sol opened the Aurora service ramp.':previousVisits>0?client+' welcomed you back.':client+' signed the receipt.');
    }
    $('dispatchMessage').textContent=msg;hooks.notify(msg);render();
    if(panel.open)panel.querySelector('[data-page]:not([hidden]) .courier-action')?.focus({preventScroll:true});
  }
  function interact(){
    refresh(true);if(near<0)return;
    const tasks=actionable();
    if(tasks.length===1){handoff(tasks[0].id);return;}
    open('jobs',false);
  }
  function panniers(){
    return '<div class="panniers">'+[0,1].map(i=>{
      const j=network.jobs.find(job=>job.id===slots[i]);return '<div><small>'+['Left pannier','Right pannier'][i]+'</small><strong>'+escape(j?.parcel||'Empty')+'</strong><span>'+escape(j?phase[state.status[j.id]]:'Available')+'</span></div>';
    }).join('')+'</div>';
  }
  function mapPage(){
    const address=stop(selected)||network.stops[0], visited=state.visits[address.id]||0;
    const route=network.routeTo(address.id);
    const addresses=network.stops.map(s=>'<button class="address-option" data-do="select" data-id="'+s.id+'" aria-pressed="'+(s.id===address.id)+'"><i style="--ink:'+stopColor(s.id)+'"></i>'+escape(s.name)+'</button>').join('');
    let guide=near===address.id?'Stopped at bay':meters(route.meters)+' by road';
    if(near!==address.id)guide+=' · '+escape(guidance(circuit.ride(),address,route));
    $('mapDetails').innerHTML='<div class="address-list">'+addresses+'</div><div class="address-detail"><small>'+escape(address.district)+'</small><h3>'+escape(address.name)+'</h3><p>'+escape(address.client)+(visited?' · Returning client':'')+'</p><div class="map-route">'+guide+'</div>'+option('navigate',address.id,'Ride to this address')+(manual>=0?'<button class="text-button" data-do="clear">Clear waypoint</button>':'')+'</div>';
    drawMap(circuit.ride(),address.id,address.id,state,route);
  }
  function jobRow(job){
    const n=state.status[job.id], from=stop(job.from),to=stop(job.to);
    let act='';
    if(n===0)act=active().length<2?option('accept',job.id,'Reserve run'):'<span class="job-tag">Panniers assigned</span>';
    if(n===1||n===2)act=jobStop(job)===near?option('handoff',job.id,n===1?'Collect parcel':'Hand over parcel'):option('navigate',jobStop(job),n===1?'Route to pickup':'Route to client');
    const timing=job.windowSeconds>0 ? (n===3?(state.onTime[job.id]?'On time':'After window'):'Express · '+clock(job.windowSeconds)+' from pickup'):'Flexible window';
    return '<article class="job-row"><div><small>'+escape(timing)+'</small><h4>'+escape(job.parcel)+'</h4><p>'+escape(from.name)+' <span>→</span> '+escape(to.name)+'</p></div>'+act+'</article>';
  }
  function jobsPage(){
    const orders=active();
    const offers=network.jobs.filter(j=>state.status[j.id]===0).sort((a,b)=>Number(b.from===near)-Number(a.from===near));
    const dock=stop(near), atBay=offers.filter(j=>j.from===near);
    const pickup=atBay.length?atBay.length+' available '+(atBay.length===1?'pickup':'pickups'):'No open pickups';
    const location=dock?'<p class="bay-location"><strong>'+escape(dock.name)+'</strong><span>'+escape(dock.client)+' · '+pickup+'</span></p>':'';
    const briefing=campaign[state.number-1]?.[1];
    $('jobDetails').innerHTML='<div class="shift-brief"><small>Shift '+String(state.number).padStart(2,'0')+'</small><h3>'+shiftTitle(state.number)+'</h3>'+(briefing?'<p>'+briefing+'</p>':'')+'</div>'+location+panniers()+(orders.length?'<h3 class="ledger-heading">Your runs</h3>'+orders.map(jobRow).join(''):'')+'<h3 class="ledger-heading">Dispatch board</h3>'+(offers.length?offers.map(jobRow).join(''):'<p class="muted">All runs have been assigned.</p>');
  }
  function shiftPage(){
    const delivered=network.jobs.filter(j=>state.status[j.id]===3);
    const clients=network.stops.filter(s=>state.visits[s.id]>0);
    const client=stop(lastClient),note=client&&state.visits[lastClient]?'<blockquote class="radio-note"><small>'+escape(client.client)+' / '+escape(client.name)+'</small><p>“'+escape(clientNotes[lastClient][Number(state.visits[lastClient]>1)])+'”</p></blockquote>':'';
    $('shiftDetails').innerHTML='<div class="shift-result"><small>Shift '+Number(state.number)+' · '+(state.finished?'Shift complete':'On the clock')+'</small><h3>'+shiftTitle(state.number)+'</h3><p>'+delivered.length+' of '+network.jobs.length+' runs delivered</p></div>'
      +(campaignComplete()?'<p class="campaign-ending">'+closing+'</p>':'')+(state.finished?option('next',0,state.number<3?'Start '+shiftTitle(state.number+1):'Start another shift'):'')+note+'<h3 class="ledger-heading">Client book</h3><div class="client-list">'+(clients.length?clients.map(s=>'<p><strong>'+escape(s.client)+'</strong><span>'+escape(s.name)+' · '+state.visits[s.id]+' delivered</span></p>').join(''):'<p class="muted">Your first delivery opens a client record.</p>')+'</div>'
      +'<div class="ramp-note">'+(state.rampOpen?'Cinema service ramp access granted.':'Deliver to Aurora Cinema to earn service ramp access.')+'</div>'
      +(delivered.length?'<h3 class="ledger-heading">Delivery receipts</h3>'+delivered.map(jobRow).join(''):'');
  }
  function render(){
    if(!panel.open)return;
    state=network.state();
    $('dutyStatus').textContent='Shift '+String(state.number).padStart(2,'0')+' / '+(state.finished?'Complete':'On duty');
    panel.querySelectorAll('[data-tab]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.tab===tab)));
    panel.querySelectorAll('[data-page]').forEach(p=>p.hidden=p.dataset.page!==tab);
    if(tab==='map')mapPage();else if(tab==='jobs')jobsPage();else shiftPage();
  }
  panel.addEventListener('click',event=>{
    const b=event.target.closest('button');if(!b)return;
    if(b.dataset.tab){tab=b.dataset.tab;$('dispatchMessage').textContent='';render();return;}
    const id=Number(b.dataset.id);
    switch(b.dataset.do){
      case 'select':selected=id;render();panel.querySelector('[data-do="select"][data-id="'+id+'"]')?.focus();break;
      case 'navigate':nav(id);break;
      case 'clear':manual=-1;refresh(true);hooks.save();render();break;
      case 'accept':if(network.accept(id)){
        manual=-1;refresh(true);hooks.save();render();
        const job=network.jobs.find(j=>j.id===id);
        $('dispatchMessage').textContent='Reserved. Collect at '+stop(job.from).name+'.';
        const follow=network.nearby()===job.from?'[data-do="handoff"][data-id="'+id+'"]':'[data-do="navigate"][data-id="'+job.from+'"]';
        $('jobDetails').querySelector(follow)?.focus({preventScroll:true});
      }break;
      case 'handoff':handoff(id);break;
      case 'next':if(network.nextShift()){
        manual=-1;lastFinished=false;refresh(true);hooks.save();tab='jobs';render();$('dispatchMessage').textContent='New shift opened.';
      }break;
    }
  });
  $('openDesk').onclick=()=>open();
  $('bayAction').onclick=interact;
  return {open,interact,refresh,preferences:()=>({waypoint:manual,panniers:slots.slice(),lastClient}),
    view:()=>({targetId,rampOpen:Boolean(state.rampOpen),campaignComplete:campaignComplete()})};
}
