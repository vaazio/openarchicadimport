#include "../Src/geometria.hpp"
#include <cassert>

int main()
{
    using namespace modelo;
    Ponto a{4,2,1}, b{6,2,1}, c{6,5,1}, d{4,5,1};
    Face abc{{a,b,c}, 3}, acd{{a,c,d}, 3};
    auto plano = converter({abc, acd});
    assert(plano.linhas.size() == 4);
    std::swap(acd.pontos[1], acd.pontos[2]);
    assert(converter({abc, acd}).linhas == plano.linhas);
    acd.material = 7;
    assert(converter({abc, acd}).linhas.size() == 5);

    auto triangulo = converter({abc});
    assert(converter({abc, abc}).linhas == triangulo.linhas);
    Face inversa = abc;
    std::swap(inversa.pontos[1], inversa.pontos[2]);
    assert(converter({abc, inversa}).linhas == triangulo.linhas);

    Face distante{{Ponto{-500,-500,-500}, a, a}, 42};
    auto limpo = converter({distante, abc});
    assert(limpo.largura == 2 && limpo.profundidade == 3);
    assert(limpo.script == triangulo.script && limpo.linhas == triangulo.linhas);
    assert(limpo.materiais == triangulo.materiais);

    Face aba{{a,c,Ponto{5,3,4}}, 8};
    acd.material = 3;
    Linha diagonal{{XY{0,0}, XY{2,3}}};
    assert(converter({abc, aba, acd}).linhas.count(diagonal) == 1);
    assert(converter({abc, acd, aba}).linhas.count(diagonal) == 1);

    // Tampo opaco, peça inferior totalmente oculta e assento que ultrapassa as duas bordas.
    std::vector<Face> mesa;
    auto retangulo=[&](double x,double y,double X,double Y,double z,size_t parte){
        Ponto p{x,y,z},q{X,y,z},r{X,Y,z},s{x,Y,z};
        mesa.push_back({{p,q,r},0,parte});mesa.push_back({{p,r,s},0,parte});
    };
    retangulo(1,1,3,4,1,0);
    retangulo(1.3,1.3,1.5,1.5,.7,1);
    retangulo(0,2,4,3,.5,2);
    auto linhas=plantaVisivel(mesa);
    std::set<Linha> assento;
    for(auto& s:linhas){assert(s.parte!=1);if(s.parte==2)assento.insert(s.linha);}
    assert(assento.size()==6);
    assert(assento.count({XY{0,2},XY{1,2}}) && assento.count({XY{3,2},XY{4,2}}));
    auto editadas=linhas;editadas.erase(editadas.begin());
    assert(converter(mesa,&editadas).linhas.size()+1==converter(mesa).linhas.size());
    assert(converter(mesa,&editadas).script==converter(mesa).script);
    // A altura do encosto cruza o tampo dentro de um único triângulo, em x=2.
    mesa.push_back({{Ponto{1,2.5,0},Ponto{3,2.5,2},Ponto{3,2.6,2}},0,3});
    bool cruzou=false;
    for(auto& s:plantaVisivel(mesa)) if(s.parte==3 && s.linha==Linha{XY{2,2.5},XY{3,2.5}}) cruzou=true;
    assert(cruzou);

    // Os pontos do assento usam a mesma origem e transformação do tampo e do símbolo.
    std::array<double,6> referencia{-1,5,-2,4,-3,3};
    std::vector<std::array<double,6>> caixas{{1,2,1,2,0,2},{2,3,1,2,0,2}};
    auto padrao=converter(mesa);
    auto extras=converter({abc},&editadas,&referencia,caixas);
    assert(padrao.pontos2d.empty() && padrao.script.find("HOTSPOT ")==std::string::npos);
    assert(extras.largura==6 && extras.profundidade==6);
    assert(extras.pontos2d.count({2,3}) && extras.pontos2d.count({4,4}));
    assert(extras.script.find("HOTSPOT 2, 3, 3,")!=std::string::npos);
    for(auto& seg:editadas){
        auto l=seg.linha;for(auto& p:l){p[0]+=1;p[1]+=2;}
        assert(extras.linhas.count(l));
    }

    Face invalida = abc;
    invalida.pontos[0][1] = std::numeric_limits<double>::quiet_NaN();
    for (const std::vector<Face>& entrada : std::vector<std::vector<Face>>{{}, {distante}, {abc, invalida}}) {
        try {
            converter(entrada);
            return 1;
        } catch (const std::runtime_error&) {}
    }
}
