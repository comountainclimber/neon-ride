import * as T from 'three';
import {GLTFLoader} from './vendor/loaders/GLTFLoader.js';
import {MeshoptDecoder} from './vendor/meshopt_decoder.module.js';
import {EffectComposer} from './vendor/postprocessing/EffectComposer.js';
import {RenderPass} from './vendor/postprocessing/RenderPass.js';
import {UnrealBloomPass} from './vendor/postprocessing/UnrealBloomPass.js';
import {OutputPass} from './vendor/postprocessing/OutputPass.js';
import {practicals} from './practicals.js';
import {createTrack} from './track.js';
import {instruments} from './instruments.js';

const V=(x=0,y=0,z=0)=>new T.Vector3(x,y,z);
const names=['bike','glove','truck','cab','tower-a','tower-b','tower-c','building-crown','water-tower','lamp-a','lamp-b','sign-a','sign-b','sign-c','car','car-a','car-b','car-c','jelly'];
const detailed=['bike','glove','truck','cab','tower-a','tower-b','tower-c','lamp-a','sign-a'];
const sectors=['Rainline','Glass Heights','Violet Reach','Lower Ring'];
export const sector=(s,len)=>sectors[Math.min(3,Math.floor(s/len*4))];

function environment(scene,skies) {
  const c=document.createElement('canvas');c.width=4096;c.height=2048;
  const ctx=c.getContext('2d'),g=ctx.createLinearGradient(0,0,0,c.height);
  for(const [s,color] of [[0,'#12142c'],[.34,'#313955'],[.53,'#66829c'],[1,'#131823']])g.addColorStop(s,color);
  ctx.fillStyle=g;ctx.fillRect(0,0,c.width,c.height);
  for(let i=0;i<4;i++){
    ctx.save();ctx.translate(i*1024,0);if(i%2){ctx.translate(1024,0);ctx.scale(-1,1);}
    ctx.drawImage(skies[0].image,0,640,1025,675);
    const layer=document.createElement('canvas');layer.width=1024;layer.height=675;
    const lc=layer.getContext('2d');lc.drawImage(skies[1].image,0,0,1024,675);
    const mask=lc.createLinearGradient(0,0,1024,0);mask.addColorStop(0,'#0000');mask.addColorStop(.55,'#0009');mask.addColorStop(1,'#0000');
    lc.globalCompositeOperation='destination-in';lc.fillStyle=mask;lc.fillRect(0,0,1024,675);ctx.drawImage(layer,0,640);ctx.restore();
  }
  for(const [y,h,a,b] of [[605,130,'#303953','#30395300'],[1170,130,'#30384500','#242e43']]){
    const fade=ctx.createLinearGradient(0,y,0,y+h);fade.addColorStop(0,a);fade.addColorStop(1,b);ctx.fillStyle=fade;ctx.fillRect(0,y,c.width,h);
  }
  const mist=ctx.createLinearGradient(0,790,0,1320);mist.addColorStop(0,'#29375400');mist.addColorStop(.44,'#344861c9');mist.addColorStop(.75,'#283954f3');mist.addColorStop(1,'#252f45');
  ctx.fillStyle=mist;ctx.fillRect(0,790,c.width,530);
  const env=new T.CanvasTexture(c);env.mapping=T.EquirectangularReflectionMapping;env.colorSpace=T.SRGBColorSpace;
  scene.environment=env;scene.background=env;scene.backgroundIntensity=.76;scene.environmentIntensity=.64;
  const hemi=new T.HemisphereLight(0xc0d5ff,0x493f57,1.13);scene.add(hemi);
  const moon=new T.DirectionalLight(0xffe3c2,1.62);moon.position.set(-100,210,65);scene.add(moon);
}

