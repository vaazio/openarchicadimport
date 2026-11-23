#include "../Src/reducao.hpp"
#include <cassert>

int main()
{
    using namespace previa;
    const auto verificar=[]{};
    Ponto a{0,0,0}, b{2,0,0}, c{2,2,0}, d{0,2,0};
    std::vector<Face> piso{{{a,b,c},1},{{a,c,d},1}};
    auto separados=piso;
    separados.push_back({{c,Ponto{3,2,0},Ponto{3,3,0}},1});
    auto ids=ilhas(separados,verificar);
    assert(ids[0]==ids[1] && ids[1]!=ids[2]);
    std::vector<Face> cortadas;
    for(const auto& f:piso) recortar(f,{.5,1.5,.5,1.5,-1,1},cortadas);
    auto resultado=modelo::converter(cortadas);
    assert(resultado.largura==1 && resultado.profundidade==1);
    for(const auto& f:cortadas) for(const auto& p:f.pontos)
        assert(p[0]>=.5 && p[0]<=1.5 && p[1]>=.5 && p[1]<=1.5 && p[2]==0 && f.material==1);
    // A aresta diagonal encontra o plano de corte em (1.25,1.25).
    // O vértice resultante deve ser um ponto real do símbolo, na mesma escala do GDL.
    Face inclinada{{Ponto{0,0,0},Ponto{2,2,1},Ponto{2,0,1}},1};
    std::vector<Face> trecho;
    recortar(inclinada,{0,1.25,0,2,-1,2},trecho);
    auto comPontos=modelo::converter(trecho,nullptr,nullptr,{{0,1.25,0,1.25,0,.625}});
    assert(comPontos.pontos2d.count({1.25,1.25}) && !comPontos.pontos2d.count({2,2}));
    assert(comPontos.script.starts_with("MUL A / 1.25, B / 1.25, 1\n"));
    assert(comPontos.script.find("HOTSPOT 1.25, 1.25, 0.625,")!=std::string::npos);

    std::vector<Face> fora;
    recortar(piso[0],{3,4,3,4,3,4},fora);
    assert(fora.empty());
    std::vector<Face> grade;
    for(int x=0;x<20;++x) for(int y=0;y<20;++y){
        auto p=[&](int dx,int dy){return Ponto{double(x+dx),double(y+dy),0};};
        grade.push_back({{p(0,0),p(1,0),p(1,1)},2});
        grade.push_back({{p(0,0),p(1,1),p(0,1)},2});
    }
    auto gradeOriginal=grade;
    auto topologia=reconhecer(grade,verificar);
    assert(topologia);
    reverter(grade,*topologia,2,{},verificar);
    assert(grade.size()==50 && modelo::converter(grade).linhas.size()==4);
    assert(!reconhecer(separados,verificar));
    // Região planar com furo quadrado e perímetro subdividido.
    std::vector<Face> aro;
    for(auto& f:gradeOriginal){
        double x=(f.pontos[0][0]+f.pontos[1][0]+f.pontos[2][0])/3;
        double y=(f.pontos[0][1]+f.pontos[1][1]+f.pontos[2][1])/3;
        if(x<8 || x>12 || y<8 || y>12) aro.push_back(f);
    }
    auto contorno=modelo::converter(aro).linhas;
    assert(contorno.size()==8);
    planar(aro,1,verificar);
    assert(aro.size()==8 && modelo::converter(aro).linhas==contorno);
    grade=gradeOriginal;
    reduzir(grade,.3,verificar);
    assert(!grade.empty() && grade.size()<800);
    auto reduzido=modelo::converter(grade);
    assert(reduzido.largura==20 && reduzido.profundidade==20);
}
