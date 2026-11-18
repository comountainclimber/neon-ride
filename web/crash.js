import * as T from 'three';

const UP=new T.Vector3(0,1,0);
const pelvis=new T.Vector3(.03251,1.04948,-.01679);
const bikeCenter=new T.Vector3(0,.54,0);
const V=()=>new T.Vector3();

function contactShadow(scene,texture){
  const material=new T.MeshBasicMaterial({map:texture,transparent:true,depthWrite:false,polygonOffset:true,polygonOffsetFactor:-1});
  const geometry=new T.PlaneGeometry(1,1);
  geometry.rotateX(-Math.PI/2);
  const mesh=new T.Mesh(geometry,material);
  mesh.renderOrder=1;
  scene.add(mesh);
  return mesh;
}

function bikeHull(body,bike){
  bike.updateMatrixWorld(true);
  const points=[],worldToBike=bike.matrixWorld.clone().invert(),local=new T.Matrix4();
  body.traverse(mesh=>{
    if(!mesh.isMesh)return;
    local.multiplyMatrices(worldToBike,mesh.matrixWorld);
    const attribute=mesh.geometry.getAttribute('position');
    const stride=Math.max(1,Math.floor(attribute.count/2600));
    for(let i=0;i<attribute.count;i+=stride){
      points.push(V().fromBufferAttribute(attribute,i).applyMatrix4(local).sub(bikeCenter));
    }
  });
  const hull=new Set(),axis=V();
  for(let i=0;i<96;i++){
    const y=1-(2*i+1)/96,radial=Math.sqrt(1-y*y),angle=i*2.3999632297;
    axis.set(radial*Math.cos(angle),y,radial*Math.sin(angle));
    let extreme=null,best=-Infinity;
    for(const point of points){
      const projection=axis.dot(point);
      if(projection>best){best=projection;extreme=point;}
    }
    if(extreme)hull.add(extreme);
  }
  return {hull:[...hull],samples:points};
}

// A parked motorcycle lies across the road on irregular contact points. Project
// its actual outline at several heights so its tyres, fairing and bar occlude the
// wet surface most strongly where they touch. Only rebuilt when the bike settles.
function parkedContact(scene,samples){
  const canvas=document.createElement('canvas');canvas.width=canvas.height=256;
  const ctx=canvas.getContext('2d'),tex=new T.CanvasTexture(canvas),shadow=contactShadow(scene,tex);
  shadow.visible=false;shadow.renderOrder=3;
  const world=V(),relative=V(),right=V(),forward=V(),matrix=new T.Matrix4(),cache=new T.Matrix4();
  function outline(points){
    points.sort((a,b)=>a.x-b.x||a.y-b.y);
    const half=list=>{const side=[];for(const p of list){
      while(side.length>1){const a=side[side.length-2],b=side[side.length-1];if((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x)>0)break;side.pop();}
      side.push(p);
    }side.pop();return side;};
    return half(points).concat(half(points.reverse()));
  }
  return {shadow,update(com,bike,frame,floorPoint,up){
    bike.updateMatrix();if(cache.equals(bike.matrix))return;
    cache.copy(bike.matrix);
    forward.set(frame.fx,Math.tan(frame.grade),frame.fz).normalize();right.crossVectors(forward,up).normalize();
    matrix.makeBasis(forward,up,right);shadow.quaternion.setFromRotationMatrix(matrix);
    shadow.position.copy(floorPoint).addScaledVector(up,.030);shadow.scale.set(3.45,1,3.45);
    const points=samples.map(sample=>{
      relative.copy(world.copy(sample).applyQuaternion(bike.quaternion).add(com)).sub(floorPoint);
      return {x:128+relative.dot(forward)*256/3.45,y:128+relative.dot(right)*256/3.45,h:relative.dot(up)};
    });
    ctx.clearRect(0,0,256,256);
    for(const [height,blur,alpha] of [[1.2,12,.37],[.48,6,.58],[.24,3.5,.69],[.09,1.2,.82]]){
      const edge=outline(points.filter(p=>p.h<height));if(edge.length<3)continue;
      ctx.filter='blur('+blur+'px)';ctx.fillStyle='rgba(6,10,13,'+alpha+')';ctx.beginPath();
      edge.forEach((p,i)=>i?ctx.lineTo(p.x,p.y):ctx.moveTo(p.x,p.y));ctx.closePath();ctx.fill();
    }
    ctx.filter='none';tex.needsUpdate=true;
  }};
}

