import * as T from 'three';
import {Reflector} from './vendor/objects/Reflector.js';

// Each impact lives inside one cell. Its outer crest fades before it reaches a cell edge.
const rainSurfaceShader=`
  vec3 rainSeed(vec3 cell) {
    vec3 h=fract(cell*vec3(.1031,.1030,.0973));
    h+=dot(h,h.yxz+33.33);
    return fract((h.xxy+h.yzz)*h.zyx);
  }
  vec2 rainImpact(vec2 metres,float time,float pixelWidth,float spacing,float salt) {
    vec2 grid=metres/spacing;
    vec2 cell=floor(grid);
    vec3 schedule=rainSeed(vec3(cell,salt));
    float cycle=time*mix(.76,1.44,schedule.x)+schedule.y;
    float age=fract(cycle);
    vec3 seed=rainSeed(vec3(cell,mod(floor(cycle),2048.)+salt));
    vec2 displacement=(fract(grid)-mix(vec2(.29),vec2(.71),seed.xy))*spacing;
    float distanceToDrop=length(displacement);
    float radius=spacing*(.019+age*.218);
    float width=.016+pixelWidth*.34;
    float rim=(distanceToDrop-radius)/width;
    float wake=(distanceToDrop-radius+.053)/width;
    float slope=rim*exp(-rim*rim)-.46*wake*exp(-wake*wake);
    float envelope=smoothstep(0.,.07,age)*(1.-smoothstep(.36,1.,age))*mix(.65,1.,seed.z);
    return displacement/max(.001,distanceToDrop)*slope*envelope;
  }`;

