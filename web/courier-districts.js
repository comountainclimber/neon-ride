import * as T from 'three';

// Canal cuts and dock aprons occupy the outside of each road. Decks stop at the
// shared collector lanes and at each address, where the receiving apron takes over.
export function districtWaterfronts(scene,network,extraInfill,glyphs){
  const uniformTime={value:0},tmp=new T.Object3D(),boxGeo=new T.BoxGeometry(),batches=new Map();
  const concrete=new T.MeshStandardMaterial({color:0x516074,roughness:.65,metalness:.26,side:T.DoubleSide});
  const coping=new T.MeshStandardMaterial({color:0x627f84,roughness:.41,metalness:.58,side:T.DoubleSide});
  const iron=new T.MeshStandardMaterial({color:0x3e4857,roughness:.51,metalness:.63,side:T.DoubleSide});
  const walkway=new T.MeshStandardMaterial({color:0x43535a,roughness:.54,metalness:.30,side:T.DoubleSide});
  const white=new T.MeshBasicMaterial({color:new T.Color(.30,.52,.50),side:T.DoubleSide});
  const orange=new T.MeshBasicMaterial({color:new T.Color(1.62,.44,.105),side:T.DoubleSide});
  const water=new T.MeshPhysicalMaterial({color:0x1c4c52,metalness:.27,roughness:.12,clearcoat:.85,clearcoatRoughness:.16,side:T.DoubleSide});
  water.onBeforeCompile=sh=>{
    sh.uniforms.canalTime=uniformTime;
    sh.vertexShader='varying vec2 canalXZ;\n'+sh.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\ncanalXZ=position.xz;');
    sh.fragmentShader='varying vec2 canalXZ;uniform float canalTime;\n'+sh.fragmentShader;
    sh.fragmentShader=sh.fragmentShader.replace('#include <normal_fragment_maps>',`#include <normal_fragment_maps>
      float band=sin(canalXZ.x*.38+canalXZ.y*.64+canalTime*1.1);
      float ripple=sin(canalXZ.y*3.23+canalXZ.x*.43+canalTime*2.+band)*.32+band*.68;
      vec3 waterWave=vec3(.19*ripple,1.,.11*cos(canalXZ.x*.77+canalTime*1.3)+.058*sin(canalXZ.y*5.8+canalTime*2.5));
      normal=normalize(mix(normal,mat3(viewMatrix)*normalize(waterWave),.71));`);
  };
  function strip(r,points,mat,begin,finish,allow=()=>true){
    const pos=[],idx=[],count=Math.ceil((finish-begin)/4);
    for(let i=0;i<=count;i++){
      const d=begin+(finish-begin)*i/count;
      for(const [l,y] of points){const f=network.frame(r,d,l);pos.push(f.x,f.h+y,f.z);}
      if(!i||!allow(d-(finish-begin)/(2*count)))continue;
      for(let j=0;j<points.length-1;j++){
        const a=(i-1)*points.length+j,b=a+points.length;idx.push(a,a+1,b,a+1,b+1,b);
      }
    }
    const geom=new T.BufferGeometry();geom.setAttribute('position',new T.Float32BufferAttribute(pos,3));geom.setIndex(idx);geom.computeVertexNormals();scene.add(new T.Mesh(geom,mat));
  }
  function box(f,mat,dy,w,h,depth,yawOffset=0,roll=0){
    tmp.position.set(f.x,f.h+dy,f.z);tmp.rotation.set(0,-f.yaw+yawOffset,roll,'YXZ');tmp.scale.set(w,h,depth);tmp.updateMatrix();
    if(!batches.has(mat))batches.set(mat,[]);batches.get(mat).push(tmp.matrix.clone());
  }
  for(const r of network.roads.filter(x=>x.id>0)){
    const begin=95,end=r.length-88;
    for(const side of [-1,1]){
      const available=d=>!network.stopOpening(r,r.start+d,side)&&network.adjacent(network.frame(r,d,side*8.5),r.id)>9;
      const to=r.id===1?11.6:r.id===3?10.8:8.25;
      const pts=[[6.91,-.22],[to,-.22],[to,-.94]].map(([l,y])=>[side*l,y]);strip(r,pts,walkway,begin,end,available);
      if(r.id===1)strip(r,[[side*(to-.22),-.205],[side*(to-.10),-.204]],white,begin,end,available);
    }
    if(r.id===2){
      strip(r,[[-42,-3.45],[-10.8,-3.45]],water,begin,end);
      for(const edge of [-42.3,-10.6]){
        strip(r,[[edge,-4.4],[edge,-.68],[edge+.35,-.68]],concrete,begin,end);
        strip(r,[[edge,-.67],[edge+.4,-.67]],coping,begin,end);
      }
      for(let d=begin+12;d<end;d+=48){
        const f=network.frame(r,d,-10.62);
        box(f,iron,-1.26,.23,2.4,.43);box(f,orange,-.42,.32,.14,.48);
        if(Math.floor(d/48)%3===0)for(let j=0;j<9;j++)box(network.frame(r,d,-11.02),coping,-.67-j*.31,.80,.087,.12);
      }
    }
    if(r.id===3){
      const crateMats=['#52787f','#705c64','#766b55'].map(color=>new T.MeshStandardMaterial({color,roughness:.57,metalness:.44}));
      for(let d=begin+25,i=0;d<end;d+=124,i++){
        const side=i%2?-1:1,free=!(network.stops.some(stop=>Math.abs(stop.s-r.start-d)<34));
        if(!free||[-12,-6,0,6,12].some(ds=>[10,14,18,22].some(l=>network.adjacent(network.frame(r,d+ds,side*l),r.id)<8)))continue;
        const f=network.frame(r,d,side*16);
        box(f,concrete,-.72,23,1.26,12);
        for(let j=0;j<3;j++){
          const c=network.frame(r,d-6.2+j*6.1,side*(j%2?16.9:18.9));
          box(c,crateMats[(i+j)%3],1.03,5.78,2.08,2.44);
          for(const o of [-2.5,-1.1,.3,1.7])box(network.frame(r,d-6.2+j*6.1+o,side*(j%2?16.9:18.9)),iron,1.06,.065,2.14,2.48);
        }
        box(network.frame(r,d,side*11.2),orange,.07,20.9,.07,.09);
      }
    }
    if(r.id===4)for(let d=begin;d<end;d+=41){
      for(const side of [-1,1]){const f=network.frame(r,d,side*7.55);if(network.adjacent(f,r.id)<10)continue;
        box(f,iron,-2.6,.48,5.2,.58);box(f,orange,.19,.47,.15,.33);}
    }
  }
  for(const [mat,poses] of batches){const mesh=new T.InstancedMesh(boxGeo,mat,poses.length);poses.forEach((p,i)=>mesh.setMatrixAt(i,p));mesh.computeBoundingSphere();scene.add(mesh);}
  const signGeometry=new T.PlaneGeometry();
  const signMats=glyphs.map((tex,i)=>new T.MeshBasicMaterial({map:tex,color:new T.Color([0xffb986,0x82dfee,0xe59add][i%3]).multiplyScalar(1.44)}));
  const signs=signMats.map(()=>[]);
  extraInfill.forEach(([x,z,base,w,d,h,yaw,seed],i)=>{
    if(i%2 || h<16)return;
    let near=null,best=70**2;
    for(const seg of network.segments){
      if(seg.road===0||seg.road===4)continue;
      const dist=network.distance(seg,x,z),ds=dist.dx**2+dist.dz**2;if(ds<best){near=seg.a;best=ds;}
    }
    if(!near)return;
    const side=Math.sign((x-near.x)*near.rx+(z-near.z)*near.rz);
    const inward=new T.Vector3(Math.sin(yaw)*side,0,-Math.cos(yaw)*side);
    tmp.position.set(x,Math.min(base+h-2,near.h+7+seed%8),z).addScaledVector(inward,d*.5+.12);
    tmp.rotation.set(0,(side>0?Math.PI:0)-yaw,0);tmp.scale.set(w*.74,Math.min(4.5,w*.21),1);tmp.updateMatrix();
    signs[seed%signMats.length].push(tmp.matrix.clone());
  });
  signs.forEach((poses,i)=>{if(poses.length){const mesh=new T.InstancedMesh(signGeometry,signMats[i],poses.length);poses.forEach((m,j)=>mesh.setMatrixAt(j,m));scene.add(mesh);}});
  return time=>{uniformTime.value=time;};
}
