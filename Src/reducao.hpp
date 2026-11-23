#pragma once
#include "edicao.hpp"
#include <earcut.hpp>
#include <optional>

namespace previa {
using Borda=std::array<Ponto,2>;
inline Borda borda(Ponto a,Ponto b){return a<b?Borda{a,b}:Borda{b,a};}
inline double produto(Ponto a,Ponto b){return std::inner_product(a.begin(),a.end(),b.begin(),0.);}
inline Ponto diferenca(Ponto a,Ponto b){for(unsigned k=0;k<3;++k)a[k]-=b[k];return a;}
inline bool intermediario(Ponto a,Ponto p,Ponto b)
{
    auto u=diferenca(p,a),v=diferenca(b,a);
    double vv=produto(v,v);
    if(vv==0) return false;
    double t=produto(u,v)/vv;
    if(t<=0 || t>=1) return false;
    for(unsigned k=0;k<3;++k) u[k]-=t*v[k];
    return produto(u,u)<=1e-20*vv;
}

inline std::vector<Face> triangularRegiao(const std::vector<Face>& faces,const std::vector<size_t>& regiao,
                                          const std::map<Borda,std::vector<size_t>>& adj,Ponto normal)
{
    // Seguir cada anel orientado. Ramos, auto-contatos e regiões ambíguas conservam a malha de entrada.
    std::map<Ponto,Ponto> seguinte;
    std::set<Ponto> protegidos;
    std::set<size_t> dentro(regiao.begin(),regiao.end());
    for(auto i:regiao) for(unsigned k=0;k<3;++k) {
        auto a=faces[i].pontos[k],b=faces[i].pontos[(k+1)%3];
        auto& viz=adj.at(borda(a,b));
        if(viz.size()==2 && dentro.count(viz[0]) && dentro.count(viz[1])) continue;
        if(!seguinte.emplace(a,b).second) return {};
        if(viz.size()>1) {protegidos.insert(a); protegidos.insert(b);}
    }
    unsigned eixo=0;
    for(unsigned k=1;k<3;++k) if(std::abs(normal[k])>std::abs(normal[eixo])) eixo=k;
    std::vector<std::vector<Ponto>> aneis;
    std::vector<double> areas;
    double soma=0;
    const auto origem=faces[regiao.front()].pontos[0];
    auto xy=[&](Ponto p){return modelo::XY{p[(eixo+1)%3]-origem[(eixo+1)%3],p[(eixo+2)%3]-origem[(eixo+2)%3]};};
    auto cruz=[](auto a,auto b){return a[0]*b[1]-a[1]*b[0];};
    while(!seguinte.empty()) {
        std::vector<Ponto> anel;
        auto p=seguinte.begin()->first,inicio=p;
        do {
            auto it=seguinte.find(p);
            if(it==seguinte.end()) return {};
            anel.push_back(p); p=it->second; seguinte.erase(it);
        } while(p!=inicio);
        std::vector<Ponto> cantos;
        for(size_t i=0;i<anel.size();++i) {
            if(protegidos.count(anel[i]) || !intermediario(anel[(i+anel.size()-1)%anel.size()],anel[i],anel[(i+1)%anel.size()]))
                cantos.push_back(anel[i]);
        }
        if(cantos.size()<3) return {};
        double area=0;
        for(size_t i=0;i<cantos.size();++i) area+=cruz(xy(cantos[i]),xy(cantos[(i+1)%cantos.size()]));
        areas.push_back(area); soma+=area; aneis.push_back(std::move(cantos));
    }
    if(aneis.empty() || soma*normal[eixo]<=0) return {};
    size_t externo=0;
    for(size_t i=1;i<areas.size();++i) if(std::abs(areas[i])>std::abs(areas[externo])) externo=i;
    for(size_t i=0;i<areas.size();++i) if(i!=externo && areas[i]*areas[externo]>=0) return {};
    std::swap(aneis[0],aneis[externo]);
    std::vector<std::vector<modelo::XY>> poligono;
    std::vector<Ponto> vertices;
    for(auto& anel:aneis) {
        auto& pontos=poligono.emplace_back();
        for(auto& p:anel) {pontos.push_back(xy(p)); vertices.push_back(p);}
    }
    auto indices=mapbox::earcut<unsigned>(poligono);
    if(indices.empty() || indices.size()/3>=regiao.size()) return {};
    std::vector<Face> novas;
    double areaObtida=0;
    std::set<Borda> emitidas;
    for(size_t i=0;i<indices.size();i+=3) {
        Face f{{vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]},faces[regiao.front()].material};
        auto n=modelo::normalUnitaria(f);
        if(produto(n,normal)<0) std::swap(f.pontos[1],f.pontos[2]);
        if(n==Ponto{}) return {};
        for(unsigned k=0;k<3;++k) {
            auto a=f.pontos[k],b=f.pontos[(k+1)%3];
            areaObtida+=cruz(xy(a),xy(b)); emitidas.insert(borda(a,b));
        }
        novas.push_back(f);
    }
    if(std::abs(areaObtida-soma)>1e-8*std::abs(soma)) return {};
    for(auto& anel:aneis) for(size_t i=0;i<anel.size();++i)
        if(!emitidas.count(borda(anel[i],anel[(i+1)%anel.size()]))) return {};
    return novas;
}