export function createTrack(scene, circuit, [deck,asphalt,normals,roughness], network) {
  function strip(points, material, asphaltSurface=false) {
    const xyz=[],uv=[],mark=[],clearance=[],index=[];
    function vertex(x,y,z,u,v,paint,clear) {
      xyz.push(x,y,z);uv.push(u,v);mark.push(paint);clearance.push(clear);return mark.length-1;
    }
    function intersection(a,b){
      const t=clearance[a]/(clearance[a]-clearance[b]),mix=(data,n,k)=>T.MathUtils.lerp(data[a*n+k],data[b*n+k],t);
      return vertex(mix(xyz,3,0),mix(xyz,3,1),mix(xyz,3,2),mix(uv,2,0),mix(uv,2,1),mix(mark,1,0),0);
    }
    function triangle(a,b,c){
      if(clearance[a]>=0&&clearance[b]>=0&&clearance[c]>=0){index.push(a,b,c);return;}
      if(clearance[a]<0&&clearance[b]<0&&clearance[c]<0)return;
      const ids=[a,b,c],poly=[];
      for(let j=0;j<3;j++){
        const curr=ids[j],next=ids[(j+1)%3];
        if(clearance[curr]>=0)poly.push(curr);
        if((clearance[curr]<0)!==(clearance[next]<0))poly.push(intersection(curr,next));
      }
      for(let j=1;j+1<poly.length;j++)index.push(poly[0],poly[j],poly[j+1]);
    }
    for(const route of network.roads){
      const count=Math.ceil(route.length/3);let previousRow=-1;
      for(let i=0;i<=count;i++) {
        const d=i*route.length/count,row=mark.length,prev=previousRow;previousRow=row;
        for(const [l,h] of points){
          const f=network.frame(route,d,l);
          const adj=network.adjacent(f,route.id);
          const clear=asphaltSurface&&route.id?network.adjacent(f,route.id,true)-6.52:1;
          vertex(f.x,f.h+h,f.z,l,d,T.MathUtils.smoothstep(adj,6.7,9.5),clear);
        }
        if(!i)continue;
        if(!asphaltSurface){
          const side=Math.sign(points[0][0]),mid=d-route.length/(2*count);
          if(network.stopOpening(route,route.start+mid,side)||network.adjacent(network.frame(route,mid,side*6.78),route.id)<7.8)continue;
        }
        for(let j=0;j<points.length-1;j++){
          const a=prev+j,b=row+j;
          triangle(a,a+1,b);triangle(a+1,b+1,b);
        }
      }
    }
    const g=new T.BufferGeometry();
    g.setAttribute('position',new T.Float32BufferAttribute(xyz,3));
    g.setAttribute('uv',new T.Float32BufferAttribute(uv,2));
    if(asphaltSurface)g.setAttribute('junctionPaint',new T.Float32BufferAttribute(mark,1));
    g.setIndex(index);g.computeVertexNormals();
    const m=new T.Mesh(g,material);scene.add(m);return m;
  }
  const mirror=new Reflector(new T.PlaneGeometry(1,1),{textureWidth:960,textureHeight:540,multisample:0,clipBias:.001});
  const projection={value:new T.Matrix4()}, clock={value:0},plane={value:new T.Vector4()};
  for(const t of [deck,asphalt,normals,roughness]){t.wrapS=t.wrapT=T.RepeatWrapping;t.repeat.set(.43,.43);}
  const roadMat=new T.MeshStandardMaterial({color:0xb6c1c8,map:asphalt,normalMap:normals,normalScale:new T.Vector2(.40,.40),roughnessMap:roughness,metalness:.18,roughness:.88});
  roadMat.onBeforeCompile=sh=>{
    Object.assign(sh.uniforms,{roadReflection:{value:mirror.getRenderTarget().texture},roadProjection:projection,roadTime:clock,roadGrime:{value:deck},wetPlane:plane});
    sh.vertexShader='attribute float junctionPaint; varying float paintOpacity; varying vec2 trackUV; varying vec3 wetWorld; varying vec4 roadProjectionUV; uniform mat4 roadProjection;\n'+sh.vertexShader;
    sh.vertexShader=sh.vertexShader.replace('#include <uv_vertex>','#include <uv_vertex>\npaintOpacity=junctionPaint;trackUV=uv; wetWorld=(modelMatrix*vec4(position,1.)).xyz;roadProjectionUV=roadProjection*vec4(wetWorld,1.);');
    sh.fragmentShader='varying float paintOpacity; varying vec2 trackUV; varying vec3 wetWorld; varying vec4 roadProjectionUV; uniform vec4 wetPlane; uniform sampler2D roadReflection; uniform float roadTime; uniform sampler2D roadGrime;\n'+rainSurfaceShader+'\n'+sh.fragmentShader;
    sh.fragmentShader=sh.fragmentShader.replace('#include <normal_fragment_maps>',`
      #include <normal_fragment_maps>
      vec2 texNoise=texture2D(roadGrime,vMapUv*.42).rg;
      float macro=texture2D(roadGrime,trackUV*vec2(.041,.015)).g;
      float rut=exp(-pow((abs(abs(trackUV.x)-3.)-.83)/.48,2.));
      float puddle=smoothstep(-.027,.17,texNoise.y+macro*.75+.08*sin(trackUV.x*.65+trackUV.y*.034)-rut*.051);
      float pixelWidth=max(length(dFdx(trackUV)),length(dFdy(trackUV)));
      float basin=sin(trackUV.x*1.63+trackUV.y*.22+.65*sin(trackUV.y*.39))+.71*sin(trackUV.y*.79-trackUV.x*.53);
      float paintDistance=min(abs(trackUV.x),abs(abs(trackUV.x)-5.99));
      float pooled=smoothstep(-.40,.57,basin)*(1.-.33*rut);
      float resolved=1.-smoothstep(.045,.16,pixelWidth);
      float wetDetail=smoothstep(.13,.71,max(puddle,.73*pooled))*pooled
                     *smoothstep(.065,.19,paintDistance)*resolved;
      puddle=max(puddle,.42*pooled*resolved);
      vec2 rainSlope=vec2(0.);
      if(wetDetail>.005){
        rainSlope=wetDetail*(rainImpact(trackUV,roadTime,pixelWidth,.91,8.)
                  +.64*rainImpact(trackUV+vec2(2.76,1.82),roadTime,pixelWidth,.67,51.));
        normal=normalize(normal+tbn[0]*rainSlope.x*.23+tbn[1]*rainSlope.y*.23);
      }`);
    sh.fragmentShader=sh.fragmentShader.replace('#include <opaque_fragment>',`
      float x=trackUV.x, s=trackUV.y;
      vec2 ref=roadProjectionUV.xy/roadProjectionUV.w;
      vec2 chip=texture2D(normalMap,vNormalMapUv).rg-.5;
      float wide=length(vViewPosition);
      float waterSheet=sin(s*9.+roadTime*2.6+x*.7)*sin(x*7.-s*.9)*.0007;
      vec2 offset=chip*vec2(.008,.017)*(1.-.55*puddle)+vec2(waterSheet,(macro-.06)*.019);
      vec3 rippleNormal=tbn[0]*rainSlope.x+tbn[1]*rainSlope.y;
      offset+=rippleNormal.xy*vec2(.012,.021);
      vec2 blur=vec2(.0007,.0028)*(1.65-puddle);
      vec3 water=(texture2D(roadReflection,ref+offset).rgb*.44+texture2D(roadReflection,ref+offset+blur).rgb*.21+texture2D(roadReflection,ref+offset-blur).rgb*.21+texture2D(roadReflection,ref+offset+vec2(0.,blur.y*2.3)).rgb*.14);
      float facing=clamp(dot(normalize(vViewPosition),normal),0.,1.);
      float fresnel=.025+.74*pow(1.-facing,3.4);
      float inBounds=step(0.,ref.x)*step(ref.x,1.)*step(0.,ref.y)*step(ref.y,1.)*step(0.,roadProjectionUV.w);
      float onPlane=1.-smoothstep(.19,.74,abs(dot(wetPlane,vec4(wetWorld,1.))));
      outgoingLight*=1.-rut*.21-puddle*.10;
      outgoingLight=mix(outgoingLight,water,inBounds*onPlane*fresnel*(.16+.84*puddle)*(1.-smoothstep(100.,245.,wide)));
      float lampDist=abs(mod(s-12.+16.,32.)-16.);
      float lampPool=exp(-lampDist*lampDist*.030)*exp(-pow((abs(x)-6.5)/4.,2.));
      vec3 poolColor=mix(vec3(.08,.33,.38),vec3(.48,.19,.10),smoothstep(-.18,.6,sin(s*.0061)));
      outgoingLight+=poolColor*lampPool*(.09+.28*texture2D(map,vMapUv).r);
      // Only the facets facing the wet skylight catch a rim; the opposite side stays dark.
      float rainGlint=max(0.,dot(rainSlope,vec2(-.60,.80)));
      outgoingLight+=rainGlint*(vec3(.047,.071,.079)+.20*outgoingLight+.22*poolColor*lampPool);
      float gap=abs(mod(s+17.5,56.)-28.);
      float joint=1.-smoothstep(.017,.045+fwidth(s),gap);
      outgoingLight*=1.-.55*joint*(1.-smoothstep(18.,55.,wide));
      float gutter=smoothstep(6.18,6.28,abs(x));
      float grate=(1.-smoothstep(.07,.12,abs(mod(s,.33)-.165)))*gutter;
      outgoingLight=mix(outgoingLight,outgoingLight*.20+vec3(.016,.021,.027),grate*(1.-smoothstep(9.,45.,wide)));
      float halfLine=max(.018,fwidth(x)),edge=1.-smoothstep(.055-halfLine,.055+halfLine,abs(abs(x)-5.99));
      float center=(1.-smoothstep(.07-halfLine,.07+halfLine,abs(x)))*(1.-smoothstep(3.2-fwidth(s),3.2+fwidth(s),mod(s+1.,12.)));
      float wear=clamp(.53+2.*texture2D(roughnessMap,vRoughnessMapUv*.9).r-.34*rut,.53,1.);
      vec3 stripe=vec3(.39,.45,.40)+outgoingLight*.63;
      outgoingLight=mix(outgoingLight,stripe,max(edge*.75,center*.85)*wear*paintOpacity);
      #include <opaque_fragment>`);
  };
  const road=strip([[-6.55,0],[-3.25,0],[0,0],[3.25,0],[6.55,0]],roadMat,true);
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
    surface:roadMat,
    reflect(renderer,camera,f,time) {
      clock.value=time;
      along.set(f.fx,Math.tan(f.grade),f.fz);across.set(f.rx,-Math.sin(f.bank),f.rz);
      normal.crossVectors(across,along).normalize();
      mirror.position.set(f.x,f.h-.025,f.z);mirror.quaternion.setFromUnitVectors(z,normal);mirror.updateMatrixWorld();
      plane.value.set(normal.x,normal.y,normal.z,-normal.dot(mirror.position));
      camera.updateMatrixWorld();road.visible=false;
      mirror.onBeforeRender(renderer,scene,camera);road.visible=true;
      projection.value.multiplyMatrices(mirror.material.uniforms.textureMatrix.value,inv.copy(mirror.matrixWorld).invert());
    },
    resize(w,h){mirror.getRenderTarget().setSize(Math.min(1440,Math.round(w*.7)),Math.min(900,Math.round(h*.7)));}
  };
}
