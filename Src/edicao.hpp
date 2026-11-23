#pragma once
#include "geometria.hpp"
#include <meshoptimizer.h>
#include <functional>
#include <numeric>

namespace previa {
using modelo::Face;
using modelo::Ponto;
using Caixa = std::array<double, 6>;
using Verificar = std::function<void()>;

inline std::vector<size_t> ilhas(const std::vector<Face>& faces, const Verificar& verificar)
{
    std::vector<size_t> pais(faces.size());
    std::iota(pais.begin(), pais.end(), size_t{0});
    auto raiz = [&](size_t i) {
        while (pais[i] != i) { pais[i] = pais[pais[i]]; i = pais[i]; }
        return i;
    };
    std::map<std::array<Ponto, 2>, size_t> arestas;
    for (size_t i = 0; i < faces.size(); ++i) {
        if (i % 1024 == 0) verificar();
        for (size_t lado = 0; lado < 3; ++lado) {
            auto a = faces[i].pontos[lado], b = faces[i].pontos[(lado+1)%3];
            if (a == b) continue;
            if (b < a) std::swap(a,b);
            auto [it, nova] = arestas.emplace(std::array<Ponto,2>{a,b}, i);
            if (!nova) pais[raiz(i)] = raiz(it->second);
        }
    }
    std::map<size_t,size_t> ids;
    for (size_t i=0; i<pais.size(); ++i) {
        auto r = raiz(i);
        ids.emplace(r, ids.size());
    }
    std::vector<size_t> resultado;
    for (size_t i=0; i<pais.size(); ++i) resultado.push_back(ids.at(raiz(i)));
    return resultado;
}

inline void recortar(const Face& face, const Caixa& caixa, std::vector<Face>& saida)
{
    std::vector<Ponto> poligono(face.pontos.begin(), face.pontos.end()), prox;
    for (size_t plano=0; plano<6 && !poligono.empty(); ++plano) {
        size_t eixo=plano/2;
        double limite=caixa[plano];
        auto dentro=[&](const Ponto& p){ return plano%2 ? p[eixo]<=limite : p[eixo]>=limite; };
        prox.clear();
        Ponto a=poligono.back();
        for (const auto& b : poligono) {
            if (dentro(a)!=dentro(b)) {
                // Interpolar sempre na mesma direção, inclusive nas arestas compartilhadas.
                const auto& lo=a[eixo]<b[eixo]?a:b;
                const auto& hi=a[eixo]<b[eixo]?b:a;
                double t=(limite-lo[eixo])/(hi[eixo]-lo[eixo]);
                Ponto p;
                for(size_t j=0;j<3;++j) p[j]=std::lerp(lo[j],hi[j],t);
                p[eixo]=limite;
                prox.push_back(p);
            }
            if(dentro(b)) prox.push_back(b);
            a=b;
        }
        poligono.swap(prox);
    }
    for(size_t i=2;i<poligono.size();++i) {
        Face f{{poligono[0],poligono[i-1],poligono[i]},face.material,face.parte};
        if(modelo::normalUnitaria(f)!=Ponto{}) saida.push_back(f);
    }
}

// Cada chamada recebe uma peça de um só material. A fronteira é protegida inclusive nos cortes.
inline void reduzir(std::vector<Face>& faces,double fracao,const Verificar& verificar)
{
    if(fracao>=1 || faces.size()<2) return;
    std::map<Ponto,unsigned> mapa;
    std::vector<Ponto> vertices;
    std::vector<std::array<float,3>> pos;
    std::vector<unsigned> indices;
    const auto origem=faces.front().pontos.front();
    for(const auto& f:faces) {
        if(indices.size()%3072==0) verificar();
        for(const auto& p:f.pontos) {
            auto [it,nova]=mapa.emplace(p,static_cast<unsigned>(vertices.size()));
            if(nova) {
                vertices.push_back(p);
                pos.push_back({float(p[0]-origem[0]),float(p[1]-origem[1]),float(p[2]-origem[2])});
            }
            indices.push_back(it->second);
        }
    }
    std::vector<unsigned> novos(indices.size());
    size_t meta=3*std::max<size_t>(1,static_cast<size_t>(faces.size()*fracao));
    auto n=meshopt_simplify(novos.data(),indices.data(),indices.size(),pos.front().data(),
        vertices.size(),sizeof(pos.front()),meta,1.f,meshopt_SimplifyLockBorder);
    verificar();
    auto material=faces.front().material;
    faces.clear();
    for(size_t i=0;i<n;i+=3) faces.push_back({{vertices[novos[i]],vertices[novos[i+1]],vertices[novos[i+2]]},material});
}
}
