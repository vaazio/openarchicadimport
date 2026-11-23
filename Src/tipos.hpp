#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace modelo {
using Ponto = std::array<double, 3>;
using XY = std::array<double, 2>;
using Linha = std::array<XY, 2>;
struct Face { std::array<Ponto, 3> pontos; unsigned material; size_t parte = 0; unsigned quadrilatero = 0; };
inline Ponto normalUnitaria(const Face& face)
{
    const auto& p = face.pontos;
    Ponto u {}, v {}, normal {};
    for (size_t i = 0; i < 3; ++i) {
        u[i] = p[1][i] - p[0][i];
        v[i] = p[2][i] - p[0][i];
    }
    for (size_t i = 0; i < 3; ++i)
        normal[i] = u[(i+1)%3]*v[(i+2)%3] - u[(i+2)%3]*v[(i+1)%3];
    double comprimento = std::hypot(normal[0], normal[1], normal[2]);
    if (!std::isfinite(comprimento)) throw std::runtime_error("Coordenadas fora do intervalo suportado.");
    if (comprimento > 0)
        for (auto& c : normal) c /= comprimento;
    return normal;
}

}
