import * as T from 'three';

export function instruments(bike){
  const canvas=document.createElement('canvas');canvas.width=768;canvas.height=252;
  const ctx=canvas.getContext('2d'),texture=new T.CanvasTexture(canvas);texture.colorSpace=T.SRGBColorSpace;
  texture.anisotropy=8;
  const panel=new T.Group();panel.position.set(.70,1.136,0);panel.rotation.set(-.57,-Math.PI/2,0,'YXZ');bike.add(panel);
  const frame=new T.Mesh(new T.BoxGeometry(.509,.185,.014),new T.MeshStandardMaterial({color:0x111b20,metalness:.57,roughness:.28}));panel.add(frame);
  const screen=new T.Mesh(new T.PlaneGeometry(.483,.158),new T.MeshBasicMaterial({map:texture,toneMapped:false}));screen.position.z=.009;panel.add(screen);
  let prev='';
  return {update(p){
    const speed=Math.round(3.6*p.v),charge=Math.round(p.charge*100),rpm=Math.round(p.rpm*20);
    const status=p.boost?'DISCHARGE':p.charge>=.995?'READY':p.cool>0?'COOLING':'CHARGING';
    const next=[speed,charge,rpm,p.gear,status].join('/');
    if(next===prev)return;prev=next;
    ctx.fillStyle='#07131a';ctx.fillRect(0,0,768,252);
    ctx.strokeStyle='#37525b';ctx.lineWidth=3;ctx.strokeRect(3,3,762,246);
    ctx.fillStyle='#87c0bd';ctx.font='bold 23px monospace';ctx.fillText('NR  /  TELEMETRY',22,38);
    for(let i=0;i<20;i++){
      ctx.fillStyle=i>=rpm?'#20353b':i>16?'#ffae6f':'#82ead2';
      ctx.fillRect(23+i*23,56,17,12);
    }
    ctx.fillStyle='#e8fff4';ctx.font='bold 102px monospace';ctx.fillText(String(speed).padStart(3,'0'),20,159);
    ctx.fillStyle='#80b9b8';ctx.font='bold 23px monospace';ctx.fillText('KM/H',213,152);
    ctx.font='bold 26px monospace';ctx.fillText('GEAR',361,109);
    ctx.fillStyle='#e1fff0';ctx.font='bold 53px monospace';ctx.fillText(String(p.gear),370,160);
    ctx.fillStyle='#263d45';ctx.fillRect(495,20,3,146);
    ctx.fillStyle=p.boost?'#ffb56d':'#91eed3';ctx.font='bold 25px monospace';ctx.fillText('NITRO',522,49);
    ctx.font='bold 67px monospace';ctx.fillText(charge+'%',516,121);
    ctx.fillStyle='#8bb5b1';ctx.font='bold 20px monospace';ctx.fillText(status,525,154);
    for(let i=0;i<32;i++){ctx.fillStyle=i<Math.ceil(p.charge*32)?(p.boost?'#ffad67':'#7ff1cc'):'#273f45';ctx.fillRect(22+i*22.75,189,18.5,38);}
    texture.needsUpdate=true;
  }};
}
