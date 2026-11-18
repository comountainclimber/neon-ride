import * as T from 'three';
import {Reflector} from './vendor/objects/Reflector.js';

export function createTrack(scene, circuit, [deck,asphalt,normals,roughness]) {
  const count=Math.ceil(circuit.length/4);
  function strip(points, material) {
    const xyz=[],uv=[],index=[];
    for(let i=0;i<=count;i++) {
      const s=i*circuit.length/count;
      points.forEach(([l,h])=>{
        const f=circuit.frame(s,l);xyz.push(f.x,f.h+h,f.z);uv.push(l,s);
      });
      for(let j=0;i && j<points.length-1;j++){
        const a=(i-1)*points.length+j,b=a+points.length;
        index.push(a,a+1,b,a+1,b+1,b);
      }
    }
    const g=new T.BufferGeometry();
    g.setAttribute('position',new T.Float32BufferAttribute(xyz,3));
    g.setAttribute('uv',new T.Float32BufferAttribute(uv,2));
    g.setIndex(index);g.computeVertexNormals();
    const m=new T.Mesh(g,material);scene.add(m);return m;
  }
  const mirror=new Reflector(new T.PlaneGeometry(1,1),{textureWidth:960,textureHeight:540,multisample:0,clipBias:.001});
  const projection={value:new T.Matrix4()}, clock={value:0};
  for(const t of [deck,asphalt,normals,roughness]){t.wrapS=t.wrapT=T.RepeatWrapping;t.repeat.set(.43,.43);}
  const roadMat=new T.MeshStandardMaterial({color:0xb6c1c8,map:asphalt,normalMap:normals,normalScale:new T.Vector2(.53,.53),roughnessMap:roughness,metalness:.18,roughness:.88});
  roadMat.onBeforeCompile=sh=>{
    Object.assign(sh.uniforms,{roadReflection:{value:mirror.getRenderTarget().texture},roadProjection:projection,roadTime:clock,roadGrime:{value:deck}});
    sh.vertexShader='varying vec2 trackUV; varying vec4 roadProjectionUV; uniform mat4 roadProjection;\n'+sh.vertexShader;
    sh.vertexShader=sh.vertexShader.replace('#include <uv_vertex>','#include <uv_vertex>\ntrackUV=uv; roadProjectionUV=roadProjection*modelMatrix*vec4(position,1.);');
    sh.fragmentShader='varying vec2 trackUV; varying vec4 roadProjectionUV; uniform sampler2D roadReflection; uniform float roadTime; uniform sampler2D roadGrime;\n'+sh.fragmentShader;
    sh.fragmentShader=sh.fragmentShader.replace('#include <opaque_fragment>',`
      float x=trackUV.x, s=trackUV.y;
      vec2 ref=roadProjectionUV.xy/roadProjectionUV.w;
      vec2 texNoise=texture2D(roadGrime, vMapUv*.42).rg;
      float ripple=sin(s*18.+roadTime*2.+x*3.)*.5+sin(x*31.-s*4.)*.5;
      vec2 offset=vec2((texNoise.x-.10)*.010+ripple*.001,(texNoise.y-.10)*.030);
      vec3 water=(texture2D(roadReflection,ref+offset).rgb*.5+texture2D(roadReflection,ref+offset+vec2(.0012,.0022)).rgb*.25+texture2D(roadReflection,ref+offset-vec2(.0012,.0022)).rgb*.25);
      float fresnel=1.-max(0.,dot(normalize(vViewPosition),normal));
      float puddle=smoothstep(.017,.22,texNoise.y+.10*sin(x*.7+s*.034));
      float inBounds=step(0.,ref.x)*step(ref.x,1.)*step(0.,ref.y)*step(ref.y,1.)*step(0.,roadProjectionUV.w);
      outgoingLight=mix(outgoingLight,water, inBounds*(.10+.47*fresnel)*(.21+.79*puddle));
      float lampDist=abs(mod(s-12.+16.,32.)-16.);
      float lampPool=exp(-lampDist*lampDist*.030)*exp(-pow((abs(x)-6.5)/4.,2.));
      vec3 poolColor=mix(vec3(.08,.33,.38),vec3(.48,.19,.10),smoothstep(-.18,.6,sin(s*.0061)));
      outgoingLight+=poolColor*lampPool*(.28+.72*texture2D(map,vMapUv).r);
      float halfLine=max(.018,fwidth(x)),edge=1.-smoothstep(.055-halfLine,.055+halfLine,abs(abs(x)-5.99));
      float center=(1.-smoothstep(.07-halfLine,.07+halfLine,abs(x)))*(1.-smoothstep(3.2-fwidth(s),3.2+fwidth(s),mod(s+1.,12.)));
      outgoingLight=mix(outgoingLight,vec3(.48,.56,.57),max(edge*.77,center*.82)*(.65+.35*texNoise.x));
      #include <opaque_fragment>`);
  };
  const road=strip([[-6.55,0],[0,0],[6.55,0]],roadMat);
  const steel=new T.MeshStandardMaterial({color:0x304353,metalness:.63,roughness:.43,side:T.DoubleSide});
  const concrete=new T.MeshStandardMaterial({color:0x344051,roughness:.68,side:T.DoubleSide});
  const rail=new T.MeshBasicMaterial({color:new T.Color(.14,.85,.82)});
  rail.onBeforeCompile=sh=>{
    sh.vertexShader='varying float railS;\n'+sh.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\nrailS=uv.y;');
    sh.fragmentShader='varying float railS;\n'+sh.fragmentShader.replace('#include <opaque_fragment>',`
      vec3 tint=mix(vec3(.12,1.55,1.32),vec3(1.24,.18,.42),smoothstep(.16,.75,sin(railS*.0041)));
      outgoingLight=tint*mix(.27,1.,1.-smoothstep(10.,12.,mod(railS,18.)));
      #include <opaque_fragment>`);
  };
  for(const side of [-1,1]){
    const profile=(pairs)=>pairs.map(([l,h])=>[side*l,h]);
    strip(profile([[6.55,-1.25],[6.55,.18],[6.88,.18],[6.88,-1.25]]),concrete);
    strip(profile([[6.77,.29],[6.77,.64],[6.94,.67],[6.94,.29]]),steel);
    const band=strip(profile([[6.765,.48],[6.765,.525]]),rail);band.material.side=T.DoubleSide;
  }
  const along=new T.Vector3(),across=new T.Vector3(),normal=new T.Vector3(),inv=new T.Matrix4(),z=new T.Vector3(0,0,1);
  return {
    reflect(renderer,camera,f,time) {
      clock.value=time;
      along.set(f.fx,Math.tan(f.grade),f.fz);across.set(f.rx,-Math.sin(f.bank),f.rz);
      normal.crossVectors(across,along).normalize();
      mirror.position.set(f.x,f.h-.035,f.z);mirror.quaternion.setFromUnitVectors(z,normal);mirror.updateMatrixWorld();
      camera.updateMatrixWorld();road.visible=false;
      mirror.onBeforeRender(renderer,scene,camera);road.visible=true;
      projection.value.multiplyMatrices(mirror.material.uniforms.textureMatrix.value,inv.copy(mirror.matrixWorld).invert());
    },
    resize(w,h){mirror.getRenderTarget().setSize(Math.min(1440,Math.round(w*.7)),Math.min(900,Math.round(h*.7)));}
  };
}