inline void planar(std::vector<Face>& faces,double graus,const Verificar& verificar)
{
    std::map<Borda,std::vector<size_t>> adj;
    std::vector<Ponto> normais;
    for(size_t i=0;i<faces.size();++i) {
        if(i%1024==0) verificar();
        normais.push_back(modelo::normalUnitaria(faces[i]));
        auto& p=faces[i].pontos;
        for(unsigned k=0;k<3;++k) adj[borda(p[k],p[(k+1)%3])].push_back(i);
    }
    const double limite=std::cos(graus*std::acos(-1.)/180.)-1e-12;
    std::vector<bool> usada(faces.size());
    std::vector<Face> resultado;
    for(size_t i=0;i<faces.size();++i) {
        if(usada[i]) continue;
        std::vector<size_t> regiao{i}; usada[i]=true;
        for(size_t j=0;j<regiao.size();++j) {
            if(j%1024==0) verificar();
            auto f=regiao[j];
            for(unsigned k=0;k<3;++k) {
                auto& p=faces[f].pontos;
                auto& viz=adj.at(borda(p[k],p[(k+1)%3]));
                if(viz.size()!=2) continue;
                auto v=viz[0]==f?viz[1]:viz[0];
                if(!usada[v] && faces[v].material==faces[i].material &&
                   produto(normais[v],normais[f])>=limite && produto(normais[v],normais[i])>=limite) {
                    usada[v]=true; regiao.push_back(v);
                }
            }
        }
        auto novas=regiao.size()>1?triangularRegiao(faces,regiao,adj,normais[i]):std::vector<Face>{};
        if(novas.empty()) for(auto j:regiao) resultado.push_back(faces[j]);
        else resultado.insert(resultado.end(),novas.begin(),novas.end());
    }
    faces.swap(resultado);
}

using Quad=std::array<Ponto,4>;
using UV=std::array<int,2>;
struct Grade {
    std::map<UV,Ponto> pontos;
    std::map<UV,std::array<size_t,2>> celulas;
    UV min{},max{};
};

// Parear os triângulos da mesma face original. Sem topologia fornecida, aceitar apenas
// pares com hipotenusa comum e cantos praticamente retos, depois verificar toda a grade.
inline std::optional<Grade> reconhecer(const std::vector<Face>& faces,const Verificar& verificar)
{
    if(faces.size()<8 || faces.size()%2) return {};
    std::map<unsigned,std::vector<size_t>> originais;
    std::map<Borda,std::vector<size_t>> inferidos;
    for(size_t i=0;i<faces.size();++i) {
        if(i%1024==0) verificar();
        auto& f=faces[i];
        if(f.quadrilatero) { originais[f.quadrilatero].push_back(i); continue; }
        int canto=-1;
        for(int k=0;k<3;++k) {
            auto u=diferenca(f.pontos[(k+1)%3],f.pontos[k]),v=diferenca(f.pontos[(k+2)%3],f.pontos[k]);
            if(std::abs(produto(u,v))<0.015*std::sqrt(produto(u,u)*produto(v,v))) canto=k;
        }
        if(canto<0) return {};
        inferidos[borda(f.pontos[(canto+1)%3],f.pontos[(canto+2)%3])].push_back(i);
    }
    std::vector<Quad> quads;
    std::vector<std::array<size_t,2>> pares;
    auto parear=[&](auto& paresPorChave){
        for(auto& entrada:paresPorChave) {
            auto& par=entrada.second;
            if(par.size()!=2) return false;
            auto& a=faces[par[0]].pontos; auto& b=faces[par[1]].pontos;
            auto unico=std::find_if(b.begin(),b.end(),[&](auto p){return std::find(a.begin(),a.end(),p)==a.end();});
            if(unico==b.end()) return false;
            unsigned k=0;
            while(k<3 && std::find(b.begin(),b.end(),a[k])!=b.end()) ++k;
            if(k==3 || std::find(b.begin(),b.end(),a[(k+1)%3])==b.end() || std::find(b.begin(),b.end(),a[(k+2)%3])==b.end()) return false;
            quads.push_back({a[k],a[(k+1)%3],*unico,a[(k+2)%3]});
            pares.push_back({par[0],par[1]});
        }
        return true;
    };
    if(!parear(originais) || !parear(inferidos)) return {};
    std::map<Borda,std::vector<size_t>> lados;
    for(size_t i=0;i<quads.size();++i) for(unsigned k=0;k<4;++k)
        lados[borda(quads[i][k],quads[i][(k+1)%4])].push_back(i);
    std::vector<std::array<UV,4>> coords(quads.size());
    coords[0]={UV{0,0},UV{1,0},UV{1,1},UV{0,1}};
    std::vector<bool> vista(quads.size()); vista[0]=true;
    std::vector<size_t> fila{0};
    Grade g; std::map<Ponto,UV> inverso;
    for(size_t passo=0;passo<fila.size();++passo){
        if(passo%1024==0) verificar();
        auto i=fila[passo]; auto& uv=coords[i]; auto& q=quads[i];
        UV cel=uv[0];
        for(unsigned k=0;k<4;++k){
            const auto a=uv[k],b=uv[(k+1)%4];
            if(g.pontos.emplace(a,q[k]).first->second!=q[k] || inverso.emplace(q[k],a).first->second!=a) return {};
            for(unsigned j=0;j<2;++j){cel[j]=std::min(cel[j],a[j]);g.min[j]=std::min(g.min[j],a[j]);g.max[j]=std::max(g.max[j],a[j]);}
            auto& viz=lados.at(borda(q[k],q[(k+1)%4]));
            if(viz.size()>2) return {};
            if(viz.size()==1) continue;
            size_t n=viz[0]==i?viz[1]:viz[0];
            auto& outro=quads[n];
            auto t=static_cast<unsigned>(std::find(outro.begin(),outro.end(),q[(k+1)%4])-outro.begin());
            if(t==4 || outro[(t+1)%4]!=q[k]) return {};
            std::array<UV,4> proxima;
            proxima[t]=b; proxima[(t+1)%4]=a;
            UV fora{b[1]-a[1],a[0]-b[0]};
            for(int j=0;j<2;++j){proxima[(t+2)%4][j]=a[j]+fora[j];proxima[(t+3)%4][j]=b[j]+fora[j];}
            if(vista[n] && coords[n]!=proxima) return {};
            if(!vista[n]) {coords[n]=proxima;vista[n]=true;fila.push_back(n);}
        }
        if(!g.celulas.emplace(cel,pares[i]).second) return {};
    }
    auto largura=g.max[0]-g.min[0],altura=g.max[1]-g.min[1];
    if(fila.size()!=quads.size() || largura<2 || altura<2 || static_cast<size_t>(largura)*altura!=quads.size()) return {};
    return g;
}

