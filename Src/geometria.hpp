#pragma once
#include "planta.hpp"


namespace modelo {
struct Resultado {
    std::string script;
    std::set<Linha> linhas;
    std::set<unsigned> materiais;
    std::set<XY> pontos2d;
    double largura, profundidade;
};

inline Resultado converter(const std::vector<Face>& faces, const std::vector<Segmento>* simbolo = nullptr, const std::array<double,6>* referencia = nullptr, const std::vector<std::array<double,6>>& caixas = {})
{
    if (faces.empty()) throw std::runtime_error("Selecione pelo menos uma geometria.");
    Ponto minimo, maximo;
    minimo.fill(std::numeric_limits<double>::max());
    maximo.fill(std::numeric_limits<double>::lowest());
    std::vector<Ponto> normais;
    normais.reserve(faces.size());
    for (const auto& face : faces) {
        for (const auto& ponto : face.pontos)
            for (double valor : ponto)
                if (!std::isfinite(valor)) throw std::runtime_error("Coordenada invalida.");
        normais.push_back(normalUnitaria(face));
        if (normais.back() == Ponto{}) continue;
        for (const auto& ponto : face.pontos)
            for (size_t i = 0; i < 3; ++i) {
                minimo[i] = std::min(minimo[i], ponto[i]);
                maximo[i] = std::max(maximo[i], ponto[i]);
            }
    }
    if (maximo[0] < minimo[0]) throw std::runtime_error("Todas as faces selecionadas sao degeneradas.");
    for (size_t i = 0; i < 3; ++i)
        if (!std::isfinite(maximo[i] - minimo[i])) throw std::runtime_error("Dimensoes fora do intervalo suportado.");
    if(referencia) for(size_t k=0;k<3;++k){
        minimo[k]=referencia->at(2*k);maximo[k]=referencia->at(2*k+1);
    }
    Resultado r {};
    r.largura = std::max(0.000001, maximo[0] - minimo[0]);
    r.profundidade = std::max(0.000001, maximo[1] - minimo[1]);
    std::ostringstream gdl;
    gdl.imbue(std::locale::classic());
    gdl << std::setprecision(15) << "MUL A / " << r.largura << ", B / " << r.profundidade << ", 1\n";
    for (size_t f = 0; f < faces.size(); ++f) {
        const auto& n = normais[f];
        if (n == Ponto{}) continue;
        const auto& face = faces[f];
        auto p = face.pontos;
        for (auto& pt : p)
            for (size_t i = 0; i < 3; ++i) pt[i] -= minimo[i];
        r.materiais.insert(face.material);
        gdl << "MATERIAL sup_" << face.material << "\nBASE\n";
        for (auto& pt : p) gdl << "VERT " << pt[0] << ", " << pt[1] << ", " << pt[2] << "\n";
        gdl << "EDGE 1,2,1,0,0\nEDGE 2,3,1,0,0\nEDGE 3,1,1,0,0\nPGON 3,0,-1,1,2,3\nBODY -1\n";
    }
    if(!caixas.empty()){
        std::set<Ponto> pontos;
        for(const auto& c:caixas)
            for(size_t x:{0,1}) for(size_t y:{2,3}) for(size_t z:{4,5})
                pontos.insert({c[x]-minimo[0],c[y]-minimo[1],c[z]-minimo[2]});
        // Preservar a âncora de inserção e os cantos da caixa ao usar hotspots explícitos.
        const double altura=maximo[2]-minimo[2];
        for(double x:{0.,r.largura}) for(double y:{0.,r.profundidade}) for(double z:{0.,altura})
            pontos.insert({x,y,z});
        pontos.insert({r.largura/2,r.profundidade/2,0});
        size_t id=1;
        for(auto& pt:pontos){
            gdl<<"HOTSPOT "<<pt[0]<<", "<<pt[1]<<", "<<pt[2]<<", "<<id++<<"\n";
            r.pontos2d.insert({pt[0],pt[1]});
        }
    }
    auto gerar = simbolo ? std::vector<Segmento>{} : plantaVisivel(faces);
    for (auto seg : simbolo ? *simbolo : gerar) {
        for (auto& p : seg.linha) { p[0] -= minimo[0]; p[1] -= minimo[1]; }
        r.linhas.insert(seg.linha);
    }
    r.script = gdl.str();
    return r;
}
}