function staticScenery(scene,entries,models){
  const batches=[],tmp=new T.Object3D(),mult=new T.Matrix4();
  for(const name of new Set(entries.map(entry=>entry[0]))){
    const items=entries.filter(item=>item[0]===name);
    const ornate=models.has('detail/'+name);
    for(const hi of ornate?[false,true]:[false]){
      const asset=models.get((hi?'detail/':'models/')+name);
      const transforms=items.map(([,x,y,z,size,yaw])=>{
        tmp.position.set(x,z,y);tmp.rotation.set(0,-yaw,0);tmp.scale.setScalar(size/asset.length);
        tmp.updateMatrix();return tmp.matrix.clone();
      });
      const meshes=[];
      asset.obj.traverse(part=>{
        if(!part.isMesh)return;
        const mesh=new T.InstancedMesh(part.geometry,part.material,items.length);
        mesh.frustumCulled=false;mesh.instanceMatrix.setUsage(T.DynamicDrawUsage);
        scene.add(mesh);meshes.push({mesh,local:part.matrixWorld.clone()});
      });
      batches.push({items,transforms,meshes,hi,ornate});
    }
  }
  let last=V(Infinity,0,0),prevYaw=Infinity;
  return (p)=>{
    if(last.distanceToSquared(p.position)<25&&Math.abs(prevYaw-p.yaw)<.07)return;
    last.copy(p.position);prevYaw=p.yaw;
    for(const {items,transforms,meshes,hi,ornate} of batches){
      let count=0;
      items.forEach(([,x,y,,size],i)=>{
        const dx=x-p.position.x,dz=y-p.position.z,distance=Math.hypot(dx,dz),forward=dx*Math.cos(p.yaw)+dz*Math.sin(p.yaw);
        if(distance>Math.max(250,size*5.5) || forward<-Math.max(35,size*.74) || (forward+size*.65)/Math.max(1,distance)<.17)return;
        if(ornate && hi !== (distance<Math.max(38,size*.8)))return;
        for(const {mesh,local} of meshes)mesh.setMatrixAt(count,mult.multiplyMatrices(transforms[i],local));
        count++;
      });
      for(const {mesh} of meshes){mesh.count=count;mesh.visible=count>0;mesh.instanceMatrix.needsUpdate=true;}
    }
  };
}

function rain(scene){
  const n=380,geo=new T.BufferGeometry(),pts=[],seed=new Float32Array(n*3);
  for(let i=0;i<n;i++){seed[i*3]=Math.random()*25;seed[i*3+1]=Math.random()*16;seed[i*3+2]=Math.random()*25;pts.push(0,0,0,0,0,0);}
  const buffer=new T.Float32BufferAttribute(pts,3);geo.setAttribute('position',buffer);
  const lines=new T.LineSegments(geo,new T.LineBasicMaterial({color:0xa2bacc,opacity:.19,transparent:true,depthWrite:false}));
  lines.frustumCulled=false;scene.add(lines);
  const wrap=(x,k)=>x-Math.floor(x/k)*k;
  return (eye,time,speed)=>{
    for(let i=0;i<n;i++){
      const x=eye.x+wrap(seed[i*3]+2.3*time-eye.x,25)-12.5;
      const y=eye.y+wrap(seed[i*3+1]-12.4*time-eye.y,16)-3;
      const z=eye.z+wrap(seed[i*3+2]-eye.z,25)-12.5;
      buffer.setXYZ(i*2,x,y,z);buffer.setXYZ(i*2+1,x-.034,y+.17+speed*.0008,z+.012);
    }
    buffer.needsUpdate=true;
  };
}

function setFrame(obj,f,raise=0,roll=0){
  obj.position.set(f.x,f.h+raise,f.z);
  obj.rotation.order='YZX';obj.rotation.set(f.bank+roll,-f.yaw,f.grade,'YZX');
}