inline void reverter(std::vector<Face>& faces,const Grade& g,int iteracoes,const std::optional<Caixa>& corte,const Verificar& verificar)
{
    int passo=1;
    const int menor=std::min(g.max[0]-g.min[0],g.max[1]-g.min[1]);
    for(int i=0;i<iteracoes && passo<=menor/2;++i) passo*=2;
    if(passo==1) return;
    std::vector<Face> novas;
    for(int y=g.min[1];y<g.max[1];y+=passo) for(int x=g.min[0];x<g.max[0];x+=passo){
        verificar();
        auto p=[&](int i,int j){return g.pontos.at({x+i,y+j});};
        int w=std::min(passo,g.max[0]-x),h=std::min(passo,g.max[1]-y);
        bool manter=w!=passo || h!=passo;
        if(corte && !manter) {
            for(int j=0;j<=h && !manter;++j) for(int i=0;i<=w;++i) {
                auto v=p(i,j);
                for(unsigned k=0;k<3;++k) if(v[k]<(*corte)[2*k] || v[k]>(*corte)[2*k+1]) manter=true;
            }
        }
        if(manter) {
            for(int j=0;j<h;++j) for(int i=0;i<w;++i)
                for(auto t:g.celulas.at({x+i,y+j})) novas.push_back(faces[t]);
            continue;
        }
        std::vector<Ponto> anel;
        for(int lado=0;lado<4;++lado) {
            auto lateral=[&](int t){switch(lado){case 0:return p(t,0);case 1:return p(w,t);case 2:return p(w-t,h);default:return p(0,h-t);}};
            anel.push_back(lateral(0));
            bool fronteira=(lado==0 && y==g.min[1]) || (lado==1 && x+w==g.max[0]) ||
                           (lado==2 && y+h==g.max[1]) || (lado==3 && x==g.min[0]);
            // Conservar a curva da borda e a ligação com blocos que atravessam o corte.
            bool vizinhoFino=corte.has_value() || (lado==1 && g.max[0]-x-w<passo) || (lado==2 && g.max[1]-y-h<passo);
            if(fronteira || vizinhoFino) for(int t=1;t<passo;++t)
                if(!intermediario(lateral(t-1),lateral(t),lateral(t+1))) anel.push_back(lateral(t));
        }
        auto material=faces[g.celulas.at({x,y})[0]].material;
        if(anel.size()==4) {
            novas.push_back({{anel[0],anel[1],anel[2]},material});
            novas.push_back({{anel[0],anel[2],anel[3]},material});
        } else {
            auto centro=p(w/2,h/2);
            for(size_t k=0;k<anel.size();++k) novas.push_back({{anel[k],anel[(k+1)%anel.size()],centro},material});
        }
    }
    faces.swap(novas);
}
}
