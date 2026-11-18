import * as T from 'three';

const MAX_NEAR=5;

// World-space lighting keeps the spill and contact patches on the actual banked asphalt,
// including when seen through the bike mirrors or from the exterior crash camera.
export function trafficLighting(surface){
  const transforms=Array.from({length:MAX_NEAR},()=>new T.Vector4());
  const shapes=Array.from({length:MAX_NEAR},()=>new T.Vector4());
  const previous=surface.onBeforeCompile.bind(surface);
  surface.onBeforeCompile=(sh,renderer)=>{
    previous(sh,renderer);
    sh.uniforms.vehicleFrame={value:transforms};sh.uniforms.vehicleBody={value:shapes};
    sh.fragmentShader=`uniform vec4 vehicleFrame[${MAX_NEAR}];
    uniform vec4 vehicleBody[${MAX_NEAR}];
    float tirePatch(vec2 p,float lengthRadius,float widthRadius){
      vec2 d=p/vec2(lengthRadius,widthRadius);
      return exp(-min(70.,dot(d,d)*1.65));
    }
    `+sh.fragmentShader;
    sh.fragmentShader=sh.fragmentShader.replace('#include <opaque_fragment>',`
      for(int i=0;i<${MAX_NEAR};i++){
        vec4 frame=vehicleFrame[i],body=vehicleBody[i];
        if(body.x<=0.)continue;
        vec2 delta=wetWorld.xz-frame.xy;
        if(dot(delta,delta)>3025. || abs(wetWorld.y-frame.z)>5.)continue;
        vec2 forward=vec2(cos(frame.w),sin(frame.w)),right=vec2(-forward.y,forward.x);
        vec2 local=vec2(dot(delta,forward),dot(delta,right));
        float halfLen=body.x,halfWidth=body.y,kind=body.z;
        float side=abs(local.y),bike=1.-step(.5,kind),truck=step(1.5,kind);
        vec2 outside=max(abs(local)-vec2(halfLen*.88,halfWidth*.77),vec2(0.));
        float belly=exp(-dot(outside, outside)*2.5);
        float wheelX=halfLen*mix(.60,.73,truck);
        float tireSide=side-mix(halfWidth*.90,0.,bike);
        float tires=tirePatch(vec2(abs(local.x)-wheelX,tireSide),mix(.43,.56,truck),mix(.27,.34,truck));
        tires=max(tires,truck*tirePatch(vec2(local.x+halfLen*.46,tireSide),.43,.30));
        tires=max(tires,truck*tirePatch(vec2(local.x-halfLen*.26,tireSide),.43,.30));
        // The center darkens gradually beneath the chassis. Wheels have a sharper contact core.
        float contact=max(belly*mix(.58,.45,bike),tires*.90);
        outgoingLight*=1.-contact;
        float rear=-halfLen-local.x,front=local.x-halfLen;
        float spread=mix(halfWidth*.80,0.,bike);
        float toLamp=side-spread;
        float brake=body.w;
        float lampHeight=mix(.60,.87,truck);
        float r2=rear*rear+toLamp*toLamp+lampHeight*lampHeight;
        float stop=lampHeight/(r2*sqrt(r2))*smoothstep(-.85,.62,rear);
        vec3 toEye=normalize(cameraPosition-wetWorld);
        vec3 surfaceNormal=inverseTransformDirection(normal,viewMatrix);
        float highlight=0.;
        for(int lampIndex=0;lampIndex<2;lampIndex++){
          vec2 lens=frame.xy-forward*halfLen+right*(float(lampIndex)*2.-1.)*spread;
          vec3 toLens=normalize(vec3(lens.x-wetWorld.x,frame.z+lampHeight-wetWorld.y,lens.y-wetWorld.z));
          vec3 halfRay=normalize(toLens+toEye);
          highlight+=.5*pow(max(dot(halfRay,surfaceNormal),0.),mix(35.,113.,puddle));
        }
        float tailStreak=highlight*exp(-max(0.,rear)*.14)*smoothstep(-.22,.38,rear)*(.16+.84*puddle);
        outgoingLight+=vec3(1.,.029,.009)*(stop*.22+tailStreak*.64)*(1.+brake*3.0);
        // Twin dipped beams spread as they run forward. Amber truck clearance lamps light the edge.
        float range=max(0.,front),width=.56+range*.21;
        float hotspot=exp(-min(65.,toLamp*toLamp/(width*width)))*exp(-range*.16);
        float headlights=smoothstep(-.12,1.35,front)*hotspot*(1.-bike);
        outgoingLight+=vec3(.56,.71,.87)*headlights*(.29+.16*puddle);
        vec2 markerDelta=vec2(max(0.,side-halfWidth)*2.8,(abs(local.x)-halfLen*.5)*.96);
        float marker=exp(-min(65.,dot(markerDelta,markerDelta)));
        outgoingLight+=truck*vec3(.18,.055,.008)*marker*smoothstep(halfWidth-.05,halfWidth+.22,side);
      }
      #include <opaque_fragment>`);
  };
  surface.customProgramCacheKey=()=> 'wet-traffic-contact-v1';
  const scratch=[];
  function update(eye,bikeFrame,bikeHeading,braking,cars,crash){
    scratch.length=0;
    if(!crash)scratch.push({x:bikeFrame.x,z:bikeFrame.z,h:bikeFrame.h,yaw:bikeHeading,half:1.04,width:.30,kind:0,brake:braking,dist:0});
    for(const car of cars){
      const dx=eye.x-car.x,dz=eye.z-car.z;car.dist=dx*dx+dz*dz;
      if(car.dist<110*110)scratch.push(car);
    }
    scratch.sort((a,b)=>a.dist-b.dist);
    for(let i=0;i<MAX_NEAR;i++){
      const c=scratch[i];
      if(!c){shapes[i].x=0;continue;}
      transforms[i].set(c.x,c.z,c.h,c.yaw);shapes[i].set(c.half,c.width,c.kind,c.brake);
    }
  }
  return update;
}
