#pragma once
#include "tipos.hpp"

namespace modelo {
// Os candidatos de um extremo estão em um plano. Seu par mais afastado está no
// fecho convexo. Os antípodas evitam comparar cada vértice com todos os demais.
inline std::array<Ponto,2> diametro(std::vector<Ponto>& pontos,size_t eixo)
{
    size_t x=(eixo+1)%3, y=(eixo+2)%3;
    std::sort(pontos.begin(),pontos.end(),[=](const Ponto& a,const Ponto& b){
        return a[x]==b[x]?a[y]<b[y]:a[x]<b[x];
    });
    pontos.erase(std::unique(pontos.begin(),pontos.end()),pontos.end());
    if(pontos.size()<3) return {pontos.front(),pontos.back()};
    auto area=[=](const Ponto& a,const Ponto& b,const Ponto& c){
        return (b[x]-a[x])*(c[y]-a[y])-(b[y]-a[y])*(c[x]-a[x]);
    };
    std::vector<Ponto> hull;
    auto incluir=[&](const Ponto& p,size_t inicio){
        while(hull.size()>=inicio+2 && area(hull[hull.size()-2],hull.back(),p)<=0) hull.pop_back();
        hull.push_back(p);
    };
    for(auto& p:pontos) incluir(p,0);
    size_t topo=hull.size()-1;
    for(auto i=pontos.rbegin()+1;i!=pontos.rend();++i) incluir(*i,topo);
    hull.pop_back();
    std::array<Ponto,2> par{hull[0],hull[1]};
    double maior=-1;
    auto testar=[&](size_t a,size_t b){
        double d=std::hypot(hull[a][x]-hull[b][x],hull[a][y]-hull[b][y]);
        if(d>maior){maior=d;par={hull[a],hull[b]};}
    };
    size_t j=1,n=hull.size();
    for(size_t i=0;i<n;++i){
        auto k=(i+1)%n;
        while(area(hull[i],hull[k],hull[(j+1)%n])>area(hull[i],hull[k],hull[j])) j=(j+1)%n;
        testar(i,j);testar(k,j);
    }
    return par;
}

class ExtremosPorPeca {
    std::map<size_t,std::array<std::vector<Ponto>,6>> planos;
public:
    void incluir(size_t parte,const Ponto& p){
        auto& limites=planos[parte];
        for(size_t i=0;i<6;++i){
            auto& candidatos=limites[i];size_t eixo=i/2;
            if(!candidatos.empty()){
                double anterior=candidatos.front()[eixo];
                if(i%2?p[eixo]<anterior:p[eixo]>anterior) continue;
                if(p[eixo]!=anterior) candidatos.clear();
            }
            candidatos.push_back(p);
        }
    }
    std::set<Ponto> pontos(){
        std::set<Ponto> unicos;
        for(auto& entrada:planos) for(size_t i=0;i<6;++i){
            auto par=diametro(entrada.second[i],i/2);
            unicos.insert(par.begin(),par.end());
        }
        return unicos;
    }
};
}
