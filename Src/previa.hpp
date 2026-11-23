#pragma once
#include "cena.hpp"
#include <functional>
#include <nlohmann/json.hpp>

struct ResultadoPrevia {
    std::vector<modelo::Face> faces;
    std::vector<int> vinculos;
    std::vector<modelo::Segmento> linhas;
    std::wstring nome;
    std::array<double,6> referencia{};
    std::vector<std::array<double,6>> caixas;
};
using GravarObjeto=std::function<void(ResultadoPrevia&,const nlohmann::json&)>;
bool abrirPrevia(const std::vector<Parte>& partes, const nlohmann::json& materiais, const GravarObjeto& gravar);

std::wstring nomeDisponivel(const std::wstring& nome);
