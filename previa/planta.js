import {abrange} from './pecas.js';

export function criarPlanta(canvas, selecionar) {
  const ctx=canvas.getContext('2d');
  let caixas=[],linhas=[], limites, corte, peca, solo, cor='#253137', arraste, camera;
  let ocultas=new Set(), marcadas=new Set(), fundoEscuro=false;
  const pos=e=>{const r=canvas.getBoundingClientRect();return [e.clientX-r.left,e.clientY-r.top];};
  const tela=([x,y])=>[(x-camera.x)*camera.k+canvas.clientWidth/2,(camera.y-y)*camera.k+canvas.clientHeight/2];
  function visivel(i){return !ocultas.has(i) && (!solo || abrange(solo,linhas[i][0]));}
  function desenhar(){
    if(canvas.hidden || !camera)return;
    const w=canvas.clientWidth,h=canvas.clientHeight,dpr=Math.min(devicePixelRatio,2);
    if(!w || !h)return;
    if(canvas.width!==Math.round(w*dpr) || canvas.height!==Math.round(h*dpr)){
      canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);
    }
    ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
    for(let i=0;i<linhas.length;++i){
      if(!visivel(i))continue;
      const [id,[a,b]]=linhas[i],ativa=peca && abrange(peca,id);
      ctx.strokeStyle=marcadas.has(i)?(fundoEscuro?'#ffae71':'#cc4b10'):ativa?(fundoEscuro?'#68d9e1':'#006e76'):cor;
      ctx.lineWidth=marcadas.has(i)?3:ativa?2:1.4;
      ctx.beginPath();ctx.moveTo(...tela(a));ctx.lineTo(...tela(b));ctx.stroke();
    }
    ctx.strokeStyle=fundoEscuro?'#ffba64':'#9e6114';ctx.lineWidth=1;ctx.setLineDash([3,4]);
    for(const c of caixas){
      if(!c)continue;
      const a=tela([c[0],c[3]]),b=tela([c[1],c[2]]);
      ctx.strokeRect(...a,b[0]-a[0],b[1]-a[1]);
    }
    ctx.setLineDash([]);
    if(corte){
      const a=tela([corte[0],corte[3]]),b=tela([corte[1],corte[2]]);
      ctx.setLineDash([5,4]);ctx.strokeStyle='#428785';ctx.lineWidth=1;
      ctx.strokeRect(...a,b[0]-a[0],b[1]-a[1]);ctx.setLineDash([]);
    }
    if(arraste && !arraste.pan && arraste.fim){
      const [x,y]=arraste.inicio,dx=arraste.fim[0]-x,dy=arraste.fim[1]-y;
      ctx.fillStyle='#00888a22';ctx.strokeStyle='#00888a';ctx.lineWidth=1;
      ctx.fillRect(x,y,dx,dy);ctx.strokeRect(x,y,dx,dy);
    }
  }
  function enquadrar(){
    if(!limites || !canvas.clientWidth || limites[0]>limites[1])return;
    const [x,X,y,Y]=limites;
    camera={x:x+(X-x)/2,y:y+(Y-y)/2,k:.8*Math.min(canvas.clientWidth/Math.max(1e-9,X-x),canvas.clientHeight/Math.max(1e-9,Y-y))};
    desenhar();
  }
  function zoom(f,p=[canvas.clientWidth/2,canvas.clientHeight/2]){
    if(!camera)return;
    const k=camera.k,novo=Math.min(1e14,Math.max(1e-12,k*f));
    camera.x+=(p[0]-canvas.clientWidth/2)*(1/k-1/novo);
    camera.y-=(p[1]-canvas.clientHeight/2)*(1/k-1/novo);
    camera.k=novo;desenhar();
  }
  // Recorte paramétrico em coordenadas de tela, incluindo linhas que atravessam o retângulo.
  function cruza(a,b,min,max){
    let lo=0,hi=1;
    for(let k=0;k<2;k++){
      const d=b[k]-a[k];
      if(!d){if(a[k]<min[k] || a[k]>max[k])return false;continue;}
      const u=(min[k]-a[k])/d,v=(max[k]-a[k])/d;
      lo=Math.max(lo,Math.min(u,v));hi=Math.min(hi,Math.max(u,v));
    }
    return lo<=hi;
  }
  canvas.onwheel=e=>{e.preventDefault();zoom(Math.exp(-Math.sign(e.deltaY)*.15),pos(e));};
  canvas.oncontextmenu=e=>e.preventDefault();
  canvas.onpointerdown=e=>{
    if(!camera)return;
    arraste={inicio:pos(e),x:camera.x,y:camera.y,pan:e.button!==0 || e.altKey};
    canvas.setPointerCapture(e.pointerId);
  };
  canvas.onpointermove=e=>{
    if(!arraste)return;
    const p=pos(e);arraste.fim=p;
    if(arraste.pan){camera.x=arraste.x-(p[0]-arraste.inicio[0])/camera.k;camera.y=arraste.y+(p[1]-arraste.inicio[1])/camera.k;}
    desenhar();
  };
  canvas.onpointercancel=()=>{arraste=null;desenhar();};
  canvas.onpointerup=e=>{
    if(!arraste)return;
    const {inicio,pan}=arraste;arraste=null;
    if(pan){desenhar();return;}
    const p=pos(e),clique=Math.hypot(p[0]-inicio[0],p[1]-inicio[1])<=4;
    if(!e.shiftKey)marcadas.clear();
    let maisPerto=-1,distancia=7,primeiro=-1;
    const min=p.map((v,k)=>Math.min(v,inicio[k])),max=p.map((v,k)=>Math.max(v,inicio[k]));
    for(let i=0;i<linhas.length;i++){
      if(!visivel(i))continue;
      const [a,b]=linhas[i][1].map(tela);
      if(!clique){if(cruza(a,b,min,max)){marcadas.add(i);if(primeiro<0)primeiro=i;}continue;}
      const dx=b[0]-a[0],dy=b[1]-a[1],t=Math.max(0,Math.min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy || 1)));
      const d=Math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy);
      if(d<distancia){distancia=d;maisPerto=i;}
    }
    if(maisPerto>=0){
      if(e.shiftKey && marcadas.has(maisPerto))marcadas.delete(maisPerto);else marcadas.add(maisPerto);
      primeiro=maisPerto;
    }
    selecionar(primeiro>=0?linhas[primeiro][0]:null,{ctrlKey:e.ctrlKey,metaKey:e.metaKey});
    desenhar();
  };
  new ResizeObserver(()=>{if(!camera)enquadrar();else desenhar();}).observe(canvas);
  return {zoom,enquadrar,desenhar,marcadas,
    mostrar(r,caixa,pen){caixas=r.caixas??[];linhas=r.linhas;limites=r.limites;corte=caixa;cor='rgb('+pen.map(v=>Math.round(255*v)).join(',')+')';
      const linear=pen.map(v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4);
      fundoEscuro=.2126*linear[0]+.7152*linear[1]+.0722*linear[2]>.32;
      canvas.style.backgroundColor=fundoEscuro?'#202428':'';
      marcadas.clear();if(!camera)enquadrar();desenhar();},
    selecionar(id,isolado,escondidas){peca=id;solo=isolado;ocultas=escondidas;desenhar();},
    abrir(){if(!camera)enquadrar();else desenhar();}
  };
}
