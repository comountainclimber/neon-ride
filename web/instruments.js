import * as T from 'three';

export function instruments(bike){
  const canvas=document.createElement('canvas');canvas.width=640;canvas.height=224;
  const ctx=canvas.getContext('2d'),texture=new T.CanvasTexture(canvas);texture.colorSpace=T.SRGBColorSpace;
  texture.anisotropy=8;
  const panel=new T.Group();panel.position.set(.74,1.101,0);panel.rotation.set(-.57,-Math.PI/2,0,'YXZ');bike.add(panel);
  const rim=new T.Shape(),w=.164,h=.059,b=.009;
  rim.moveTo(-w+b,-h);rim.lineTo(w-b,-h);rim.lineTo(w,-h+b);rim.lineTo(w,h-b);
  rim.lineTo(w-b,h);rim.lineTo(-w+b,h);rim.lineTo(-w,h-b);rim.lineTo(-w,-h+b);rim.closePath();
  const casing=new T.Mesh(new T.ExtrudeGeometry(rim,{depth:.007,bevelEnabled:true,bevelSize:.002,bevelThickness:.002,bevelSegments:2}),
    new T.MeshStandardMaterial({color:0x182228,roughness:.36,metalness:.66}));panel.add(casing);
  const screen=new T.Mesh(new T.PlaneGeometry(.304,.102),new T.MeshBasicMaterial({map:texture,toneMapped:false}));screen.position.z=.010;panel.add(screen);
  const screwGeo=new T.CylinderGeometry(.0028,.0028,.0015,8),screwMat=new T.MeshStandardMaterial({color:0x67737a,metalness:.83,roughness:.3});
  for(const side of [-1,1]){const screw=new T.Mesh(screwGeo,screwMat);screw.position.set(side*.158,0,.009);screw.rotation.x=Math.PI*.5;panel.add(screw);}
  let prev='';
  return {panel,update(p){
    const speed=Math.round(3.6*p.v),charge=Math.round(p.charge*100),rpm=Math.round(p.rpm*18);
    const next=[speed,charge,rpm,p.gear,p.boost].join('/');if(next===prev)return;prev=next;
    const tint=p.boost?'#ffc281':'#98f2db';
    ctx.fillStyle='#061319';ctx.fillRect(0,0,640,224);
    for(let i=0;i<18;i++){
      ctx.fillStyle=i<rpm?(i>14?'#ffb287':'#70bfb2'):'#233239';ctx.fillRect(15+i*34,13,26,9);
    }
    ctx.fillStyle='#e4fff4';ctx.font='bold 108px monospace';ctx.fillText(String(speed).padStart(3,'0'),12,136);
    ctx.fillStyle='#88b5b1';ctx.font='bold 23px monospace';ctx.fillText('KM/H',26,170);
    ctx.fillStyle='#45626a';ctx.fillRect(250,44,2,111);ctx.fillRect(382,44,2,111);
    ctx.font='bold 22px monospace';ctx.fillStyle='#8cb7b6';ctx.fillText('GEAR',273,70);
    ctx.fillStyle='#dcf9e9';ctx.font='bold 64px monospace';ctx.fillText(String(p.gear),279,139);
    ctx.font='bold 25px monospace';ctx.fillStyle=tint;ctx.fillText('NITRO',401,70);
    ctx.font='bold 64px monospace';ctx.fillText(charge+'%',399,138);
    for(let i=0;i<16;i++){ctx.fillStyle=i<Math.ceil(p.charge*16)?tint:'#25363a';ctx.fillRect(15+i*38.3,184,31,23);}
    texture.needsUpdate=true;
  }};
}
