import '@ui-sdk/web/tree';
import {abrange,alvosDaLinha,alterarExclusoes,selecionarLinha} from './pecas.js';
import {criarPlanta} from './planta.js';
import '@ui-sdk/web/number-input';
import {criarVista} from './vista.js';
const $=id=>document.getElementById(id), pausa=ms=>new Promise(ok=>setTimeout(ok,ms));
const mensagem=t=>{$('estado').textContent=t;};
const nomes=['X','Y','Z'],formatar=v=>v.toLocaleString('pt-BR',{maximumSignificantDigits:6});
let meta,vista,dados,estado,selecionado,solo,geracao=0,pronta=0,ticket=0,limites;
const historico=[],historicoPlanta=[];
const substituicoes=new Map(),rascunhos=[];
function preencherTema(escuro){
  document.documentElement.classList.toggle('escuro',!!escuro);
  vista?.tema(!!escuro);
}
let selecao=new Set(),ancora,estruturaArvore;
const nomesPecas=new Map();
const icones={
  excluir:'M4 6h16 M9 6V3h6v3 M6 6l1 15h10l1-15 M10 10v7 M14 10v7',
  restaurar:'M3 12s3-7 9-7 9 7 9 7-3 7-9 7-9-7-9-7z M9 12a3 3 0 1 0 6 0 3 3 0 1 0-6 0',
  desfazer:'M9 4L3 10l6 6 M4 10h10a6 6 0 0 1 0 12',
  isolar:'M3 9V3h6 M15 3h6v6 M21 15v6h-6 M9 21H3v-6 M8 8h8v8H8z'
};
function dica(botao,txt){
  botao.title=txt;botao.setAttribute('aria-label',txt);
  if(document.activeElement===botao)$('dica').textContent=txt;
}
function ultima(id){
  return historico.findLast(h=>[...h.anteriores.keys()].some(chave=>pertence(chave,id)));
}
function desfazer(id){
  const h=ultima(id);if(!h)return;
  for(const [chave,antes] of h.anteriores){
    if(!pertence(chave,id))continue;
    if(h.tipo==='reducao'){
      if(antes===undefined)delete estado.excecoes[chave];else estado.excecoes[chave]=antes;
    }else estado.excluidos=alterarExclusoes(dados.arvore,estado.excluidos,[chave],!antes);
    h.anteriores.delete(chave);
  }
  if(!h.anteriores.size)historico.splice(historico.indexOf(h),1);
  sincronizar();atualizar();
}
function agir(id,acao,alvos=alvosDaLinha(selecao,id)){
  if(acao==='desfazer'){desfazer(id);return;}
  if(acao==='isolar'){solo=solo?null:new Set(alvos);realcar();return;}
  const restaurar=acao==='restaurar' || (acao==='excluir' && excluido(id));
  const grupo=new Set(alvos),anteriores=new Map();
  for(const folha of Object.keys(dados.contagens)){
    if(abrange(grupo,folha) && excluido(folha)===restaurar)anteriores.set(folha,restaurar);
  }
  if(!anteriores.size)return;
  historico.push({tipo:restaurar?'restauração':'exclusão',anteriores});
  editar(()=>{estado.excluidos=alterarExclusoes(dados.arvore,estado.excluidos,alvos,restaurar);});
}
function botaoDaLinha(id,acao){
  const b=document.createElement('button');b.type='button';b.dataset.acao=acao;
  b.innerHTML='<svg aria-hidden="true" viewBox="0 0 24 24"><path/></svg>';
  b.onfocus=()=>{
    const r=b.getBoundingClientRect(),t=$('dica');
    t.textContent=b.getAttribute('aria-label');t.hidden=false;
    t.style.left=Math.max(2,Math.min(r.left,innerWidth-t.offsetWidth-2))+'px';
    t.style.top=(r.bottom+t.offsetHeight<innerHeight?r.bottom+2:r.top-t.offsetHeight-2)+'px';
  };
  b.onblur=()=>{$('dica').hidden=true;};
  b.onkeydown=e=>e.stopPropagation();
  b.onclick=e=>{e.stopPropagation();agir(id,acao);};
  return b;
}
let planta, emPlanta=false, chaves=[], escondidas=new Set();
const padroes={collapse:100,planar:5,grade:1};
const pertence=(id,pai)=>id===pai || id.startsWith(pai+'.');
function folhasSelecionadas(){
  return Object.keys(dados?.contagens??{}).filter(id=>abrange(selecao,id));
}
function ajusteAtual(){
  if($('escopo').value==='geral')return estado.geral;
  const ajustes=folhasSelecionadas().map(id=>estado.excecoes[id]??estado.geral);
  const primeiro=ajustes[0]??estado.geral;
  if(ajustes.some(a=>a.metodo!==primeiro.metodo))return null;
  return ajustes.some(a=>a.valor!==primeiro.valor)?{metodo:primeiro.metodo,valor:null}:primeiro;
}
function definirAjuste(ajuste){
  if($('escopo').value==='geral'){estado.geral=ajuste;return;}
  const anteriores=new Map();
  for(const id of folhasSelecionadas()){
    const anterior=estado.excecoes[id];
    if(ajuste?anterior?.metodo===ajuste.metodo && anterior.valor===ajuste.valor:!anterior)continue;
    anteriores.set(id,anterior);
    if(ajuste)estado.excecoes[id]=ajuste;else delete estado.excecoes[id];
  }
  if(anteriores.size)historico.push({tipo:'reducao',anteriores});
}
function controlesReducao(){
  const ajuste=ajusteAtual(),metodo=ajuste?.metodo??'misto',v=$('valor');
  const ids=folhasSelecionadas(),porSelecao=$('escopo').value==='selecao';
  const semAlvo=porSelecao && !ids.length;
  $('metodo').disabled=semAlvo;v.disabled=semAlvo || !ajuste;
  $('metodo').value=metodo;v.value=ajuste?.valor??'';v.placeholder=ajuste?.valor==null?'Misto':'';
  v.min=metodo==='planar'?0:1;v.max={planar:89,grade:8,collapse:100}[metodo]??100;v.step=metodo==='planar'?0.1:1;
  $('intensidade').textContent={planar:'Ângulo máximo (°)',grade:'Iterações',collapse:'Triângulos a manter (%)',misto:'Intensidade'}[metodo];
  $('herdar').hidden=!porSelecao;
  $('herdar').disabled=!ids.some(id=>Object.hasOwn(estado.excecoes,id));
  const pecas=ids.map(id=>dados.contagens[id]);
  const soma=k=>formatar(pecas.reduce((n,p)=>n+p[k],0));
  $('selecao').textContent=pecas.length?(selecao.size===1?nomesPecas.get(selecionado):selecao.size+' selecionadas')+' · '+soma('fonte')+' faces · '+soma('antes')+' → '+soma('depois')+' triângulos':'';
  $('disponibilidade').textContent=pecas.some(p=>p.indisponivel)?'Grade indisponível: '+ids.filter(id=>dados.contagens[id].indisponivel).map(id=>nomesPecas.get(id)??(nomesPecas.get(id.split('.')[0])+' · ilha '+(Number(id.split('.')[1])+1))).join(', '):Object.values(dados?.contagens??{}).some(p=>p.indisponivel)?'Há peças sem grade reconhecida.':'';
}
function atualizarOcultas(){
  const ocultas=new Set(estado.trechos),pecas=new Set(estado.semLinhas);
  escondidas=new Set();
  dados?.linhas.forEach(([id],i)=>{
    if(pecas.has(id) || pecas.has(id.split('.')[0]) || ocultas.has(chaves[i]))escondidas.add(i);
  });
  const reconhecidas=new Set(chaves);
  const revisar=estado.trechos.filter(k=>!reconhecidas.has(k)).length;
  $('revisar').textContent=revisar?revisar+' trecho(s) editado(s) para revisar nesta projeção.':'';
}
function botoesPlanta(){
  $('ocultar').disabled=pronta!==geracao || !planta?.marcadas.size || $('comparar').value==='antes';
  const ocultas=new Set(estado.semLinhas);
  $('ocultarPeca').disabled=!selecao.size || [...selecao].every(id=>abrange(ocultas,id));
  $('ocultarPeca').textContent=selecao.size>1?'Ocultar linhas das peças selecionadas':'Ocultar linhas desta peça na planta';
  $('desfazerPlanta').disabled=!historicoPlanta.length;
  $('restaurar').disabled=!estado.semLinhas.length && !estado.trechos.length;
}
function abrirAba(pl){
  emPlanta=pl;
  $('modelo').hidden=pl;$('planta').hidden=!pl;$('regua').hidden=pl;$('referencia').hidden=pl;
  $('zoom').hidden=!pl;$('edicaoPlanta').hidden=!pl;
  $('tab2d').setAttribute('aria-selected',String(pl));$('tab3d').setAttribute('aria-selected',String(!pl));
  if(pl)planta.abrir();
}