function scrape(scene){
  const count=30,positions=new Float32Array(count*6),colors=new Float32Array(count*6);
  const geometry=new T.BufferGeometry();
  geometry.setAttribute('position',new T.BufferAttribute(positions,3).setUsage(T.DynamicDrawUsage));
  geometry.setAttribute('color',new T.BufferAttribute(colors,3).setUsage(T.DynamicDrawUsage));
  const material=new T.LineBasicMaterial({vertexColors:true,transparent:true,blending:T.AdditiveBlending,depthWrite:false,toneMapped:false});
  const streaks=new T.LineSegments(geometry,material);
  streaks.frustumCulled=false;
  scene.add(streaks);
  const tangent=V(),side=V(),start=V(),tip=V();
  const random=seed=>{const n=Math.sin(seed*127.1+311.7)*43758.5453;return n-Math.floor(n);};
  return (contact,road,at,speed,hit,ground)=>{
    streaks.visible=Boolean(ground&&(speed>2||hit>.12));
    if(!streaks.visible)return;
    tangent.set(road.fx,Math.tan(road.grade),road.fz).normalize();
    side.set(road.rx,-Math.sin(road.bank),road.rz).normalize();
    const strength=Math.min(1,speed/25+hit*.8);
    const active=Math.max(3,Math.round(count*strength));
    geometry.setDrawRange(0,active*2);
    for(let i=0;i<active;i++){
      const cycle=at*4.5+i*.618034,epoch=Math.floor(cycle),age=(cycle-epoch)*.22;
      const seed=i+73*epoch;
      const trail=speed*(.12+.26*random(seed))*age;
      const drift=(random(seed+2)-.5)*5*age;
      const kick=(1+random(seed+3)*3)*age-4.9*age*age;
      start.copy(contact).addScaledVector(tangent,-trail).addScaledVector(side,drift).y+=Math.max(.013,kick);
      tip.copy(start).addScaledVector(tangent,-(.02+age*speed*.027)).y+=.007;
      start.toArray(positions,6*i);tip.toArray(positions,6*i+3);
      const energy=(1-age/.22)*strength;
      colors.set([2.2*energy,1.2*energy,.28*energy,.8*energy,.20*energy,.025*energy],6*i);
    }
    geometry.attributes.position.needsUpdate=geometry.attributes.color.needsUpdate=true;
  };
}

