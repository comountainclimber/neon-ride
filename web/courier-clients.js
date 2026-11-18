import * as T from 'three';

const files=['night-porter','arcade-clerk','pump-operator','canal-keeper','cinema-projectionist','clinic-physician'];
const coats=['#a87b50','#94526f','#436c80','#806348','#695775','#789a92'];
const skins=['#9f7460','#ba8a6c','#8d6352','#7f5d50','#ba8778','#b69478'];
const cloth=color=>new T.MeshStandardMaterial({color,roughness:.79});
const shared={pants:cloth('#29363d'),boots:cloth('#202a30'),parcel:cloth('#91785c'),hair:cloth('#252c32')};
const ball=new T.SphereGeometry(1,12,10),bodyShape=new T.CapsuleGeometry(.5,1,5,12),block=new T.BoxGeometry();

function standIn(id){
  const group=new T.Group(),jacket=cloth(coats[id]),skin=cloth(skins[id]);
  const trim=new T.MeshStandardMaterial({color:'#9baaa2',metalness:.45,roughness:.35});
  function mesh(geo,mat,x,y,z,sx,sy,sz,rot=0){
    const a=new T.Mesh(geo,mat);a.position.set(x,y,z);a.scale.set(sx,sy,sz);a.rotation.x=rot;group.add(a);
  }
  for(const sign of [-1,1]){
    mesh(bodyShape,shared.pants,sign*.135,.49,0,.23,.42,.24);mesh(bodyShape,shared.boots,sign*.135,.12,.045,.23,.13,.32);
    mesh(bodyShape,jacket,sign*.255,1.11,.055,.18,.29,.20,-.27);mesh(ball,skin,sign*.24,.91,.165,.079,.10,.077);
  }
  mesh(bodyShape,jacket,0,1.10,0,.44,.29,.29);mesh(ball,skin,0,1.53,.021,.146,.205,.15);
  mesh(ball,shared.hair,0,1.63,-.008,.153,.12,.153);mesh(block,trim,-.116,1.27,.142,.116,.054,.015);
  mesh(block,shared.parcel,0,.924,.213,.34,.22,.20);mesh(block,trim,0,.927,.315,.045,.223,.009);
  return group;
}

// The authored characters stream independently after the street can already be ridden.
export function streetClients(stations,loader){
  let live=0;
  const actors=stations.map(({root,id})=>{
    const mount=new T.Group();mount.name=files[id];mount.position.set(3.95,.004,7.95);mount.rotation.y=-2.52;root.add(mount);
    const proxy=standIn(id);mount.add(proxy);
    return {id,mount,proxy,status:'waiting',retry:0};
  });
  async function stream(a){
    a.status='loading';live++;
    try{
      const gltf=await loader.loadAsync('./assets/models/'+files[a.id]+'.glb');
      const model=gltf.scene;model.updateMatrixWorld(true);
      const bounds=new T.Box3().setFromObject(model),size=bounds.getSize(new T.Vector3());
      if(!Number.isFinite(size.y)||size.y<.001)throw Error('Invalid client bounds');
      if(a.id===2)model.traverse(obj=>{
        if(!obj.isMesh)return;
        // Preserve the worn hard-hat texture under the receiving window's close practical light.
        obj.geometry.computeBoundingBox();
        const b=obj.geometry.boundingBox,y=b.max.y-b.min.y;
        obj.material.onBeforeCompile=shader=>{
          shader.vertexShader='varying float helmet;\n'+shader.vertexShader.replace('#include <begin_vertex>',
            '#include <begin_vertex>\nhelmet=smoothstep('+Number(b.min.y+.921*y).toFixed(6)+','+Number(b.min.y+.954*y).toFixed(6)+',position.y);');
          shader.fragmentShader='varying float helmet;\n'+shader.fragmentShader.replace('#include <map_fragment>',
            '#include <map_fragment>\ndiffuseColor.rgb*=mix(1.0,.48,helmet);').replace('#include <roughnessmap_fragment>',
            '#include <roughnessmap_fragment>\nroughnessFactor=max(roughnessFactor,.69*helmet);');
        };
      });
      const body=new T.Group();const scale=1.72/size.y,center=bounds.getCenter(new T.Vector3());
      model.scale.multiplyScalar(scale);model.position.add(new T.Vector3(-center.x,-bounds.min.y,-center.z).multiplyScalar(scale));body.add(model);
      body.name='Authored client';a.mount.add(body);a.mount.remove(a.proxy);a.status='ready';
    }catch{a.status='waiting';a.retry=performance.now()+60000;}
    finally{live--;}
  }
  return (time,eye,active)=>{
    const ordered=actors.map(a=>{
      const dx=eye.x-stations[a.id].root.position.x,dz=eye.z-stations[a.id].root.position.z;
      return {a,d:dx*dx+dz*dz};
    }).sort((a,b)=>a.d-b.d);
    for(const {a,d} of ordered){
      a.mount.visible=d<220*220;
      if(a.mount.visible){
        const engaged=active===a.id&&d<14*14;
        a.mount.rotation.y=-2.52+.065*Math.sin(time*.46+a.id)+(engaged?.18:0);
      }
      if(live<2&&a.status==='waiting'&&performance.now()>=a.retry)stream(a);
    }
  };
}
