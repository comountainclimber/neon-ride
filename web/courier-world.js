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
    const vertices=[],indices=[],uv=[];
    for(const [s0,s1,l0,l1] of pieces){
      const steps=Math.max(1,Math.ceil((s1-s0)/2));
      const begin=vertices.length/3;
      for(let i=0;i<=steps;i++){
        const s=branchDistance(stop.s,s0+(s1-s0)*i/steps);
        for(const l of [l0,l1]){const f=circuit.frame(s,l);vertices.push(f.x,f.h+raise,f.z);uv.push((s0+(s1-s0)*i/steps)/3,l/3);}
        if(i){const a=begin+2*(i-1);indices.push(a,a+1,a+2,a+1,a+3,a+2);}
      }
    }
    const g=new T.BufferGeometry();g.setAttribute('position',new T.Float32BufferAttribute(vertices,3));g.setAttribute('uv',new T.Float32BufferAttribute(uv,2));g.setIndex(indices);g.computeVertexNormals();
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
    const plate=artTexture(512,230,(c,w,h)=>{
      c.fillStyle='#d4d1ad';c.fillRect(0,0,w,h);c.fillStyle='#233638';lettering(c,'FW  /  09',18,112,w-36,106,'900');c.fillRect(17,131,w-34,3);lettering(c,'HIGH PRESSURE   •   INLET',18,196,w-36,33);
    });
    for(const x of [-7.2,-2.2,2.8]){
      cylinder(g,plantTank,x,4.4,11.85,1.70,8.1);
      for(let y=1;y<=8;y+=1.4)cylinder(g,steel,x,y,11.85,1.77,.12);
      cylinder(g,accent,x,7.3,11.85,1.78,.10);
      cylinder(g,steel,x,9.1,12.0,.48,1.6);
      sign(g,plate,x,2.05,10.11,1.06,.48,PI,.88);
      part(g,tube,warm,x,.92,10.14,.37,.75,.37,0,0,PI/2);
      part(g,tube,shutter,x,.92,9.79,.48,.10,.48,0,0,PI/2);
      for(let i=0;i<8;i++){const a=2*PI*i/8;part(g,tube,trim,x+.39*Math.cos(a),.92+.39*Math.sin(a),9.728,.049,.041,.049,0,0,PI/2);}
    }
    cylinder(g,warm,-2,9.8,12,.38,11.2,PI/2);
    for(const x of [-8,3.3])cylinder(g,warm,x,7.75,12,.37,3.9);
    box(g,steel,7.1,2.45,9.48,5.2,3.35,.34);
    const consoleFace=artTexture(768,408,(c,w,h)=>{
      const grad=c.createLinearGradient(0,0,0,h);grad.addColorStop(0,'#526461');grad.addColorStop(1,'#273936');c.fillStyle=grad;c.fillRect(0,0,w,h);
      c.strokeStyle='#80918a';c.lineWidth=3;c.strokeRect(8,8,w-16,h-16);c.fillStyle='#c1cbb9';lettering(c,'PUMP CONTROL     /     INTAKE 09',24,53,w-46,29);
      for(let i=0;i<3;i++){const x=84+i*120,y=154;c.fillStyle='#233434';c.beginPath();c.arc(x,y,54,0,2*PI);c.fill();c.fillStyle='#bebba5';c.beginPath();c.arc(x,y,47,0,2*PI);c.fill();
        c.save();c.translate(x,y);c.strokeStyle='#3b4744';c.lineWidth=2;for(let j=0;j<14;j++){const a=PI*(.73+j*.113);c.beginPath();c.moveTo(34*Math.cos(a),34*Math.sin(a));c.lineTo(42*Math.cos(a),42*Math.sin(a));c.stroke();}
        c.rotate(-.9+i*.52);c.strokeStyle='#9e4329';c.lineWidth=4;c.beginPath();c.moveTo(0,7);c.lineTo(0,-33);c.stroke();c.restore();}
      c.fillStyle='#182d2d';c.fillRect(402,84,336,130);c.strokeStyle='#75a899';c.lineWidth=2;c.beginPath();for(let i=0;i<320;i++){const y=151+Math.sin(i/29)*20+Math.sin(i/9)*5;if(i)c.lineTo(408+i,y);else c.moveTo(408+i,y);}c.stroke();
      for(let k=0;k<9;k++){c.fillStyle='#203532';c.fillRect(27,241+k*14,287,4);}
      for(let i=0;i<12;i++){const x=364+(i%6)*65,y=263+Math.floor(i/6)*58;c.fillStyle='#1d292b';c.fillRect(x,y,43,26);c.fillStyle=i%4?'#91ad90':'#cc895b';c.fillRect(x+6,y+5,31,8);}
    });
    sign(g,consoleFace,7.1,2.46,9.299,4.9,2.65,PI,.72);
    for(const x of [5.0,5.3,8.8,9.12])cylinder(g,shutter,x,.41,9.4,.07,.82);
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
    const screen=sign(g,dawn,0,2.66,8.97,7.3,3.65,PI,1.50);screen.name='Dawn screening';screen.visible=false;dawnLights.push(screen);
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
  const deckTexture=artTexture(256,256,(c,w,h)=>{
    c.fillStyle='#6e7474';c.fillRect(0,0,w,h);
    for(let i=0;i<1560;i++){const x=(i*5897%1279)%w,y=(i*2381%1213)%h;c.fillStyle=i%3?'#52605c':'#89918a';c.fillRect(x,y,1+i%4,1);}
    c.fillStyle='#344149';c.fillRect(0,0,w,2);c.fillRect(0,0,2,h);c.fillStyle='#9a9f94';c.fillRect(0,2,w,1);c.fillRect(2,0,1,h);
    c.fillStyle='#475253';c.fillRect(163,9,77,23);c.fillStyle='#282f31';
    for(let i=0;i<14;i++)c.fillRect(168+i*5,13,2,15);
    for(const x of [7,249])for(const y of [7,249]){c.beginPath();c.arc(x,y,1.7,0,2*PI);c.fill();}
  });
  deckTexture.wrapS=deckTexture.wrapT=T.RepeatWrapping;
  const platformMat=new T.MeshStandardMaterial({map:deckTexture,bumpMap:deckTexture,bumpScale:.042,metalness:.36,roughness:.62});
  const tankSkin=artTexture(512,512,(c,w,h)=>{
    c.fillStyle='#819697';c.fillRect(0,0,w,h);for(let i=0;i<540;i++){let x=(i*383)%w,y=(i*71)%h;
      c.fillStyle=i%3?'#879a9b':'#748789';c.fillRect(x,y,1+i%3,5+i%37);}
    c.fillStyle='#66777a';c.fillRect(0,3,w,2);c.fillRect(4,0,2,h);
    c.fillStyle='#344f51';c.fillRect(0,244,w,14);c.fillStyle='#9ab8ac';c.fillRect(0,242,w,2);
    c.fillStyle='#234649';c.textAlign='center';lettering(c,'09',w*.5,196,205,129,'900');lettering(c,'FLOODWORKS',w*.5,224,222,21,'bold');
  });
  const plantTank=new T.MeshStandardMaterial({map:tankSkin,bumpMap:tankSkin,bumpScale:.027,roughness:.47,metalness:.56});
  const painted=artTexture(384,384,(c,w,h)=>{
    c.fillStyle='#a7afaf';c.fillRect(0,0,w,h);
    for(let i=0;i<1200;i++){const x=(i*367%1259)%w,y=(i*239%1543)%h;c.fillStyle=i%4?'#a0a8a9':'#8d9b9d';c.fillRect(x,y,i%3+1,i%17+1);}
    c.fillStyle='#687779';c.fillRect(0,0,w,2);c.fillRect(0,0,2,h);
    for(const x of [6,w-6])for(const y of [6,h-6]){c.beginPath();c.arc(x,y,2.2,0,PI*2);c.fill();}
  });painted.wrapS=painted.wrapT=T.RepeatWrapping;painted.repeat.set(3,2);
  for(const mat of [grayWindow,steel,black,dark]){mat.map=painted;mat.bumpMap=painted;mat.bumpScale=.024;}
  const contactTexture=artTexture(128,128,(c,w,h)=>{
    const fade=c.createRadialGradient(64,64,4,64,64,64);fade.addColorStop(0,'rgba(10,15,16,.85)');fade.addColorStop(.36,'rgba(10,15,16,.65)');fade.addColorStop(1,'rgba(10,15,16,0)');
    c.fillStyle=fade;c.fillRect(0,0,w,h);
  });
  const contactMat=new T.MeshBasicMaterial({map:contactTexture,transparent:true,opacity:.90,depthWrite:false,polygonOffset:true,polygonOffsetFactor:-1,polygonOffsetUnits:-1});
  function floorShadow(g,x,z,width,depth){
    const m=new T.Mesh(flat,contactMat);m.position.set(x,.011,z);m.rotation.x=-PI/2;m.scale.set(width,depth,1);m.renderOrder=2;g.add(m);
  }
  function platform(g,stop){
    const vertices=[],uv=[],index=[];
    // A level loading floor meets the continuously banked road through a short apron.
    const p=new T.Vector3();
    for(let i=0;i<=15;i++){const x=-15+2*i;
      const f=circuit.frame(branchDistance(stop.s,x),6.47);p.set(f.x,f.h-.034,f.z);g.worldToLocal(p);vertices.push(p.x,p.y,p.z);uv.push(x/3,6.47/3);
      for(const z of [7.45,16.89]){vertices.push(x,0,z);uv.push(x/3,z/3);}
      if(i){const a=3*(i-1);index.push(a,a+1,a+3,a+1,a+4,a+3,a+1,a+2,a+4,a+2,a+5,a+4);}
    }
    const geom=new T.BufferGeometry();geom.setAttribute('position',new T.Float32BufferAttribute(vertices,3));geom.setAttribute('uv',new T.Float32BufferAttribute(uv,2));geom.setIndex(index);geom.computeVertexNormals();g.add(new T.Mesh(geom,platformMat));
    for(let j=0;j<22;j++){const x=-13.8+j*1.28;box(g,j%2?warm:trim,x,.008,7.50,.71,.011,.079);}
    for(const z of [8.0,15.8]){for(const x of [-11.4,-7.4,0,7.4,11.4])floorShadow(g,x,z,2.3,1.2);}
    floorShadow(g,3.95,7.95,1.22,.75);floorShadow(g,3.95,7.95,.72,.38);
  }
  const workLight=new T.SpotLight(0xffe8c4,280,30,1.03,.72,1.7);
  const doorLight=new T.PointLight(0xffd5b5,24,15,1.8);scene.add(workLight,workLight.target,doorLight);
  for(const stop of network.stops){
    const theme=themes[stop.id%themes.length],g=anchor(stop.s),accent=glow(theme.color,1.72);
    g.position.y=circuit.frame(branchDistance(stop.s,3.95),7.95).h;g.updateMatrixWorld();platform(g,stop);
    const bayMat=new T.MeshStandardMaterial({color:theme.color,emissive:theme.color,emissiveIntensity:.16,roughness:.55,polygonOffset:true,polygonOffsetFactor:-2,polygonOffsetUnits:-2});
    const marks=[[-13,13,3.5,3.61],[-13,13,6.08,6.18],[-13,-12.85,3.5,6.18],[12.85,13,3.5,6.18]];
    for(const s of [-12,-10.3,-8.6,8.6,10.3,12])marks.push([s,s+.18,5.64,6.08]);
    paintStrip(stop,marks,bayMat);
    const bayStencil=artTexture(320,512,(ctx,w,h)=>{
      ctx.fillStyle='#cfdfd2';ctx.textAlign='center';lettering(ctx,'BAY',w*.5,129,w-40,78);
      lettering(ctx,String(stop.id+1).padStart(2,'0'),w*.5,366,w-30,204,'900');ctx.fillRect(29,405,w-58,11);
    });
    roadDecal(stop.s,4.83,1.7,6.4,bayStencil);
    box(g,base,0,-3.23,12.14,30,6.4,9.45);
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
    box(g,steel,4.35,1.44,9.45,.10,2.83,.11);
    box(g,black,4.35,2.11,9.44,1.9,.76,.095,-2.49);
    sign(g,tag,4.313,2.11,9.388,1.86,.72,-2.49,1.18);
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
