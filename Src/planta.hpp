#pragma once
#include "tipos.hpp"
#include <functional>
#include <numeric>
#include <tuple>

namespace modelo {
struct Segmento { Linha linha; size_t parte; };

namespace projecao {
using Intervalo = std::array<double,2>;
using Aresta = std::array<Ponto,2>;
inline double lado(const Ponto& a,const Ponto& b,const Ponto& p)
{
    return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);
}

// Interseção analítica de um segmento com um semiespaço linear, em t = [0,1].
inline bool limitar(double a,double b,Intervalo& t)
{
    if(a<0 && b<0) return false;
    if(a<0) t[0]=std::max(t[0],a/(a-b));
    if(b<0) t[1]=std::min(t[1],a/(a-b));
    return t[0]<t[1];
}
struct Triangulo {
    const Face* face;
    Ponto normal, minimo, maximo;
};
class Indice {
    struct No { Ponto min,max; size_t inicio,fim,esq=0,dir=0; };
    std::vector<Triangulo> tris;
    std::vector<No> nos;
    double tol;
    size_t dividir(size_t inicio,size_t fim)
    {
        No no{tris[inicio].minimo,tris[inicio].maximo,inicio,fim};
        for(size_t i=inicio+1;i<fim;++i) for(unsigned j=0;j<3;++j) {
            no.min[j]=std::min(no.min[j],tris[i].minimo[j]);
            no.max[j]=std::max(no.max[j],tris[i].maximo[j]);
        }
        size_t id=nos.size(); nos.push_back(no);
        if(fim-inicio>12) {
            unsigned eixo=(no.max[1]-no.min[1]>no.max[0]-no.min[0]);
            size_t meio=inicio+(fim-inicio)/2;
            std::nth_element(tris.begin()+inicio,tris.begin()+meio,tris.begin()+fim,[eixo](const auto& a,const auto& b){
                return a.minimo[eixo]+a.maximo[eixo]<b.minimo[eixo]+b.maximo[eixo];
            });
            auto esq=dividir(inicio,meio),dir=dividir(meio,fim);
            nos[id].esq=esq; nos[id].dir=dir;
        }
        return id;
    }
    void consultar(size_t id,const Aresta& e,const Ponto& min,const Ponto& max,std::vector<Intervalo>& sombras) const
    {
        auto& no=nos[id];
        if(no.max[2]<=min[2] || no.min[0]>max[0] || no.max[0]<min[0] || no.min[1]>max[1] || no.max[1]<min[1]) return;
        if(no.dir) {
            consultar(no.esq,e,min,max,sombras);
            consultar(no.dir,e,min,max,sombras);
            return;
        }
        for(size_t i=no.inicio;i<no.fim;++i) {
            auto& tri=tris[i];
            auto& p=tri.face->pontos;
            auto& n=tri.normal;
            double sinal=n[2]>0?1.:-1.;
            Intervalo t{0,1};
            // Distância vertical multiplicada por |nz| evita dividir por planos quase verticais.
            auto altura=[&](const Ponto& v){
                return sinal*((p[0][0]-v[0])*n[0]+(p[0][1]-v[1])*n[1]+(p[0][2]-v[2])*n[2]);
            };
            double a=altura(e[0]),b=altura(e[1]);
            if(std::max(a,b)<=tol || !limitar(a,b,t)) continue;
            for(unsigned j=0;j<3;++j) {
                if(!limitar(sinal*lado(p[j],p[(j+1)%3],e[0]),sinal*lado(p[j],p[(j+1)%3],e[1]),t)) { t={1,0}; break; }
            }
            if(t[0]<t[1]) sombras.push_back(t);
        }
    }
public:
    explicit Indice(double tolerancia):tol(tolerancia){}
    void adicionar(const Face& f,Ponto normal)
    {
        if(normal[2]==0) return;
        Triangulo t{&f,normal,f.pontos[0],f.pontos[0]};
        for(auto& p:f.pontos) for(unsigned j=0;j<3;++j) {
            t.minimo[j]=std::min(t.minimo[j],p[j]); t.maximo[j]=std::max(t.maximo[j],p[j]);
        }
        tris.push_back(t);
    }
    void construir(){if(!tris.empty()) dividir(0,tris.size());}
    void consultar(const Aresta& e,std::vector<Intervalo>& sombras) const
    {
        Ponto min,max;
        for(unsigned i=0;i<3;++i){min[i]=std::min(e[0][i],e[1][i]);max[i]=std::max(e[0][i],e[1][i]);}
        if(!nos.empty()) consultar(0,e,min,max,sombras);
    }
};

// Agrupa pelo suporte da reta e pelo proprietário. O intervalo é medido no eixo dominante.
inline std::vector<Segmento> unir(std::vector<Segmento>& segmentos,const Ponto& origem,double tol)
{
    using Chave=std::tuple<size_t,int,long long,long long>;
    std::map<Chave,std::vector<Linha>> grupos;
    for(auto& s:segmentos) {
        auto [a,b]=s.linha;
        int eixo=std::abs(b[1]-a[1])>std::abs(b[0]-a[0]);
        if(a[eixo]>b[eixo]) std::swap(a,b);
        double delta=b[eixo]-a[eixo];
        if(delta<=tol) continue;
        double m=(b[1-eixo]-a[1-eixo])/delta;
        double intercepto=(a[1-eixo]-origem[1-eixo])-m*(a[eixo]-origem[eixo]);
        grupos[{s.parte,eixo,std::llround(m*1e10),std::llround(intercepto/tol)}].push_back({a,b});
    }
    std::vector<Segmento> saida;
    std::set<Linha> vistas;
    for(auto& [chave,linhas]:grupos) {
        auto [parte,eixo,m,intercepto]=chave;
        std::sort(linhas.begin(),linhas.end(),[eixo](auto& a,auto& b){return a[0][eixo]<b[0][eixo];});
        auto guardar=[&](Linha l){
            if(l[1]<l[0]) std::swap(l[0],l[1]);
            if(vistas.insert(l).second) saida.push_back({l,parte});
        };
        Linha atual=linhas.front();
        for(auto& l:linhas) {
            if(l[0][eixo]>atual[1][eixo]+tol) {guardar(atual); atual=l;}
            else if(l[1][eixo]>atual[1][eixo]) atual[1]=l[1];
        }
        guardar(atual);
    }
    return saida;
}
}

