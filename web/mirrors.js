import * as T from 'three';

export function rearMirrors(scene,renderer,bike,body){
  const target=new T.WebGLRenderTarget(768,320,{type:T.HalfFloatType,depthBuffer:true,samples:0});
  // Trace the boundaries of the two silver inserts on the original film bike. The
  // fixed construction camera maps that silhouette into the bike's local geometry.
  const construction=new T.PerspectiveCamera(65+9*24/82,1.6,.045,100);
  construction.position.set(0,1.46,0);construction.lookAt(1,1.46+Math.tan(-.145),0);construction.updateMatrixWorld();
  const maskCanvas=document.createElement('canvas');maskCanvas.width=200;maskCanvas.height=90;
  const m=maskCanvas.getContext('2d');m.fillStyle='white';m.beginPath();
  const glass=[[302,628],[324,604],[336,601],[414,599],[427,600],[464,619],[472,626],[470,634],[444,656],[430,662],[345,662],[332,659],[325,654]];
  glass.forEach(([x,y],i)=>{if(i)m.lineTo(x-288,y-584);else m.moveTo(x-288,y-584);});m.closePath();m.fill();
  const mask=new T.CanvasTexture(maskCanvas);
  body.updateWorldMatrix(true,true);
  const point=new T.Vector3(),projected=new T.Vector3();
  body.traverse(part=>{
    if(!part.isMesh)return;
    const geometry=part.geometry.clone(),attr=geometry.getAttribute('position');
    const uv=new Float32Array(attr.count*3);
    for(let i=0;i<attr.count;i++){
      point.fromBufferAttribute(attr,i).applyMatrix4(part.matrixWorld);
      if(point.x<.605||point.x>.755||point.y<1.105||point.y>1.255||Math.abs(point.z)<.245||Math.abs(point.z)>.477)continue;
      projected.copy(point);projected.z=-Math.abs(point.z);projected.project(construction);
      const px=(projected.x+1)*720,py=(1-projected.y)*450;
      uv[i*3]=(px-288)/200;uv[i*3+1]=1-(py-584)/90;uv[i*3+2]=Math.sign(point.z);
    }
    geometry.setAttribute('mirrorGlass',new T.BufferAttribute(uv,3));part.geometry=geometry;
    const material=part.material.clone();part.material=material;
    material.onBeforeCompile=sh=>{
      Object.assign(sh.uniforms,{rearView:{value:target.texture},glassMask:{value:mask}});
      sh.vertexShader='attribute vec3 mirrorGlass; varying vec3 glassUV;\n'+sh.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\nglassUV=mirrorGlass;');
      sh.fragmentShader='uniform sampler2D rearView,glassMask; varying vec3 glassUV;\n'+sh.fragmentShader;
      sh.fragmentShader=sh.fragmentShader.replace('#include <opaque_fragment>',`
        if(abs(glassUV.z)>.9 && all(greaterThan(glassUV.xy,vec2(0.))) && all(lessThan(glassUV.xy,vec2(1.)))){
          float useGlass=texture2D(glassMask,glassUV.xy).a;
          float mirrorX=glassUV.z<0.?mix(.96,.49,glassUV.x):mix(.06,.53,glassUV.x);
          vec3 rear=texture2D(rearView,vec2(mirrorX,mix(.28,.81,glassUV.y))).rgb;
          outgoingLight=mix(outgoingLight,rear*vec3(.77,.87,.93)+vec3(.016,.026,.030),useGlass*.97);
        }
        #include <opaque_fragment>`);
    };
  });
  const camera=new T.PerspectiveCamera(67,target.width/target.height,.15,1350),look=new T.Vector3();
  let last=-Infinity;
  return (frame,heading,time,crash)=>{
    if(crash || (time>=last && time-last<1/24))return;last=time;
    camera.position.set(frame.x,frame.h+1.22,frame.z);
    look.set(-Math.cos(heading),-.058-Math.tan(frame.grade),-Math.sin(heading)).add(camera.position);
    camera.lookAt(look);camera.updateMatrixWorld();
    const prev=renderer.getRenderTarget();bike.visible=false;
    renderer.setRenderTarget(target);renderer.clear();renderer.render(scene,camera);renderer.setRenderTarget(prev);bike.visible=true;
  };
}
