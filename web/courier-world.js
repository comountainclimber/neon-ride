import * as T from 'three';
import {streetClients} from './courier-clients.js';

const themes=[
  {color:'#ffc57a',short:'REPAIR',mark:'R',service:'Workshop reception',role:'Motor & electrical'},
  {color:'#f38bbd',short:'PARCEL',mark:'P',service:'Parcel lockers',role:'Independent parcel exchange'},
  {color:'#63d5ed',short:'TIDE',mark:'09',service:'Plant receiving',role:'Municipal waterworks'},
  {color:'#f1af70',short:'CANAL',mark:'C',service:'Dock office',role:'Freight & forwarding'},
  {color:'#d79bf5',short:'AURORA',mark:'A',service:'Stage door',role:'Picture house'},
  {color:'#98e7ba',short:'GLASSHOUSE',mark:'+',service:'Clinic receiving',role:'Glasshouse health service'}
];
const PI=Math.PI;
const standard=(color,metalness=.46,roughness=.47)=>new T.MeshStandardMaterial({color,metalness,roughness});
const glow=(color,power=1)=>new T.MeshBasicMaterial({color:new T.Color(color).multiplyScalar(power)});
function artTexture(width,height,paint){
  const canvas=document.createElement('canvas');canvas.width=width;canvas.height=height;
  paint(canvas.getContext('2d'),width,height);
  const tex=new T.CanvasTexture(canvas);tex.colorSpace=T.SRGBColorSpace;tex.anisotropy=8;return tex;
}
function lettering(ctx,text,x,y,width,height,weight='bold'){
  ctx.font=weight+' '+height+'px "Arial Narrow",Arial,sans-serif';ctx.fillText(text,x,y,width);
}