export function createCrash(scene,camera,circuit,bike,body,vesper,setFrame){
  const riderFrame=new T.Group(),rider=new T.Group();
  riderFrame.name='VesperEjection';
  vesper.obj.rotation.y=Math.PI/2;vesper.obj.position.copy(pelvis).negate();
  rider.add(vesper.obj);riderFrame.add(rider);scene.add(riderFrame);
  const mixer=new T.AnimationMixer(vesper.obj);
  const clip=T.AnimationClip.findByName(vesper.animations,'CrashEject');
  if(!clip)throw new Error('Vesper crash animation is missing.');
  const action=mixer.clipAction(clip);
  action.setLoop(T.LoopOnce,1);action.clampWhenFinished=true;action.play();action.paused=true;
  let suit;
  vesper.obj.traverse(mesh=>{if(mesh.isSkinnedMesh)suit=mesh;});
  const {hull,samples}=bikeHull(body,bike),parkedShade=parkedContact(scene,samples);
  const texCanvas=document.createElement('canvas');texCanvas.width=texCanvas.height=96;
  const ctx=texCanvas.getContext('2d'),grad=ctx.createRadialGradient(48,48,0,48,48,48);
  grad.addColorStop(0,'#091019e3');grad.addColorStop(.42,'#13192399');grad.addColorStop(1,'#13192300');
  ctx.fillStyle=grad;ctx.fillRect(0,0,96,96);
  const texture=new T.CanvasTexture(texCanvas);
  const riderShadow=contactShadow(scene,texture),bikeShadow=contactShadow(scene,texture);
  const light=new T.PointLight(0xd9eaff,12,9,1.7);scene.add(light);
  const sparks=scrape(scene);
  const frameUp=V(),riderCOM=V(),bikeCOM=V(),scratch=V(),floorPoint=V(),contact=V(),camTarget=V(),towardEye=V(),camRight=V(),camUp=V();
  const spin=new T.Quaternion(),angles=new T.Euler(0,0,0,'YZX');
  let shown=false;
  function visible(value){
    parkedShade.shadow.visible=false;
    bikeShadow.visible=light.visible=value;
    if(value===shown && riderFrame.visible===value)return;
    shown=value;riderFrame.visible=riderShadow.visible=bikeShadow.visible=light.visible=value;
    if(!value)sparks(null,null,0,0,0,false);
  }
  visible(false);
  function floor(frame){
    floorPoint.set(frame.x,frame.h,frame.z);
    frameUp.set(-frame.fx*Math.tan(frame.grade)+frame.rx*Math.sin(frame.bank),1,-frame.fz*Math.tan(frame.grade)+frame.rz*Math.sin(frame.bank)).normalize();
  }
  function shade(shadow,frame,height,width,length,yaw){
    setFrame(shadow,frame,.026);
    shadow.rotateY(-yaw);
    shadow.scale.set(length*(1+height*.12),1,width*(1+height*.18));
    shadow.material.opacity=.72/(1+height*.88);
  }
  function placeBike(bf,pose,clearance=.014){
    setFrame(bike,bf,pose.bikeHeight);
    bike.quaternion.multiply(spin.setFromEuler(angles.set(pose.bikeRoll,-pose.bikeYaw,pose.bikePitch,'YZX')));
    bikeCOM.copy(bike.position);
    floor(bf);
    let lowest=Infinity;
    for(const point of hull){
      scratch.copy(point).applyQuaternion(bike.quaternion);
      const below=scratch.dot(frameUp);
      if(below<lowest){lowest=below;contact.copy(scratch);}
    }
    const bikeClearance=pose.bikeHeight*frameUp.y+lowest;
    const correction=(clearance-bikeClearance)/frameUp.y;
    bikeCOM.y+=pose.bikeGround?correction:Math.max(0,correction);
    bike.position.copy(bikeCOM).sub(scratch.copy(bikeCenter).applyQuaternion(bike.quaternion));
    contact.add(bikeCOM);
    shade(bikeShadow,bf,Math.max(0,bikeCOM.y-bf.h-.29),1.05,2.55,pose.bikeYaw);
  }
  function parked(bf,pose){
    placeBike(bf,pose,.002);bikeShadow.visible=false;parkedShade.shadow.visible=true;
    parkedShade.update(bikeCOM,bike,bf,floorPoint,frameUp);
    light.visible=true;light.position.copy(bikeCOM).y+=1.45;
  }
  function draw(p,pose){
    const rf=circuit.frame(pose.riderS,pose.riderLateral),bf=circuit.frame(pose.bikeS,pose.bikeLateral);
    setFrame(riderFrame,rf,pose.riderHeight);
    rider.rotation.set(pose.riderRoll,-pose.riderYaw,pose.riderPitch,'YZX');
    action.time=Math.min(clip.duration,Math.max(0,pose.clipTime));mixer.update(0);
    riderFrame.updateMatrixWorld(true);
    floor(rf);
    if(suit&&(pose.riderGround||pose.riderHeight<1.1)){
      let bottom=Infinity;suit.skeleton.update();
      for(let k=0;k<suit.geometry.attributes.position.count;k+=19){
        suit.getVertexPosition(k,scratch).applyMatrix4(suit.matrixWorld).sub(floorPoint);
        bottom=Math.min(bottom,scratch.dot(frameUp));
      }
      const correction=(.018-bottom)/frameUp.y;
      riderFrame.position.y+=pose.riderGround?correction:Math.max(0,correction);
    }
    riderCOM.copy(riderFrame.position);placeBike(bf,pose);
    shade(riderShadow,rf,Math.max(0,riderCOM.y-rf.h-.27),1.05,1.85,pose.riderYaw);
    sparks(contact,bf,p.crashAge,pose.bikeSpeed,pose.bikeHit,pose.bikeGround);

    // An oblique view down the road keeps both trajectories readable as the bodies separate.
    const separation=riderCOM.distanceTo(bikeCOM);
    camTarget.copy(riderCOM).lerp(bikeCOM,.36);
    camTarget.y+=.10;
    const eyeSide=-p.crashSide,along=.62+Math.min(.38,separation*.033);
    towardEye.set(rf.fx*along+rf.rx*eyeSide*.83,.38+Math.min(.19,pose.cameraHeight*.045),
                  rf.fz*along+rf.rz*eyeSide*.83).normalize();
    camRight.crossVectors(UP,towardEye).normalize();
    camUp.crossVectors(towardEye,camRight).normalize();
    const tanY=Math.tan(43*Math.PI/360),tanX=tanY*camera.aspect;
    const fit=(center,radius)=>{
      scratch.subVectors(center,camTarget);
      return scratch.dot(towardEye)+Math.max((Math.abs(scratch.dot(camRight))+radius)/(tanX*.89),
                                            (Math.abs(scratch.dot(camUp))+radius)/(tanY*.87));
    };
    const distance=Math.max(3.85,pose.cameraDistance*.68,fit(riderCOM,.95),fit(bikeCOM,1.23));
    camera.position.copy(camTarget).addScaledVector(towardEye,distance);
    camera.up.copy(UP);camera.lookAt(camTarget);
    light.position.copy(riderCOM).addScaledVector(towardEye,1.2).y+=1.3;
    return Math.atan2(camTarget.z-camera.position.z,camTarget.x-camera.position.x);
  }
  return {visible,draw,parked};
}
