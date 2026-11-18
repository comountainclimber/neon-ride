import * as T from 'three';

export function lightShafts(scene,items){
  const lamps=items.filter(([name])=>name.startsWith('lamp')),positions=[],tints=[],heights=[];
  const colors=[0x95dfff,0xffb87c,0xff83b3,0x99ffdc];
  lamps.forEach(([,x,z,h,size],i)=>{
    positions.push(x,h+size*.90,z);heights.push(size*.91);
    tints.push(...new T.Color(colors[Math.floor(i/30)%4]).toArray());
  });
  const geom=new T.InstancedBufferGeometry().copy(new T.PlaneGeometry());
  geom.setAttribute('origin',new T.InstancedBufferAttribute(new Float32Array(positions),3));
  geom.setAttribute('tint',new T.InstancedBufferAttribute(new Float32Array(tints),3));
  geom.setAttribute('shaftHeight',new T.InstancedBufferAttribute(new Float32Array(heights),1));geom.instanceCount=lamps.length;
  const mat=new T.ShaderMaterial({transparent:true,depthWrite:false,blending:T.AdditiveBlending,side:T.DoubleSide,
    vertexShader:`attribute vec3 origin,tint;attribute float shaftHeight;varying vec2 coord;varying vec3 color;varying float strength;
      void main(){vec3 right=normalize(vec3(viewMatrix[0][0],0.,viewMatrix[2][0]));
      vec3 point=origin+right*position.x*8.;point.y-=(1.-uv.y)*shaftHeight;
      gl_Position=projectionMatrix*viewMatrix*vec4(point,1.);coord=uv;color=tint;
      strength=1.-smoothstep(55.,195.,distance(cameraPosition,origin));}`,
    fragmentShader:`varying vec2 coord;varying vec3 color;varying float strength;void main(){
      if(strength<=0. || any(lessThan(coord,vec2(0.))) || any(greaterThan(coord,vec2(1.))))discard;
      float down=clamp(1.-coord.y,0.,1.);
      float width=.021+.30*sqrt(down);
      float x=(coord.x-.5)/width;
      float light=exp(-min(80.,x*x*2.9))*exp(-3.2*down)*smoothstep(0.,.048,down)*smoothstep(0.,.20,coord.y);
      gl_FragColor=vec4(color,light*strength*.128);}`});
  const mesh=new T.Mesh(geom,mat);mesh.frustumCulled=false;mesh.renderOrder=2;scene.add(mesh);
}

export function districtHaze(material){
  if(!material.isMeshStandardMaterial || material.userData.districtFog)return;
  material.userData.districtFog=true;
  material.onBeforeCompile=sh=>{
    sh.vertexShader='varying float districtHeight;\n'+sh.vertexShader.replace('#include <project_vertex>',`
      vec4 fogVertex=vec4(transformed,1.);
      #ifdef USE_INSTANCING
      fogVertex=instanceMatrix*fogVertex;
      #endif
      districtHeight=(modelMatrix*fogVertex).y;
      #include <project_vertex>`);
    sh.fragmentShader='varying float districtHeight;\n'+sh.fragmentShader.replace('#include <fog_fragment>',`
      #ifdef USE_FOG
      float below=1.-smoothstep(20.,145.,districtHeight);
      float mist=1.-exp(-pow(max(0.,vFogDepth)*(.00115+.00072*below),1.54));
      vec3 glow=mix(fogColor*vec3(.59,.70,1.02),fogColor*vec3(1.08,1.15,1.13),below);
      gl_FragColor.rgb=mix(gl_FragColor.rgb,glow,clamp(mist,0.,.985));
      #endif`);
  };
  material.customProgramCacheKey=()=> 'neon-district-height-haze-v1';
}
