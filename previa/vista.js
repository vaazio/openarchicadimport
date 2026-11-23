import {abrange} from './pecas.js';
import * as T from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { OutlinePass } from 'three/addons/postprocessing/OutlinePass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';

export function criarVista({selecionar, contexto, moverLimite}) {
  const canvas=document.querySelector('#modelo'), area=canvas.parentElement;
  const gl=new T.WebGLRenderer({canvas,antialias:true});
  gl.setPixelRatio(Math.min(devicePixelRatio,2));
  const cena=new T.Scene(); cena.background=new T.Color(0xe6eded);
  const cam=new T.PerspectiveCamera(38,1,.001,1e6); cam.up.set(0,0,1);
  cam.position.set(3,-4,2.5);
  const composer=new EffectComposer(gl);
  composer.addPass(new RenderPass(cena,cam));
  const outline=new OutlinePass(new T.Vector2(1,1),cena,cam);
  outline.visibleEdgeColor.setHex(0x00a6aa);
  outline.hiddenEdgeColor.setHex(0x243735);
  outline.edgeStrength=5;
  composer.addPass(outline);
  composer.addPass(new OutputPass());
  const orb=new OrbitControls(cam,canvas);
  cena.add(new T.HemisphereLight(0xffffff,0xb6bfbe,2));
  const sol=new T.DirectionalLight(0xffffff,2);sol.position.set(4,-3,7);cena.add(sol);
  const objetos=new T.Group(), caixa=new T.Group(), escala=new T.Group();
  const encaixes=new T.Group();
  cena.add(objetos,caixa,escala,encaixes);
  let grade,limites,alvo=null,isolado=null,primeira=true,bounds=new T.Box3(),arraste,inicio;
  const vec=new T.Vector3(),ray=new T.Raycaster(),ponteiro=new T.Vector2();
  function limpar(g){for(const m of [...g.children]){g.remove(m);m.geometry.dispose();m.material.dispose();}}
  function ajustarCamera(){
    if(bounds.isEmpty())return;
    const centro=bounds.getCenter(new T.Vector3()),tam=bounds.getSize(vec).length() || 1;
    const dir=cam.position.clone().sub(orb.target).normalize();
    orb.target.copy(centro);cam.position.copy(centro).addScaledVector(dir,tam*1.65/Math.min(1,cam.aspect));
    orb.update();
  }
  function iluminar(){
    outline.selectedObjects=[];
    for(const m of objetos.children){
      m.visible=!isolado || abrange(isolado,m.name);
      const marcada=alvo && abrange(alvo,m.name);
      m.material.emissive.setHex(marcada?0x18383b:0);
      if(m.visible && marcada) outline.selectedObjects.push(m);
    }
  }
  function mostrar(grupos,cores){
    limpar(objetos);bounds.makeEmpty();
    for(const g of grupos){
      const geom=new T.BufferGeometry();
      const bin=atob(g.pos), bytes=new Uint8Array(bin.length);
      for(let i=0;i<bytes.length;i++)bytes[i]=bin.charCodeAt(i);
      geom.setAttribute('position',new T.BufferAttribute(new Float32Array(bytes.buffer),3));geom.computeVertexNormals();
      const rgb=cores(g);
      const mat=new T.MeshStandardMaterial({color:new T.Color().setRGB(...rgb,T.SRGBColorSpace),side:T.DoubleSide,roughness:.82});
      const m=new T.Mesh(geom,mat);m.name=g.id;m.position.fromArray(g.origem);objetos.add(m);bounds.expandByObject(m);
    }
    if(!bounds.isEmpty()){
      const tamanho=bounds.getSize(vec).length()||1, passo=10**Math.floor(Math.log10(tamanho/3));
      if(grade){cena.remove(grade);grade.geometry.dispose();grade.material.dispose();}
      grade=new T.GridHelper(passo*100,100,0x678784,0xc3cfcc);grade.rotation.x=Math.PI/2;
      grade.position.copy(bounds.getCenter(new T.Vector3()));grade.position.z=bounds.min.z-passo*.005;cena.add(grade);
      escala.position.set(bounds.max.x+.2,bounds.min.y,bounds.min.z);
      if(primeira){ajustarCamera();primeira=false;}
    }
    tema(document.documentElement.classList.contains('escuro'));iluminar();
  }
  // Silhueta humana em metros; a câmera pode orbitar e o perfil acompanha o observador.
  const perfil=[[0,0],[-.12,0],[-.1,.78],[-.19,1.1],[-.24,.8],[-.3,.84],[-.21,1.4],[-.09,1.48],[-.09,1.61],[-.05,1.7],[.05,1.7],[.09,1.61],[.09,1.48],[.21,1.4],[.3,.84],[.24,.8],[.19,1.1],[.1,.78],[.12,0],[.05,0],[0,.69],[-.05,0],[0,0]];
  escala.add(new T.Line(new T.BufferGeometry().setFromPoints(perfil.map(([x,z])=>new T.Vector3(x,0,z))),new T.LineBasicMaterial({color:0x44605a})));
  function mostrarCaixa(c){
    limites=c;limpar(caixa);if(!c)return;
    const min=new T.Vector3(c[0],c[2],c[4]),max=new T.Vector3(c[1],c[3],c[5]);
    const b=new T.Box3(min,max);caixa.add(new T.Box3Helper(b,0x167d78));
    const centro=b.getCenter(new T.Vector3()),raio=Math.max(.000001,b.getSize(vec).length()*.018);
    c.forEach((v,i)=>{
      const m=new T.Mesh(new T.SphereGeometry(raio,10,8),new T.MeshBasicMaterial({color:0x008c83,depthTest:false}));
      m.renderOrder=2;m.position.copy(centro).setComponent(i>>1,v);m.userData.limite=i;caixa.add(m);
    });
  }
  function apontar(e){
    const r=canvas.getBoundingClientRect();ponteiro.set((e.clientX-r.left)/r.width*2-1,1-(e.clientY-r.top)/r.height*2);
    ray.setFromCamera(ponteiro,cam);
  }
  canvas.addEventListener('pointerdown',e=>{
    inicio=[e.clientX,e.clientY];apontar(e);
    const hit=ray.intersectObjects(caixa.children.filter(o=>o.isMesh))[0];
    if(hit && e.button===0){
      const i=hit.object.userData.limite;
      const plano=new T.Plane().setFromNormalAndCoplanarPoint(cam.getWorldDirection(new T.Vector3()),hit.point);
      arraste={i,plano,origem:hit.point.clone(),c:[...limites]};orb.enabled=false;canvas.setPointerCapture(e.pointerId);
      e.stopImmediatePropagation();
    }
  },true);
  canvas.addEventListener('pointermove',e=>{
    if(!arraste)return;apontar(e);const a=arraste, ponto=ray.ray.intersectPlane(a.plano,new T.Vector3());
    if(!ponto)return;
    const c=[...a.c],v=c[a.i]+ponto.sub(a.origem).getComponent(a.i>>1);
    c[a.i]=a.i%2?Math.max(c[a.i-1],v):Math.min(c[a.i+1],v);
    mostrarCaixa(c);moverLimite(c,false);
  });
  canvas.addEventListener('pointerup',e=>{
    if(arraste){arraste=null;orb.enabled=true;moverLimite(limites,true);return;}
    if(!inicio || Math.hypot(e.clientX-inicio[0],e.clientY-inicio[1])>4 || (e.button!==0 && e.button!==2))return;
    apontar(e);const h=ray.intersectObjects(objetos.children.filter(m=>m.visible),false)[0];if(h){if(e.button===2)contexto(h.object.name,e);else selecionar(h.object.name,e);}
  });
  canvas.addEventListener('pointercancel',()=>{if(arraste){mostrarCaixa(arraste.c);moverLimite(arraste.c,false);}arraste=null;orb.enabled=true;});
  new ResizeObserver(()=>{const w=area.clientWidth,h=area.clientHeight;if(!w||!h)return;
    cam.aspect=w/h;cam.updateProjectionMatrix();gl.setSize(w,h,false);composer.setSize(w,h);
  }).observe(area);
  gl.setAnimationLoop(()=>{
    if(canvas.hidden)return;
    const d=Math.max(.00001,cam.position.distanceTo(orb.target));
    cam.near=d/10000;cam.far=Math.max(d*100, bounds.isEmpty()?1:bounds.distanceToPoint(cam.position)+bounds.getSize(vec).length()*2,10);
    cam.updateProjectionMatrix();escala.visible=document.querySelector('#pessoa').checked;
    escala.rotation.z=Math.atan2(cam.position.y-escala.position.y,cam.position.x-escala.position.x)+Math.PI/2;
    const unidade=2*d*Math.tan(T.MathUtils.degToRad(cam.fov)/2)/Math.max(1,area.clientHeight);
    const metros=10**Math.floor(Math.log10(unidade*100));
    const regua=document.querySelector('#regua');regua.style.width=metros/unidade+'px';regua.textContent=metros.toLocaleString('pt-BR')+' m';
    composer.render();
  });
  function tema(escuro){
    cena.background.setHex(escuro?0x262c30:0xe6eded);
    if(grade)grade.material.color.setHex(escuro?0x707981:0xffffff);
    outline.visibleEdgeColor.setHex(escuro?0x58e3df:0x00a6aa);
  }
  function mostrarCaixasSelecao(caixas){
    limpar(encaixes);
    for(const c of caixas??[]){
      if(!c)continue;
      const limites=new T.Box3(new T.Vector3(c[0],c[2],c[4]),new T.Vector3(c[1],c[3],c[5]));
      const helper=new T.Box3Helper(limites,0xbf730d);
      helper.material.depthTest=false;helper.renderOrder=1;encaixes.add(helper);
    }
  }
  return {tema,mostrar,mostrarCaixasSelecao,mostrarCaixa,enquadrar:ajustarCamera,selecionar(id,solo){alvo=id;isolado=solo;iluminar();}};
}