export async function makeWorld(canvas,circuit,onLoad){
  const renderer=new T.WebGLRenderer({canvas,antialias:true,powerPreference:'high-performance'});
  const dpr=Math.min(devicePixelRatio||1,2);
  renderer.setPixelRatio(dpr);renderer.toneMapping=T.ACESFilmicToneMapping;renderer.toneMappingExposure=1.14;
  const scene=new T.Scene(),camera=new T.PerspectiveCamera(67,1,.045,2900);
  scene.fog=new T.FogExp2(0x354961,.00195);
  const composer=new EffectComposer(renderer);
  composer.renderTarget1.samples=composer.renderTarget2.samples=Math.min(4,renderer.capabilities.maxSamples);
  composer.addPass(new RenderPass(scene,camera));
  const bloom=new UnrealBloomPass(new T.Vector2(640,360),.28,.32,.94);composer.addPass(bloom);composer.addPass(new OutputPass());
  const models=new Map(),loader=new GLTFLoader().setMeshoptDecoder(MeshoptDecoder),texLoader=new T.TextureLoader();
  const loadList=[...names.map(k=>'models/'+k),...detailed.map(k=>'detail/'+k),'rider/vesper'];
  let loaded=0;
  async function fetchModel(key){
    const {scene:obj,animations}=await loader.loadAsync('./assets/'+key+'.glb');obj.updateMatrixWorld(true);
    const bounds=new T.Box3().setFromObject(obj),size=bounds.getSize(V());
    models.set(key,{obj,length:Math.max(size.x,size.y,size.z),bounds,animations});
    obj.traverse(m=>{if(m.material?.map)m.material.map.anisotropy=Math.min(8,renderer.capabilities.getMaxAnisotropy());});
    onLoad(++loaded,loadList.length+12);
  }
  const jobs=loadList.slice();
  const modelLoaders=Array.from({length:5},async()=>{let key;while((key=jobs.shift()))await fetchModel(key);});
  const [skies,manifest]=await Promise.all([Promise.all(['skyline-a','skyline-b','deck','asphalt','asphalt-normal','asphalt-rough','word-1','word-2','word-3','word-4','word-5','word-6'].map(async n=>{
    const tex=await texLoader.loadAsync('./assets/visual/'+n+'.webp');tex.colorSpace=n==='asphalt-normal'||n==='asphalt-rough'?T.NoColorSpace:T.SRGBColorSpace;tex.anisotropy=renderer.capabilities.getMaxAnisotropy();onLoad(++loaded,loadList.length+12);return tex;
  })),fetch('./scenery.json').then(r=>r.json()),...modelLoaders]);
  environment(scene,skies);
  practicals(scene,manifest,skies.slice(6));
  const track=createTrack(scene,circuit,skies.slice(2,6)),refresh=staticScenery(scene,manifest.items,models);
  function clone(key,size){
    const asset=models.get(key),group=new T.Group(),copy=asset.obj.clone(true);copy.scale.multiplyScalar(size/asset.length);group.add(copy);return group;
  }
  const bike=new T.Group();scene.add(bike);
  const body=clone('detail/bike',2.107),hands=new T.Group();body.position.x=.2532;body.rotation.y=Math.PI/2;bike.add(body,hands);
  for(const side of [-1,1]){
    const wrist=new T.Group(),glove=clone('detail/glove',.4214);
    wrist.position.set(.2274,.976,side*.33);wrist.rotation.set(0,-side*.157,-1.466,'YZX');
    glove.rotation.y=-Math.PI/2;glove.scale.x=side;wrist.add(glove);hands.add(wrist);
  }
  const display=instruments(bike);
  const beam=new T.SpotLight(0xc9e6ff,60,70,.46,.7,1.2);beam.position.set(.9,.91,0);beam.target.position.set(24,0,0);bike.add(beam,beam.target);
  const rider=new T.Group(),vesper=models.get('rider/vesper');
  vesper.obj.rotation.y=Math.PI/2;vesper.obj.position.set(-.03251,-1.04948,.01679);rider.add(vesper.obj);
  const mixer=new T.AnimationMixer(vesper.obj),clip=T.AnimationClip.findByName(vesper.animations,'CrashEject');
  if(!clip)throw new Error('Vesper crash animation is missing.');
  const action=mixer.clipAction(clip);action.setLoop(T.LoopOnce,1);action.clampWhenFinished=true;action.play();action.paused=true;
  let suit;vesper.obj.traverse(o=>{if(o.isSkinnedMesh)suit=o;});
  const foot=V();
  const riderMount=new T.Group();riderMount.add(rider);scene.add(riderMount);
  const visorLight=new T.PointLight(0xc2eaff,19,10,1.6);scene.add(visorLight);
  const carList=Array.from({length:14},(_,i)=>{
    const truck=circuit.car(i,0).truck,model=truck?'truck':'cab',root=new T.Group(),hi=clone('detail/'+model,truck?13:4.8),low=clone('models/'+model,truck?13:4.8);
    hi.rotation.y=low.rotation.y=Math.PI/2;root.add(hi,low);scene.add(root);
    const tailMat=new T.MeshBasicMaterial({color:new T.Color(3.2,.08,.055)}),tailGeom=new T.BoxGeometry(.035,.075,truck?.39:.24);
    for(const side of [-1,1]){
      const light=new T.Mesh(tailGeom,tailMat);light.position.set(truck?-6.28:-2.29,truck?.83:.65,side*(truck?1.17:.83));root.add(light);
    }
    return {root,hi,low};
  });
  const {aircraft,origin,aspect,jelly:landmark}=manifest.airspace;
  const fliers=aircraft.map(flight=>{
    const flier=clone('models/'+flight.model,flight.size);scene.add(flier);return flier;
  });
  const jelly=clone('models/jelly',landmark.size);
  jelly.position.set(landmark.position[0],landmark.position[2],landmark.position[1]);scene.add(jelly);
  const cabinetLight=new T.PointLight(0xb9ddff,3.2,3,1.6);cabinetLight.position.set(.30,1.80,-.23);bike.add(cabinetLight);
  const rainfall=rain(scene),aim=V(),normal=V(),target=V();
  const resize=()=>{
    const w=Math.max(1,canvas.clientWidth),h=Math.max(1,canvas.clientHeight);
    renderer.setSize(w,h,false);composer.setSize(w,h);track.resize(w*dpr,h*dpr);camera.aspect=w/h;camera.updateProjectionMatrix();
  };new ResizeObserver(resize).observe(canvas);resize();
  function draw(p,look){
    const f=circuit.frame(p.s,p.lateral),time=p.elapsed,crash=p.crashAge>=0,pose=crash?circuit.crash():null;
    setFrame(bike,f,crash?.1:0,crash?pose.bikeRoll:p.lean*.55);
    riderMount.visible=crash;hands.visible=!crash;visorLight.visible=crash;
    display.update(p);
    let viewYaw=f.yaw+look.yaw;
    if(crash){
      const rf=circuit.frame(pose.riderS,pose.riderLateral);
      setFrame(riderMount,rf,pose.riderHeight);
      rider.rotation.set(pose.riderRoll,-pose.riderYaw,pose.riderPitch,'YZX');
      action.time=Math.min(clip.duration,p.crashAge);mixer.update(0);
      riderMount.updateMatrixWorld(true);
      if(suit && p.crashAge>.65){
        let bottom=Infinity;suit.skeleton.update();
        for(let k=0;k<suit.geometry.attributes.position.count;k+=23){
          suit.getVertexPosition(k,foot);foot.applyMatrix4(suit.matrixWorld);bottom=Math.min(bottom,foot.y);
        }
        riderMount.position.y+=Math.max(0,rf.h+.025-bottom);
      }
      const side=p.crashSide*(3.4+1.4*Math.min(1,p.crashAge));
      camera.position.set(rf.x-f.fx*pose.cameraDistance+f.rx*side,rf.h+pose.cameraHeight,rf.z-f.fz*pose.cameraDistance+f.rz*side);
      target.set(rf.x,rf.h+Math.max(.65,pose.riderHeight+.15),rf.z);camera.up.set(0,1,0);camera.lookAt(target);
      visorLight.position.copy(camera.position).lerp(target,.68).y+=2;
      viewYaw=Math.atan2(target.z-camera.position.z,target.x-camera.position.x);
    }else{
      const lean=f.bank+p.lean*.38,shake=Math.sin(time*49)*.003*p.rpm+(p.impact>0?Math.sin(time*75)*.018:0);
      camera.position.set(f.x+f.rx*lean*.34,f.h+1.46+shake,f.z+f.rz*lean*.34);
      const pitch=f.grade-.145+look.pitch;
      aim.set(Math.cos(viewYaw)*Math.cos(pitch),Math.sin(pitch),Math.sin(viewYaw)*Math.cos(pitch));
      camera.up.set(0,1,0);camera.lookAt(target.copy(camera.position).add(aim));camera.rotateZ(-lean*.57);
    }
    const fov=crash?56:65+9*p.v/82+3*p.boost;
    if(Math.abs(fov-camera.fov)>.015){camera.fov=fov;camera.updateProjectionMatrix();}
    refresh({position:camera.position,yaw:viewYaw});
    carList.forEach(({root,hi,low},i)=>{
      const car=circuit.car(i,time),cf=circuit.frame(car.s,car.lane);
      setFrame(root,cf);const distance=root.position.distanceTo(camera.position);root.visible=distance<580;hi.visible=distance<82;low.visible=!hi.visible;
    });
    fliers.forEach((m,i)=>{
      const flight=aircraft[i],a=flight.phase+time*flight.rate,r=flight.radius;
      m.position.set(origin[0]+r*Math.cos(a),flight.altitude,origin[1]+r*aspect*Math.sin(a));
      m.rotation.y=-Math.atan2(aspect*Math.cos(a),-Math.sin(a))+Math.PI/2;
    });
    jelly.position.y=landmark.position[2]+landmark.amplitude*Math.sin(time*landmark.rate);
    rainfall(camera.position,time,p.v);track.reflect(renderer,camera,f,time);composer.render();
  }
  return {draw,modelCount:models.size};
}
