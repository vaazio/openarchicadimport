export function abrange(ids,id){
  return ids.has(id) || ids.has(id.split('.')[0]);
}

export function alvosDaLinha(selecao,id){
  if(id==null)return [];
  const ids=selecao.has(id)?selecao:new Set([id]);
  return [...ids].filter(p=>!p.includes('.') || !ids.has(p.split('.')[0]));
}

export function alterarExclusoes(arvore,excluidos,alvos,restaurar){
  const anteriores=new Set(excluidos),grupo=new Set(alvos),resultado=[];
  for(const p of arvore){
    const folhas=p.ilhas>1?Array.from({length:p.ilhas},(_,j)=>p.id+'.'+j):[p.id];
    const fora=folhas.filter(id=>abrange(grupo,id)?!restaurar:abrange(anteriores,id));
    if(fora.length===folhas.length)resultado.push(p.id);
    else resultado.push(...fora);
  }
  return resultado;
}

export function selecionarLinha(atual,id,ancora,visiveis,{shiftKey,ctrlKey,metaKey}){
  if(shiftKey && visiveis.includes(ancora) && visiveis.includes(id)){
    const a=visiveis.indexOf(ancora),b=visiveis.indexOf(id);
    const faixa=visiveis.slice(Math.min(a,b),Math.max(a,b)+1);
    return new Set(ctrlKey || metaKey?[...atual,...faixa]:faixa);
  }
  if(!ctrlKey && !metaKey)return new Set([id]);
  const proxima=new Set(atual);
  if(proxima.has(id))proxima.delete(id);else proxima.add(id);
  return proxima;
}
