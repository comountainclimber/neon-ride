import * as T from 'three';

export function practicals(scene,manifest,glyphs){
  const pose=new T.Object3D();
  const colors=[0x92edff,0xffbb80,0xff71a7,0x83ffe0];
  const lampItems=manifest.items.filter(([key])=>key.startsWith('lamp'));
  const lamps=new T.InstancedMesh(new T.BoxGeometry(.36,.15,.64),new T.MeshBasicMaterial(),lampItems.length);
  const haloGeo=new T.BufferGeometry(),xyz=[],rgb=[];
  lampItems.forEach(([,x,y,h,size],i)=>{
    pose.position.set(x,h+.90*size,y);pose.updateMatrix();lamps.setMatrixAt(i,pose.matrix);
    const col=new T.Color(colors[Math.floor(i/30)%4]).multiplyScalar(3.3);
    lamps.setColorAt(i,col);xyz.push(...pose.position.toArray());rgb.push(col.r,col.g,col.b);
  });
  scene.add(lamps);
  haloGeo.setAttribute('position',new T.Float32BufferAttribute(xyz,3));haloGeo.setAttribute('color',new T.Float32BufferAttribute(rgb,3));
  const halo=new T.ShaderMaterial({vertexColors:true,transparent:true,depthWrite:false,blending:T.AdditiveBlending,
    vertexShader:'varying vec3 tint; void main(){tint=color;vec4 p=modelViewMatrix*vec4(position,1.);gl_Position=projectionMatrix*p;gl_PointSize=clamp(740./max(1.,-p.z),1.,95.);}',
    fragmentShader:'varying vec3 tint; void main(){float r=length(gl_PointCoord-.5)*2.;float a=exp(-r*r*7.)*(1.-smoothstep(.6,1.,r));gl_FragColor=vec4(tint,a*.33);}'});
  scene.add(new T.Points(haloGeo,halo));
  const sectors=['RAINLINE','GLASS HEIGHTS','VIOLET REACH','LOWER RING'];
  const inks=['#a2ffe1','#ffac71','#fc81b2','#87cfff'];
  const materials=glyphs.map((glyph,i)=>{
    const c=document.createElement('canvas');c.width=320;c.height=720;
    const ctx=c.getContext('2d'),ink=inks[i%4];
    ctx.fillStyle='#081921';ctx.fillRect(0,0,c.width,c.height);ctx.strokeStyle=ink;ctx.lineWidth=7;ctx.strokeRect(8,8,304,704);
    ctx.fillStyle=ink;ctx.fillRect(20,21,280,54);ctx.fillStyle='#102027';ctx.font='bold 28px monospace';ctx.fillText('N R   /   0'+(i+1),32,58);
    for(let j=0;j<3;j++){
      const img=glyphs[(i+j)%glyphs.length].image;
      ctx.fillStyle=ink;ctx.fillRect(27,119+j*146,266,106);
      ctx.globalCompositeOperation='multiply';ctx.drawImage(img,27,119+j*146,266,106);ctx.globalCompositeOperation='source-over';
    }
    ctx.fillStyle=ink;ctx.font='bold 28px monospace';ctx.fillText(sectors[i%4],25,617,267);
    for(let k=0;k<26;k++)ctx.fillRect(27+k*10,645,2+(k*k)%5,35);
    const tex=new T.CanvasTexture(c);tex.colorSpace=T.SRGBColorSpace;tex.anisotropy=8;
    const m=new T.MeshBasicMaterial({map:tex,color:new T.Color(1.9,1.9,1.9),side:T.DoubleSide});return m;
  });
  const sheets=new T.PlaneGeometry(1,1),mastMat=new T.MeshStandardMaterial({color:0x233546,metalness:.58,roughness:.4});
  const signs=manifest.items.filter(([key])=>key.startsWith('sign'));
  const masts=new T.InstancedMesh(new T.BoxGeometry(.20,1,.26),mastMat,signs.length);
  signs.forEach(([,x,y,z,size,yaw],i)=>{
    const mesh=new T.Mesh(sheets,materials[i%materials.length]);mesh.position.set(x,z+size*1.30,y);mesh.rotation.y=-yaw;mesh.scale.set(size*.48,size*1.0,1);scene.add(mesh);
    pose.position.set(x,z+size*.70,y);pose.rotation.set(0,-yaw,0);pose.scale.set(1,size*1.4,1);pose.updateMatrix();masts.setMatrixAt(i,pose.matrix);
  });
  scene.add(masts);
}