inline std::vector<Segmento> plantaVisivel(const std::vector<Face>& faces,bool soContornos=false,
                                          const std::function<void()>& verificar=[]{})
{
    using namespace projecao;
    if(faces.empty()) return {};
    Ponto min=faces.front().pontos[0],max=min;
    for(const auto& f:faces) for(auto& p:f.pontos) for(unsigned j=0;j<3;++j){
        min[j]=std::min(min[j],p[j]); max[j]=std::max(max[j],p[j]);
    }
    double tol=std::max({max[0]-min[0],max[1]-min[1],max[2]-min[2],1e-15})*1e-10;
    Indice indice(tol);
    struct Adjacente { Ponto normal; unsigned mat; };
    std::map<std::pair<size_t,Aresta>,std::vector<Adjacente>> arestas;
    size_t passos=0;
    for(const auto& f:faces) {
        if(++passos%1024==0) verificar();
        auto n=normalUnitaria(f);
        if(n==Ponto{}) continue;
        indice.adicionar(f,n);
        for(unsigned i=0;i<3;++i) {
            Aresta e{f.pontos[i],f.pontos[(i+1)%3]};
            if(e[0][0]==e[1][0] && e[0][1]==e[1][1]) continue;
            auto orientada=n;
            if(e[1]<e[0]) { std::swap(e[0],e[1]); for(auto& c:orientada)c=-c; }
            arestas[{f.parte,e}].push_back({orientada,f.material});
        }
    }
    indice.construir();
    std::vector<Segmento> linhas;
    for(auto& [chave,adj]:arestas) {
        if(++passos%256==0) verificar();
        if(adj.size()==2) {
            auto& a=adj[0]; auto& b=adj[1];
            bool silhueta=a.normal[2]*b.normal[2]>=0;
            double coseno=-std::inner_product(a.normal.begin(),a.normal.end(),b.normal.begin(),0.);
            if(!silhueta && (soContornos || (a.mat==b.mat && coseno>0.8191520443))) continue; // 35 graus
        }
        auto& [parte,e]=chave;
        std::vector<Intervalo> sombras;
        indice.consultar(e,sombras);
        std::sort(sombras.begin(),sombras.end());
        auto emitir=[&](double a,double b){
            if(b-a<=1e-12) return;
            auto xy=[&](double t){return XY{std::lerp(e[0][0],e[1][0],t),std::lerp(e[0][1],e[1][1],t)};};
            linhas.push_back({{xy(a),xy(b)},parte});
        };
        double t=0;
        for(auto& s:sombras) {emitir(t,s[0]); t=std::max(t,s[1]);}
        emitir(t,1);
    }
    return unir(linhas,min,tol);
}
}
