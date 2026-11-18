import * as T from 'three';

// Floors are shaded in metres so broad and narrow buildings retain the same window scale.
export function cityInfill(scene, buildings, atlas){
  atlas.wrapS=atlas.wrapT=T.RepeatWrapping;
  const facade=new T.MeshStandardMaterial({color:0x344958,metalness:.63,roughness:.31});
  const geometry=new T.BoxGeometry(),seeds=new Float32Array(buildings.length);
  facade.onBeforeCompile=shader=>{
    shader.uniforms.officeAtlas={value:atlas};
    shader.vertexShader='attribute float buildingSeed; varying vec2 officeUV; varying float officeSeed; varying float roofWeight;\n'+shader.vertexShader;
    shader.vertexShader=shader.vertexShader.replace('#include <begin_vertex>',`#include <begin_vertex>
      vec3 size=vec3(length(instanceMatrix[0].xyz),length(instanceMatrix[1].xyz),length(instanceMatrix[2].xyz));
      vec3 meters=(position+.5)*size;
      officeUV=vec2(abs(normal.z)>.5?meters.x:meters.z,meters.y);
      officeSeed=buildingSeed+dot(normal,vec3(31.,52.,73.));roofWeight=abs(normal.y);`);
    shader.fragmentShader=`varying vec2 officeUV;varying float officeSeed;varying float roofWeight;
      uniform sampler2D officeAtlas;
      float officeRand(vec2 p){vec3 v=fract(vec3(p.xyx)*.1031);v+=dot(v,v.yzx+33.33);return fract((v.x+v.y)*v.z);}
      `+shader.fragmentShader;
    shader.fragmentShader=shader.fragmentShader.replace('#include <opaque_fragment>',`
      float facadeId=floor(officeSeed+.5);
      vec2 units=officeUV/vec2(2.07,3.16), bay=floor(units), st=fract(units);
      vec2 fw=clamp(fwidth(units),.002,.17);
      vec2 a=smoothstep(vec2(.055,.20)-fw,vec2(.055,.20)+fw,st);
      vec2 b=1.-smoothstep(vec2(.954,.89)-fw,vec2(.954,.89)+fw,st);
      float glass=a.x*a.y*b.x*b.y*(1.-roofWeight);
      float zone=officeRand(vec2(floor(bay.x/3.),bay.y+facadeId));
      float room=officeRand(bay+facadeId);
      float shift=officeRand(vec2(facadeId,0.));
      float working=smoothstep(.50,.76,.64*zone+.36*room+shift*.11);
      vec3 lamp=mix(vec3(.50,.84,1.05),vec3(1.9,1.03,.43),smoothstep(.23,.71,zone*.58+shift*.42));
      vec3 film=texture2D(officeAtlas,officeUV/vec2(8.3,12.64)).rgb;
      float blinds=step(.74,room)*smoothstep(.24,.98,st.y)*(1.-smoothstep(.30,.72,fract(st.y*7.)));
      float mullionShade=1.-.23*(1.-smoothstep(.02,.05+fw.x,abs(st.x-fract(zone*7.))));
      float ledge=smoothstep(.32,.42,st.y);
      float inner=(.085+.49*pow(st.y,1.7)+.51*film.g)*(1.-blinds*.60)*mullionShade*(.63+.37*ledge);
      float trim=1.-smoothstep(.009,.035,abs(st.y-.15));
      outgoingLight=outgoingLight*(.48+.48*glass)+glass*lamp*working*inner;
      outgoingLight+=trim*(1.-roofWeight)*vec3(.019,.030,.036);
      #include <opaque_fragment>`);
  };
  const main=new T.InstancedMesh(geometry,facade,buildings.length),tmp=new T.Object3D();
  const tops=new T.InstancedMesh(new T.BoxGeometry(),new T.MeshStandardMaterial({color:0x2f3944,roughness:.57,metalness:.43}),buildings.length);
  const rim=new T.InstancedMesh(new T.BoxGeometry(),new T.MeshStandardMaterial({color:0x526075,roughness:.38,metalness:.55}),buildings.length);
  const beacon=new T.InstancedMesh(new T.SphereGeometry(.18,7,5),new T.MeshBasicMaterial({color:new T.Color(2.9,.16,.056)}),buildings.length);
  buildings.forEach(([x,y,base,w,d,h,yaw,seed,type],i)=>{
    tmp.rotation.y=-yaw;
    tmp.position.set(x,base+h/2,y);tmp.scale.set(w,h,d);tmp.updateMatrix();main.setMatrixAt(i,tmp.matrix);seeds[i]=seed;
    tmp.position.y=base+h;tmp.scale.set(w+.36,.36,d+.36);tmp.updateMatrix();rim.setMatrixAt(i,tmp.matrix);
    const top=type?2.1+type*1.9:.8;
    tmp.position.y=base+h+top/2;tmp.scale.set(w*.64,top,d*.65);tmp.updateMatrix();tops.setMatrixAt(i,tmp.matrix);
    tmp.position.set(x+w*.15,base+h+top+.22,y);tmp.scale.setScalar(1);tmp.updateMatrix();beacon.setMatrixAt(i,tmp.matrix);
  });
  geometry.setAttribute('buildingSeed',new T.InstancedBufferAttribute(seeds,1));
  scene.add(main,tops,rim,beacon);
}

// The nearest real lamp fixtures illuminate the bodywork as it passes underneath them.
export function streetLighting(scene,items){
  const colors=[0x9adbff,0xffb46a,0xff639c,0x81f6da];
  const lamps=items.filter(([name])=>name.startsWith('lamp')).map(([,x,z,h,size],i)=>({
    p:new T.Vector3(x,h+size*.90,z),color:colors[Math.floor(i/30)%4]
  }));
  const active=Array.from({length:4},()=>{
    const l=new T.PointLight(0xffffff,176,39,2);scene.add(l);return l;
  });
  let last=new T.Vector3(Infinity,0,0);
  return (eye)=>{
    if(last.distanceToSquared(eye)<12)return;last.copy(eye);
    const near=lamps.map(l=>({l,d:l.p.distanceToSquared(eye)})).sort((a,b)=>a.d-b.d);
    active.forEach((light,i)=>{light.position.copy(near[i].l.p);light.color.setHex(near[i].l.color);});
  };
}