export function courierWorld(scene,circuit,network,art,loader){
  if(!network.stops.length)return {update(){}};
  const steel=standard('#34444d',.67,.38),dark=standard('#232f37'),base=standard('#48505a',.23,.72);
  const shutter=standard('#657681',.72,.32),trim=standard('#a3b1ad',.74,.35),warm=standard('#946853',.55,.51);
  const glass=new T.MeshPhysicalMaterial({color:'#284d5a',metalness:.41,roughness:.16,clearcoat:.72});
  const grayWindow=standard('#43616c',.76,.22),plastic=standard('#334c4a',.12,.49),black=standard('#15232a');
  const outline=glow('#91b0a9',.67),activeInk=glow('#83ffcb',2.1);
  const cube=new T.BoxGeometry(),tube=new T.CylinderGeometry(1,1,1,20),globe=new T.SphereGeometry(1,12,8);
  const flat=new T.PlaneGeometry(),tmp=new T.Object3D(),batches=new Map(),stationStates=[],gates=[],dawnLights=[];
  function part(root,geom,mat,x,y,z,w,h,d,yaw=0,roll=0,pitch=0){
    tmp.position.set(x,y,z);tmp.scale.set(w,h,d);tmp.rotation.set(pitch,yaw,roll,'YXZ');tmp.updateMatrix();
    if(!batches.has(mat))batches.set(mat,new Map());
    const batch=batches.get(mat);if(!batch.has(geom))batch.set(geom,[]);
    batch.get(geom).push(new T.Matrix4().multiplyMatrices(root.matrixWorld,tmp.matrix));
  }
  const box=(root,mat,x,y,z,w,h,d,yaw=0,roll=0,pitch=0)=>part(root,cube,mat,x,y,z,w,h,d,yaw,roll,pitch);
  const cylinder=(root,mat,x,y,z,r,h,roll=0)=>part(root,tube,mat,x,y,z,r,h,r,0,roll);
  function anchor(encoded,lateral=0){
    const f=circuit.frame(encoded,lateral),group=new T.Group();
    group.position.set(f.x,f.h,f.z);group.rotation.y=-f.yaw;scene.add(group);group.updateMatrixWorld();return group;
  }
  function sign(root,texture,x,y,z,w,h,yaw,emission=1.05){
    const mat=new T.MeshBasicMaterial({map:texture,color:new T.Color(emission,emission,emission)});
    const m=new T.Mesh(flat,mat);m.position.set(x,y,z);m.scale.set(w,h,1);m.rotation.y=yaw;root.add(m);return m;
  }
  function stationSign(stop,theme){
    return artTexture(1536,396,(c,w,h)=>{
      const gradient=c.createLinearGradient(0,0,w,h);gradient.addColorStop(0,'#14252b');gradient.addColorStop(1,'#090f1b');c.fillStyle=gradient;c.fillRect(0,0,w,h);
      c.fillStyle=theme.color;c.fillRect(18,20,15,h-40);c.fillRect(52,32,210,6);
      lettering(c,'CITY DELIVERY   /   BAY '+String(stop.id+1).padStart(2,'0'),58,87,w-110,42);
      c.fillStyle='#eef7ee';lettering(c,stop.name.toUpperCase(),51,229,w-105,135,'900');
      c.fillStyle=theme.color;lettering(c,theme.role.toUpperCase(),58,340,w-150,49);
    });
  }
  function bladeTexture(stop,theme){
    return artTexture(448,640,(c,w,h)=>{
      c.fillStyle='#101820';c.fillRect(0,0,w,h);c.fillStyle=theme.color;c.fillRect(12,12,w-24,h-24);
      c.fillStyle='#101d25';lettering(c,theme.short,31,81,w-62,47);
      lettering(c,String(stop.id+1).padStart(2,'0'),22,335,w-42,271,'900');
      c.fillRect(31,382,w-62,4);lettering(c,'DELIVERY',33,465,w-67,52);lettering(c,'BAY   ↗',33,552,w-67,62);
    });
  }
  function branchDistance(s,offset){
    const r=network.roads.find(route=>s>=route.start-.001&&s<=route.start+route.length+.001);
    if(!r||r.id===0)return T.MathUtils.euclideanModulo(s+offset,circuit.length);
    return r.start+Math.max(0,Math.min(r.length-.00001,s-r.start+offset));
  }
  function paintStrip(stop,pieces,mat,raise=.027){
    const vertices=[],indices=[];
    for(const [s0,s1,l0,l1] of pieces){
      const steps=Math.max(1,Math.ceil((s1-s0)/2));
      const begin=vertices.length/3;
      for(let i=0;i<=steps;i++){
        const s=branchDistance(stop.s,s0+(s1-s0)*i/steps);
        for(const l of [l0,l1]){const f=circuit.frame(s,l);vertices.push(f.x,f.h+raise,f.z);}
        if(i){const a=begin+2*(i-1);indices.push(a,a+1,a+2,a+1,a+3,a+2);}
      }
    }
    const g=new T.BufferGeometry();g.setAttribute('position',new T.Float32BufferAttribute(vertices,3));g.setIndex(indices);g.computeVertexNormals();
    const m=new T.Mesh(g,mat);m.renderOrder=raise>0?2:0;scene.add(m);return m;
  }
  function roadDecal(s,offset,width,length,texture){
    const pts=[],uv=[],idx=[],steps=Math.ceil(length/2);
    for(let i=0;i<=steps;i++){
      const d=branchDistance(s,-length/2+length*i/steps);
      for(const [j,l] of [offset-width*.5,offset+width*.5].entries()){
        const f=circuit.frame(d,l);pts.push(f.x,f.h+.041,f.z);uv.push(j,i/steps);
      }
      if(i){const a=(i-1)*2;idx.push(a,a+1,a+2,a+1,a+3,a+2);}
    }
    const g=new T.BufferGeometry();g.setAttribute('position',new T.Float32BufferAttribute(pts,3));g.setAttribute('uv',new T.Float32BufferAttribute(uv,2));g.setIndex(idx);g.computeVertexNormals();
    const mat=new T.MeshStandardMaterial({map:texture,transparent:true,depthWrite:false,roughness:.61,polygonOffset:true,polygonOffsetFactor:-2,polygonOffsetUnits:-2});
    const mesh=new T.Mesh(g,mat);mesh.renderOrder=3;scene.add(mesh);
  }
  function arrowPaint(turn){
    return artTexture(256,512,(c,w,h)=>{
      c.fillStyle='#d1ded6';c.beginPath();
      if(turn){c.moveTo(73,482);c.lineTo(119,482);c.lineTo(119,258);c.lineTo(173,161);c.lineTo(215,209);c.lineTo(224,24);c.lineTo(112,103);c.lineTo(149,131);c.lineTo(73,240);}
      else{c.moveTo(103,482);c.lineTo(150,482);c.lineTo(150,196);c.lineTo(211,241);c.lineTo(127,24);c.lineTo(42,241);c.lineTo(103,196);}
      c.closePath();c.fill();
    });
  }
  const forkArrow=arrowPaint(true),straightArrow=arrowPaint(false);
  function workshop(g,accent,id){
    for(const x of [-5.3,2.5]){
      box(g,black,x,2.34,9.24,6.5,3.93,.18);
      for(let j=0;j<12;j++)box(g,shutter,x,.68+j*.289,9.10,6.16,.18,.115);
      box(g,accent,x,4.42,8.98,6.3,.09,.19);
    }
    for(const x of [-9.8,7.0]){box(g,warm,x,1.25,9.0,1.26,2.06,1.0);for(let i=0;i<5;i++)box(g,trim,x,.5+i*.29,8.47,.92,.055,.04);}
    for(const x of [-7,-3,1,5])cylinder(g,steel,x,7.4,13.9,.52,1.82);
  }
  function arcade(g,accent){
    for(let i=0;i<12;i++)for(let j=0;j<4;j++){
      const x=-9.8+i*1.66,y=.86+j*.80;
      box(g,(i+j)%7===0?warm:grayWindow,x,y,9.13,1.56,.72,.35);
      box(g,(i+j)%3===0?accent:trim,x+.51,y+.15,8.93,.24,.065,.04);
    }
    for(let i=0;i<8;i++)box(g,steel,-10.2+i*2.9,5.0,8.95,.13,.14,3.20,0,0,-.10);
    box(g,glass,0,5.04,8.91,22.6,.085,3.04,0,0,-.10);
  }
  function waterPlant(g,accent){
    for(const x of [-7.2,-2.2,2.8]){
      cylinder(g,shutter,x,4.4,11.85,1.70,8.1);
      for(let y=1;y<=8;y+=1.4)cylinder(g,steel,x,y,11.85,1.77,.12);
      cylinder(g,accent,x,7.3,11.85,1.78,.10);
      cylinder(g,steel,x,9.1,12.0,.48,1.6);
    }
    cylinder(g,warm,-2,9.8,12,.38,11.2,PI/2);
    for(const x of [-8,3.3])cylinder(g,warm,x,7.75,12,.37,3.9);
    box(g,glass,7.1,2.6,9.4,5.2,3.2,.18);
  }
  function exchange(g,accent){
    for(const x of [-9,8.5]){box(g,warm,x,4.9,10,.56,9.7,.65);box(g,steel,x,4.9,15,.54,9.7,.66);}
    box(g,warm,-.25,9.4,10.1,18.5,.42,.62);
    box(g,warm,-.25,8.3,10.1,18.5,.23,.52);
    for(let j=0;j<10;j++)box(g,warm,-8.15+j*1.75,8.86,10.1,2.03,.12,.27,0,j%2?.55:-.55);
    for(let i=0;i<5;i++){const x=-7+i*3;box(g,i%2?steel:warm,x,1.04,9.9,2.8,1.62,1.8);for(const d of [-1,1])box(g,trim,x+d,1.04,8.97,.09,1.53,.04);}
    box(g,accent,3.5,6.98,10.04,.52,.44,.58);cylinder(g,steel,3.5,7.66,10.05,.04,1.35);
  }
  function cinema(g,accent){
    const poster=artTexture(512,768,(c,w,h)=>{
      c.fillStyle='#14131b';c.fillRect(0,0,w,h);
      c.drawImage(art.cover.image,0,0,w,h*.75);
      const fade=c.createLinearGradient(0,h*.5,0,h*.83);fade.addColorStop(0,'#11141b00');fade.addColorStop(1,'#11141b');c.fillStyle=fade;c.fillRect(0,h*.48,w,h*.36);
      c.fillStyle='#d2ffe6';lettering(c,'NEON',22,566,466,114,'900');lettering(c,'RIDE',22,669,466,114,'900');
      c.fillStyle='#e0b0ea';lettering(c,'AURORA  /  SCREEN 01',23,730,465,28);
    });
    const dawn=artTexture(768,512,(c,w,h)=>{
      c.drawImage(art.cover.image,0,0,w,h);
      const wash=c.createLinearGradient(0,0,0,h);wash.addColorStop(0,'#482d6650');wash.addColorStop(.60,'#33233532');wash.addColorStop(1,'#191327');
      c.fillStyle=wash;c.fillRect(0,0,w,h);c.fillStyle='#ffc996';lettering(c,'AURORA   •   SCREEN 01',32,52,w-64,28);
      c.fillStyle='#fff4d8';lettering(c,'DAWN',28,390,w-56,118,'900');lettering(c,'SCREENING',32,460,w-64,65,'900');
    });
    for(const x of [-7.7,7.7]){box(g,black,x,2.75,9.2,3.65,4.72,.32);sign(g,poster,x,2.75,9.0,3.40,4.49,PI,1.34);}
    for(const x of [-3.8,-1.25,1.25,3.8])box(g,glass,x,2.23,9.15,2.30,3.72,.19);
    const screen=sign(g,dawn,0,2.66,8.97,7.3,3.65,PI,1.50);screen.visible=false;dawnLights.push(screen);
    const marquee=artTexture(1024,100,(c,w,h)=>{c.fillStyle='#381f32';c.fillRect(0,0,w,h);c.fillStyle='#ffda99';c.textAlign='center';lettering(c,'NOW SHOWING     /     DAWN SCREENING',w/2,71,w-37,61,'900');});
    const title=sign(g,marquee,0,5.22,7.482,16,.49,PI,1.72);title.visible=false;dawnLights.push(title);
    box(g,warm,0,5.22,8.8,22.5,.56,2.6);
    for(let j=0;j<29;j++)part(g,globe,glowBulb,-10.8+j*.77,5.16,7.46,.087,.087,.087);
    for(let i=0;i<3;i++)box(g,accent,0,5.64+i*.21,8.26,21.4-i*.35,.08,1.2);
  }
  function clinic(g,accent){
    for(let i=0;i<9;i++){const x=-9.8+i*2.43;
      box(g,glass,x,2.9,9.4,2.24,4.6,.21);box(g,trim,x-1.2,2.91,9.22,.075,4.7,.11);
      box(g,grayWindow,x,7.73,12.2,2.23,.14,4.22,0,0,-.44);
      if(i%2===0){box(g,plastic,x,.77,8.86,1.75,.70,.75);for(let k=0;k<4;k++)part(g,globe,foliage,x-.55+k*.36,1.24+(k%2)*.22,8.85,.35,.49,.28);}
    }
    box(g,accent,7.9,8.42,10.0,.61,2.16,.12);box(g,accent,7.9,8.42,9.99,2.16,.61,.13);
  }
  const foliage=standard('#486e5e',.05,.83),glowBulb=glow('#ffd49a',3.2);
  const builders=[workshop,arcade,waterPlant,exchange,cinema,clinic];
  function endWall(g,stop,theme,accent){
    const id=stop.id;
    const panel=artTexture(900,670,(ctx,w,h)=>{
      const backdrop=ctx.createLinearGradient(0,0,w,0);backdrop.addColorStop(0,'#18232c');backdrop.addColorStop(.65,'#28353c');backdrop.addColorStop(1,'#101c24');
      ctx.fillStyle=backdrop;ctx.fillRect(0,0,w,h);
      for(let i=0;i<290;i++){const x=(i*367)%w,y=(i*137)%h;ctx.strokeStyle='rgba(183,210,201,.08)';ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x+2,y+8+(i%15));ctx.stroke();}
      ctx.fillStyle=theme.color;ctx.fillRect(0,0,w,13);ctx.fillRect(30,35,110,6);
      lettering(ctx,theme.short,30,115,w-60,68,'900');
      const glyph=art.glyphs[id%art.glyphs.length].image;
      ctx.save();ctx.globalAlpha=.50;ctx.globalCompositeOperation='screen';ctx.drawImage(glyph,28,141,w-57,134);ctx.restore();
      ctx.fillStyle='#ecf2ed';lettering(ctx,theme.service.toUpperCase(),30,386,w-60,67);
      ctx.fillStyle=theme.color;lettering(ctx,stop.client.toUpperCase(),30,513,w-65,88,'900');
      for(let j=0;j<16;j++){ctx.fillStyle=j%3?'#8e9a91':'#35464a';ctx.fillRect(33+j*21,577,7+(j%2)*5,42);}
      ctx.fillStyle='#92aba4';lettering(ctx,'BAY  '+String(id+1).padStart(2,'0'),479,621,360,68);
    });
    box(g,steel,-11.45,3.25,13.75,.32,6.0,6.32);
    sign(g,panel,-11.64,3.60,13.68,5.87,4.40,-PI/2,1.11);
    for(const z of [10.53,16.9])box(g,trim,-11.64,3.15,z,.22,6.13,.13);
    for(const y of [1.17,5.97])box(g,accent,-11.68,y,13.69,.055,.065,6.02);
    for(let j=0;j<11;j++)box(g,shutter,-11.68,.65,10.78+j*.58,.12,.43,.42);
    if(id===0||id===2){for(const z of [15.9,16.3])cylinder(g,warm,-12.02,4.1,z,.125,7.93);}
    if(id===4){
      box(g,warm,-12.06,5.92,13.45,1.37,.37,7.14);
      for(let j=0;j<11;j++)part(g,globe,glowBulb,-12.77,5.88,10.34+j*.63,.072,.072,.072);
    }
  }
  const workLight=new T.SpotLight(0xffe8c4,280,30,1.03,.72,1.7);
  const doorLight=new T.PointLight(0xffd5b5,75,18,1.8);scene.add(workLight,workLight.target,doorLight);
  for(const stop of network.stops){
    const theme=themes[stop.id%themes.length],g=anchor(stop.s),accent=glow(theme.color,1.72);
    const platformMat=standard('#454d54',.36,.61);
    paintStrip(stop,[[-15,15,6.47,16.4]],platformMat,-.065);
    const bayMat=new T.MeshStandardMaterial({color:theme.color,emissive:theme.color,emissiveIntensity:.16,roughness:.55,polygonOffset:true,polygonOffsetFactor:-2,polygonOffsetUnits:-2});
    const marks=[[-13,13,3.5,3.61],[-13,13,6.08,6.18],[-13,-12.85,3.5,6.18],[12.85,13,3.5,6.18]];
    for(const s of [-12,-10.3,-8.6,8.6,10.3,12])marks.push([s,s+.18,5.64,6.08]);
    paintStrip(stop,marks,bayMat);
    const bayStencil=artTexture(320,512,(ctx,w,h)=>{
      ctx.fillStyle='#cfdfd2';ctx.textAlign='center';lettering(ctx,'BAY',w*.5,129,w-40,78);
      lettering(ctx,String(stop.id+1).padStart(2,'0'),w*.5,366,w-30,204,'900');ctx.fillRect(29,405,w-58,11);
    });
    roadDecal(stop.s,4.83,1.7,6.4,bayStencil);
    box(g,base,0,-3.25,12,30,6.4,9.7);
    box(g,dark,0,3.35,16.54,23,6.4,.42);box(g,dark,11.30,3.35,13.16,.43,6.4,7.28);
    if(stop.id!==2&&stop.id!==3)box(g,shutter,0,6.78,13,23.8,.35,7.8);
    else box(g,shutter,0,6.65,15.6,23.8,.3,2.13);
    endWall(g,stop,theme,accent);
    for(const x of [-11.0,11.0]){box(g,steel,x,3.36,9.32,.28,6.4,.36);box(g,accent,x,3.36,9.09,.045,5.93,.045);}
    builders[stop.id%builders.length](g,accent,stop.id);
    box(g,black,0,6.40,8.94,23,.13+2.38,.34);
    sign(g,stationSign(stop,theme),0,6.39,8.747,22.64,2.39,PI,1.26);
    box(g,accent,0,7.72,8.9,23.2,.09,.28);
    box(g,steel,-12.75,3.43,8.63,.29,6.75,.30);
    box(g,black,-12.75,5.18,9.50,.39,3.62,2.44);
    sign(g,bladeTexture(stop,theme),-12.97,5.18,9.50,2.36,3.47,-PI/2,1.30);
    box(g,steel,7.0,1.14,7.84,.84,2.04,.52);
    box(g,accent,7,1.37,7.55,.58,.46,.045);
    const tag=artTexture(640,248,(ctx,w,h)=>{
      ctx.fillStyle='#15252a';ctx.fillRect(0,0,w,h);ctx.fillStyle=theme.color;ctx.fillRect(0,0,9,h);
      lettering(ctx,'BAY '+String(stop.id+1).padStart(2,'0')+'    /    '+stop.name.toUpperCase(),26,71,w-52,49);
      ctx.fillStyle='#edf5eb';lettering(ctx,stop.client.toUpperCase(),24,196,w-48,99,'900');
    });
    box(g,steel,5.12,1.44,8.84,.10,2.83,.11);
    box(g,black,5.12,2.11,8.83,1.9,.76,.095,-2.49);
    sign(g,tag,5.083,2.11,8.778,1.86,.72,-2.49,1.18);
    const signal=new T.Mesh(new T.TorusGeometry(.48,.048,8,36),activeInk);
    signal.rotation.y=-PI/2;signal.position.set(-12.97,7.63,9.50);g.add(signal);
    stationStates.push({id:stop.id,theme,mat:bayMat,signal,root:g});
  }
  function routeSign(road,encoded,exit){
    const s=branchDistance(encoded,-28),g=anchor(s),t=themes[Math.min(5,road.id+1)];
    const tex=artTexture(1000,310,(c,w,h)=>{
      c.fillStyle='#122426';c.fillRect(0,0,w,h);c.strokeStyle=t.color;c.lineWidth=6;c.strokeRect(8,8,w-16,h-16);
      c.fillStyle=t.color;lettering(c,exit?'MERGE AHEAD':'RIGHT FORK',27,81,800,41);
      c.fillStyle='#effbf1';lettering(c,exit?'Midnight Circuit':road.name,24,208,855,92);
      lettering(c,exit?'↑':'↗',864,217,112,112);
    });
    box(g,steel,-.16,3.25,-9.67,.23,6.5,.24);
    box(g,black,0,6.15,-9.70,.26,2.02,6.04);
    sign(g,tex,-.145,6.15,-9.70,5.90,1.91,-PI/2,1.19);
  }
  for(const road of network.roads.filter(r=>r.id>0)){
    routeSign(road,road.entry,false);
    for(const d of [-61,-25]){
      roadDecal(branchDistance(road.entry,d),2.92,1.55,6.1,forkArrow);
      roadDecal(branchDistance(road.entry,d),-2.92,1.55,6.1,straightArrow);
    }
    if(road.id!==4)routeSign(road,road.start+road.length-.02,true);
    if(road.id!==4)continue;
    for(const direction of [1,-1]){
      let local=direction>0?18:road.length-18;
      for(let i=0;i<22 && network.adjacent(network.frame(road,local),road.id)<13;i++)local+=direction*4;
      const g=anchor(road.start+Math.min(road.length-.01,Math.max(.01,local)));
      box(g,steel,0,.95,6.88,.72,1.9,.68);
      const arm=new T.Group();arm.position.set(0,1.23,6.85);g.add(arm);
      const bar=new T.Mesh(new T.BoxGeometry(.25,.23,13.25),standard('#ae865e',.47,.4));bar.position.z=-6.68;arm.add(bar);
      for(let j=0;j<13;j++){const stripe=new T.Mesh(new T.BoxGeometry(.26,.24,.37),glow(j%2?'#ffae63':'#332a27',1.35));stripe.position.z=-.5-j;arm.add(stripe);}
      gates.push(arm);
    }
  }
  for(const [mat,maps] of batches)for(const [geom,transforms] of maps){
    const obj=new T.InstancedMesh(geom,mat,transforms.length);transforms.forEach((matrix,i)=>obj.setMatrixAt(i,matrix));
    obj.computeBoundingSphere();scene.add(obj);
  }
  let previous=null,rendered=false;const clients=streetClients(stationStates,loader);
  return {update(nav,time,worldPos){
    if(rendered)clients(time,worldPos,nav?.targetId);rendered=true;
    let nearest=stationStates[0],dist=Infinity;for(const station of stationStates){const d=station.root.position.distanceToSquared(worldPos);if(d<dist){dist=d;nearest=station;}}
    workLight.visible=doorLight.visible=dist<90*90;
    if(workLight.visible){const root=nearest.root;
      root.localToWorld(workLight.position.set(-15,6.8,7.8));root.localToWorld(workLight.target.position.set(-7.5,2.8,12));
      root.localToWorld(doorLight.position.set(3.12,3.25,6.95));doorLight.color.set('#fff0da').lerp(new T.Color(nearest.theme.color),.26);
    }
    const target=nav?.targetId??-1;
    if(previous!==target){
      previous=target;for(const state of stationStates){const active=state.id===target;state.signal.visible=active;state.mat.color.set(active?'#93ffcb':state.theme.color);
        state.mat.emissive.copy(state.mat.color);state.mat.emissiveIntensity=active?.48:.15;}
    }
    for(const state of stationStates)if(state.signal.visible)state.signal.scale.setScalar(1+.12*Math.sin(time*3.2));
    for(const gate of gates)gate.rotation.x=nav?.rampOpen?PI*.495:0;
    for(const light of dawnLights)light.visible=Boolean(nav?.campaignComplete);
  }};
}