async function enviar(acao,d={}){
  const raw=await window.Importador.enviar(JSON.stringify({acao,...d}));
  if(!raw)return null;
  const resposta=JSON.parse(raw);
  if(resposta?.erro)throw Error(resposta.erro);
  return resposta;
}
function editar(fn){
  const {semLinhas,trechos}=estado;
  fn();
  if(semLinhas!==estado.semLinhas || trechos!==estado.trechos)historicoPlanta.push({semLinhas,trechos});
  sincronizar();atualizar();
}
function arvore(){
  const el=$('arvore'),estrutura=JSON.stringify([estado.ilhas,dados.arvore]);
  if(estrutura===estruturaArvore){realcar();return;}
  const primeira=estruturaArvore===undefined;
  estruturaArvore=estrutura;
  const abertos=el.api?.getExpanded()??[];
  $('dica').hidden=true;el.replaceChildren();nomesPecas.clear();
  function item(id,nome,caminho){
    const no=document.createElement('ui-tree-item');no.setAttribute('value',id);
    nomesPecas.set(id,nome);
    const label=document.createElement('ui-tree-item-label');
    const texto=document.createElement('span');texto.className='nomePeca';texto.textContent=nome;texto.title=caminho;
    label.append(texto);
    for(const acao of ['excluir','desfazer','isolar'])label.append(botaoDaLinha(id,acao));
    label.onclick=e=>{
      if(e.target.closest('ui-tree-item-indicator'))return;
      e.stopPropagation();
      no.focus({preventScroll:true});selecionar(id,false,e);
    };
    no.append(label);return no;
  }
  for(const p of dados.arvore){
    const pai=item(p.id,p.nome,p.caminho);
    if(estado.ilhas && p.ilhas>1){
      const grupo=document.createElement('ui-tree-group');
      const indicador=document.createElement('ui-tree-item-indicator');indicador.textContent='▸ ';
      pai.firstChild.prepend(indicador);
      for(let j=0;j<p.ilhas;j++)grupo.append(item(p.id+'.'+j,'Ilha '+(j+1),p.caminho));
      pai.append(grupo);
    }
    el.append(pai);
  }
  if(!estado.ilhas)selecao=new Set([...selecao].map(id=>id.split('.')[0]));
  if(primeira && dados.arvore.length)selecao.add(dados.arvore[0].id);
  if(!selecao.has(selecionado))selecionado=[...selecao].at(-1);
  el.api?.setExpanded(abertos.filter(id=>nomesPecas.has(id)));
  el.api?.setValue([...selecao]);realcar();
}
function excluido(id){return estado.excluidos.includes(id)||estado.excluidos.includes(id.split('.')[0]);}
function realcar(){
  const totalAlvos=alvosDaLinha(selecao,selecionado).length;
  for(const n of $('arvore').querySelectorAll('ui-tree-item')){
    const id=n.getAttribute('value'),fora=excluido(id);
    n.toggleAttribute('data-ativo',selecao.has(id));n.toggleAttribute('data-fora',fora);
    const quantidade=selecao.has(id)?totalAlvos:1;
    const alvo=quantidade>1?quantidade+' peças':'peça';
    for(const botao of n.firstElementChild.querySelectorAll('button')){
      const acao=botao.dataset.acao;
      const icone=acao==='excluir' && fora?'restaurar':acao;
      botao.querySelector('path').setAttribute('d',icones[icone]);
      dica(botao,acao==='desfazer'?(ultima(id)?'Desfazer '+(ultima(id).tipo==='reducao'?'redução':ultima(id).tipo)+' de '+nomesPecas.get(id):'Sem alteração para desfazer'):acao==='isolar'?(solo?'Mostrar todas':'Isolar '+alvo):(fora?'Restaurar ':'Excluir ')+alvo);
      botao.disabled=acao==='desfazer' && !ultima(id);
      if(acao==='isolar')botao.setAttribute('aria-pressed',String(!!solo && abrange(solo,id)));
    }
  }
  vista?.selecionar(selecao,solo);
  planta?.selecionar(selecao,solo,escondidas);
  controlesReducao();botoesPlanta();
}
function selecionar(id,daVista=false,e={}){
  if(id!=null){
    if(!estado.ilhas)id=id.split('.')[0];
    const el=$('arvore');
    if(daVista && id.includes('.'))el.api?.expand(id.split('.')[0]);
    const visiveis=e.shiftKey?[...el.querySelectorAll('ui-tree-item')].filter(n=>n.getClientRects().length).map(n=>n.getAttribute('value')):[];
    selecao=selecionarLinha(selecao,id,ancora,visiveis,e);
    if(!e.shiftKey)ancora=id;
    selecionado=selecao.has(id)?id:[...selecao].at(-1);
    el.api?.setValue([...selecao]);
    if(daVista)el.querySelector('[value="'+id+'"]')?.scrollIntoView({block:'nearest'});
  }
  realcar();
}
$('arvore').addEventListener('valuechange',e=>{
  if(e.detail.reason==='imperative' || !estado)return;
  selecao=new Set(e.detail.value);
  const id=e.detail.event?.target?.closest('ui-tree-item')?.getAttribute('value');
  selecionado=selecao.has(id)?id:[...selecao].at(-1);ancora=selecionado;realcar();
});
function limitesUI(c){if(c) for(let i=0;i<6;i++)$('l'+i).value=Number(c[i].toPrecision(9));}
function sincronizar(){
  $('ilhas').checked=estado.ilhas;$('unidade').value=estado.unidade;$('eixo').value=estado.y?'Y':'Z';
  $('estilo').value=estado.contornos?'contornos':'vincos';
  $('cortar').checked=!!estado.corte;$('limites').hidden=!estado.corte;limitesUI(estado.corte);
  for(let i=0;i<estado.vinculos.length;i++)$('mat'+i).textContent=meta.superficies[estado.vinculos[i]].nome;
  realcar();
}
async function atualizar(){
  const versao=++geracao;pronta=0;$('criar').disabled=true;mensagem('Calculando prévia…');
  await pausa(55);if(versao!==geracao)return;
  try{
    const id=await enviar('editar',{escala:estado.unidade*estado.fator,y:estado.y,ilhas:estado.ilhas,excluidos:estado.excluidos,
      corte:estado.corte,geral:estado.geral,excecoes:estado.excecoes,antes:$('comparar').value==='antes',contornos:estado.contornos,caixas:estado.caixas.map(c=>c.membros)});
    let r;
    while(versao===geracao){r=await enviar('ler',{id});if(r)break;await pausa(60);}
    if(versao!==geracao)return;
    dados=r;ticket=id;limites=r.limites;
    const escala=estado.unidade*estado.fator;
    chaves=r.linhas.map(([id,l])=>[id,estado.y,...l.flat().map(v=>(v/escala).toPrecision(12))].join('|'));
    atualizarOcultas();planta.mostrar(r,estado.corte,meta.caneta);
    vista?.mostrar(r.grupos,g=>superficie(vinculo(g)).cor);
    vista?.mostrarCaixa(estado.corte);vista?.mostrarCaixasSelecao(r.caixas);arvore();
    listarCaixas();
    $('medidas').textContent=r.final?nomes.map((_,i)=>formatar(limites[i*2+1]-limites[i*2])).join(' × ')+' m':'Seleção vazia';
    $('contagem').textContent='Conjunto · '+formatar(r.original)+' faces de origem · '+formatar(r.corte)+' → '+formatar(r.reduzido)+' triângulos';
    pronta=versao;$('criar').disabled=!r.final || $('comparar').value==='antes';botoesPlanta();
    mensagem($('comparar').value==='antes'?'Antes da redução':r.final?'Prévia atualizada':'Inclua uma peça ou ajuste o corte.');
  }catch(e){if(versao===geracao)mensagem(e.message);}
}
function listarCaixas(){
  const painel=$('caixas');painel.hidden=!estado.caixas.length;
  const lista=$('listaCaixas');lista.replaceChildren();
  for(const caixa of estado.caixas){
    const linha=document.createElement('div'),remover=document.createElement('button');
    linha.append(document.createTextNode(caixa.nome+' '));
    remover.textContent='Remover';remover.setAttribute('aria-label','Remover caixa de '+caixa.nome);
    remover.onclick=()=>editar(()=>estado.caixas=estado.caixas.filter(c=>c!==caixa));
    linha.append(remover);lista.append(linha);
  }
}
function superficie(i){return i<0?rascunhos[-1-i]:meta.superficies[i];}
function vinculo(g){return substituicoes.get(g.id)??estado.vinculos[g.mat];}
function abrirMenu(idMalha,e){
  if(pronta!==geracao)return;
  const g=dados.grupos.find(grupo=>grupo.id===idMalha);
  if(!g)return;
  const id=estado.ilhas?idMalha:idMalha.split('.')[0],menu=$('contexto');
  const titulo=$('tituloMenu'),escopo=$('alvoMenu');
  titulo.textContent=nomesPecas.get(id)??id;
  const grupo=alvosDaLinha(selecao,id);
  escopo.replaceChildren(new Option('Esta peça','peca'));
  if(grupo.length>1)escopo.add(new Option('Seleção ('+grupo.length+' peças)','grupo',true,true));
  escopo.hidden=escopo.length===1;
  const alvos=()=>escopo.value==='grupo'?grupo:[id];
  const folhas=()=>Object.keys(dados.contagens).filter(chave=>alvos().some(p=>pertence(chave,p)));
  function aplicar(indices,valor){
    for(const chave of indices){if(valor===null)substituicoes.delete(chave);else substituicoes.set(chave,valor);}
    atualizar();
  }
  $('atualMenu').textContent='Superfície: '+superficie(vinculo(g)).nome;
  function atualizarMenu(){ $('origemMenu').hidden=!folhas().some(chave=>substituicoes.has(chave)); }
  escopo.onchange=()=>{atualizarMenu();$('ocultarMenu').textContent=escopo.value==='grupo'?'Ocultar seleção':'Ocultar peça';$('isolarMenu').textContent=solo?'Mostrar todas':escopo.value==='grupo'?'Isolar seleção':'Isolar peça';};
  escopo.onchange();
  $('caixaMenu').onclick=()=>{
    menu.hidden=true;
    const membros=folhas(),nome=alvos().map(chave=>nomesPecas.get(chave)??chave).join(', ');
    editar(()=>estado.caixas.push({membros,nome}));
  };
  $('ocultarMenu').onclick=()=>{menu.hidden=true;agir(id,'ocultar',alvos());};
  $('isolarMenu').onclick=()=>{menu.hidden=true;agir(id,'isolar',alvos());};
  $('origemMenu').onclick=()=>{menu.hidden=true;aplicar(folhas(),null);};
  const escolher=base=>enviar('superficie',{vinculo:base,x:e.clientX/innerWidth,y:e.clientY/innerHeight});
  let base=vinculo(g);
  if(base<0)base=superficie(base).base;
  $('vincularMenu').onclick=async()=>{
    menu.hidden=true;
    try{const v=await escolher(base);if(v!=null)aplicar(folhas(),v);}catch(err){mensagem(err.message);}
  };
  $('duplicarMenu').onclick=()=>{
    const destinos=folhas(),dialogo=$('duplicar');
    menu.hidden=true;
    function preencher(){
      const s=meta.superficies[base];
      $('baseCopia').textContent=s.nome;$('nomeCopia').value=s.nome+' (cópia)';
      $('corCopia').value='#'+s.cor.map(v=>Math.round(v*255).toString(16).padStart(2,'0')).join('');
      $('nomeCopia').setCustomValidity('');
    }
    preencher();dialogo.showModal();
    $('baseCopia').onclick=async()=>{try{const v=await escolher(base);if(v!=null){base=v;preencher();}}catch(err){mensagem(err.message);}};
    $('nomeCopia').oninput=()=>$('nomeCopia').setCustomValidity('');
    $('confirmarCopia').onclick=()=>{
      const campo=$('nomeCopia'),nome=campo.value.trim();
      const usadas=[...new Set(substituicoes.values())].filter(i=>i<0).map(superficie);
      const existente=[...meta.superficies,...usadas].some(s=>s.nome.localeCompare(nome,'pt-BR',{sensitivity:'accent'})===0);
      campo.setCustomValidity(!nome?'Informe o nome da superfície.':existente?'Já existe uma superfície com este nome.':'');
      if(!campo.reportValidity())return;
      const cor=$('corCopia').value.slice(1).match(/../g).map(v=>parseInt(v,16)/255);
      const indice=-rascunhos.push({base,nome,cor});
      dialogo.close();aplicar(destinos,indice);
    };
  };
  menu.hidden=false;
  menu.style.left=Math.max(0,Math.min(e.clientX,innerWidth-menu.offsetWidth))+'px';
  menu.style.top=Math.max(0,Math.min(e.clientY,innerHeight-menu.offsetHeight))+'px';
  $('ocultarMenu').focus();
}
document.addEventListener('pointerdown',e=>{if(!e.target.closest('#contexto'))$('contexto').hidden=true;},true);
document.addEventListener('keydown',e=>{if(e.key==='Escape')$('contexto').hidden=true;});
async function iniciar(){
  for(let i=0;!window.Importador && i<200;i++)await pausa(50);
  meta=await enviar('inicio');estado={vinculos:meta.vinculos,excluidos:[],caixas:[],ilhas:false,y:meta.y,unidade:1,fator:1,corte:null,
    geral:{metodo:'collapse',valor:100},excecoes:{},contornos:false,semLinhas:[],trechos:[]};
  $('nome').value=meta.nome;$('nome').maxLength=meta.limiteNome;
  planta=criarPlanta($('planta'),(id,e)=>selecionar(id,true,e));
  try{
    vista=criarVista({selecionar:(id,e)=>selecionar(id,true,e),contexto:abrirMenu,moverLimite:(c,fim)=>{limitesUI(c);if(fim)editar(()=>estado.corte=c);}});
  }catch(e){$('tab3d').disabled=true;$('tab3d').title=e.message;abrirAba(true);}
  preencherTema(meta.escuro);
  setInterval(async()=>{try{const escuro=await enviar('tema');if(typeof escuro==='boolean')preencherTema(escuro);}catch{}},1500);
  $('tab3d').onclick=()=>abrirAba(false);$('tab2d').onclick=()=>abrirAba(true);
  $('mais').onclick=()=>planta.zoom(1.3);$('menos').onclick=()=>planta.zoom(1/1.3);
  $('estilo').onchange=()=>editar(()=>estado.contornos=$('estilo').value==='contornos');
  $('ocultar').onclick=()=>editar(()=>{estado.trechos=[...new Set([...estado.trechos,...Array.from(planta.marcadas,i=>chaves[i])])];});
  $('ocultarPeca').onclick=()=>editar(()=>{estado.semLinhas=[...new Set([...estado.semLinhas,...alvosDaLinha(selecao,selecionado)])];});
  $('desfazerPlanta').onclick=()=>{const anterior=historicoPlanta.pop();if(anterior){Object.assign(estado,anterior);sincronizar();atualizar();}};
  $('restaurar').onclick=()=>editar(()=>{estado.trechos=[];estado.semLinhas=[];});
  meta.materiais.forEach((nome,i)=>{
    const label=document.createElement('label');label.textContent=nome;
    const botao=document.createElement('button');botao.type='button';botao.id='mat'+i;
    botao.setAttribute('aria-haspopup','dialog');
    botao.onclick=async()=>{
      const r=botao.getBoundingClientRect();botao.disabled=true;
      try{
        const novo=await enviar('superficie',{vinculo:estado.vinculos[i],x:r.left/innerWidth,y:r.bottom/innerHeight});
        if(novo!=null && novo!==estado.vinculos[i])editar(()=>estado.vinculos[i]=novo);
      }catch(e){mensagem(e.message);}
      finally{botao.disabled=false;botao.focus();}
    };
    label.append(botao);$('materiais').append(label);
  });
  nomes.forEach((eixo,j)=>{
    const linha=document.createElement('label');linha.textContent=eixo;
    for(let k=0;k<2;k++){
      const i=j*2+k,input=document.createElement('input');input.type='number';input.step='any';input.id='l'+i;
      input.setAttribute('aria-label',eixo+(k?' máximo':' mínimo')+' em metros');
      input.onchange=()=>{const v=input.valueAsNumber;
        if(!Number.isFinite(v)||(k?v<estado.corte[i-1]:v>estado.corte[i+1])){limitesUI(estado.corte);mensagem('Confira os limites do eixo '+eixo);return;}
        editar(()=>estado.corte[i]=v);
      };linha.append(input);
    }$('limites').append(linha);
  });
  $('ilhas').onchange=()=>editar(()=>{estado.ilhas=$('ilhas').checked;solo=null;if(!estado.ilhas && selecionado)selecionado=selecionado.split('.')[0];});
  $('enquadrar').onclick=()=>emPlanta?planta.enquadrar():vista.enquadrar();
  function reescalar(razao){if(estado.corte)estado.corte=estado.corte.map(v=>v*razao);}
  $('unidade').onchange=()=>editar(()=>{const u=Number($('unidade').value);reescalar(u/estado.unidade);estado.unidade=u;});
  $('eixo').onchange=()=>editar(()=>{estado.y=$('eixo').value==='Y';estado.corte=null;});
  $('calibrar').onclick=()=>{const i=$('dim').selectedIndex*2,d=limites?.[i+1]-limites?.[i],v=Number($('conhecida').value);
    if(pronta!==geracao || !(v>0&&d>0)){mensagem('Informe uma medida positiva para uma dimensão com extensão.');return;}
    editar(()=>{estado.fator*=v/d;reescalar(v/d);});
  };
  $('cortar').onchange=()=>{if($('cortar').checked && (!dados?.final||pronta!==geracao)){$('cortar').checked=false;return;}
    editar(()=>estado.corte=$('cortar').checked?[...limites]:null);};
  $('escopo').onchange=controlesReducao;
  $('metodo').onchange=()=>editar(()=>{const metodo=$('metodo').value;definirAjuste({metodo,valor:padroes[metodo]});});
  $('valor').onchange=()=>{
    const input=$('valor');
    if(!input.checkValidity() || !Number.isFinite(input.valueAsNumber)){input.reportValidity();controlesReducao();return;}
    editar(()=>definirAjuste({metodo:ajusteAtual().metodo,valor:input.valueAsNumber}));
  };
  $('herdar').onclick=()=>editar(()=>definirAjuste(null));
  $('comparar').onchange=atualizar;
  $('criar').onclick=async()=>{if(pronta!==geracao)return;
    const nome=$('nome').value.trim();$('nome').value=nome;
    if(!nome || nome.length>meta.limiteNome){mensagem('Confira o nome do objeto.');$('nome').focus();return;}
    $('criar').disabled=true;
    try{
      const r=await enviar('criar',{id:ticket,vinculos:estado.vinculos,nome,acabamentos:dados.grupos.map(g=>substituicoes.get(g.id)??null),rascunhos,ocultas:[...escondidas]});
      if(r?.sugestao){$('nome').value=r.sugestao;$('nome').focus();mensagem('Nome em uso. Confirme a sugestão ou edite o nome.');}
    }catch(e){mensagem(e.message);}
    finally{$('criar').disabled=pronta!==geracao || !dados.final || $('comparar').value==='antes';}
  };
  sincronizar();atualizar();
}
$('cancelar').onclick=()=>enviar('cancelar');
iniciar().catch(e=>mensagem('Prévia indisponível: '+e.message+'. Use o comando de seleção simples.'));

