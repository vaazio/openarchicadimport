#pragma once
#include "geometria.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

struct Parte {
    std::string nome;
    std::string caminho;
    const aiMesh* malha;
    aiMatrix4x4 matriz;
    using Trinca = std::array<unsigned,3>;
    using Origem = std::map<Trinca,unsigned>;
    const Origem* quads;
    unsigned indice = 0;
};

// Capturar a conectividade no espaço de índices do importador antes da triangulação.
inline std::vector<Parte::Origem> quadrilateros(const aiScene& cena)
{
    std::vector<Parte::Origem> resultado(cena.mNumMeshes);
    for(unsigned i=0;i<cena.mNumMeshes;++i) {
        auto& mesh=*cena.mMeshes[i];
        for(unsigned j=0;j<mesh.mNumFaces;++j) {
            const auto& f=mesh.mFaces[j];
            if(f.mNumIndices!=4) continue;
            for(unsigned k=0;k<4;++k) {
                Parte::Trinca t{f.mIndices[k],f.mIndices[(k+1)%4],f.mIndices[(k+2)%4]};
                std::sort(t.begin(),t.end());
                auto [it,novo]=resultado[i].emplace(t,j+1);
                if(!novo) it->second=0;
            }
        }
    }
    return resultado;
}

inline void listar(const aiScene& cena, const aiNode& no, aiMatrix4x4 pai,
                   std::string caminho, std::vector<Parte>& partes, const std::vector<Parte::Origem>* origem=nullptr)
{
    caminho += std::string("/") + no.mName.C_Str();
    auto matriz = pai * no.mTransformation;
    for (unsigned i = 0; i < no.mNumMeshes; ++i) {
        auto* m = cena.mMeshes[no.mMeshes[i]];
        if (!(m->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) continue;
        std::string nome=m->mName.C_Str();
        if(nome.empty()) nome=no.mName.C_Str();
        const auto pos=nome.find_last_of("/\\");
        if(pos!=std::string::npos) nome.erase(0,pos+1);
        if(nome=="defaultobject" || nome=="<STL_BINARY>" || nome==cena.mRootNode->mName.C_Str()) nome.clear();
        partes.push_back({nome,caminho+" / "+m->mName.C_Str(),m,matriz,
                          origem?&origem->at(no.mMeshes[i]):nullptr,static_cast<unsigned>(partes.size())});
    }
    for (unsigned i = 0; i < no.mNumChildren; ++i)
        listar(cena, *no.mChildren[i], matriz, caminho, partes,origem);
}

inline void adicionar(const Parte& parte, double escala, bool yVertical, std::vector<modelo::Face>& faces)
{
    auto& m = *parte.malha;
    const bool espelhada = parte.matriz.Determinant() < 0;
    for (unsigned i = 0; i < m.mNumFaces; ++i) {
        auto& f = m.mFaces[i];
        if (f.mNumIndices != 3) continue;
        modelo::Face face {};
        face.parte = parte.indice;
        if(parte.quads && !parte.quads->empty()) {
            Parte::Trinca t{f.mIndices[0],f.mIndices[1],f.mIndices[2]};
            std::sort(t.begin(),t.end());
            auto it=parte.quads->find(t);
            if(it!=parte.quads->end()) face.quadrilatero=it->second;
        }
        face.material = m.mMaterialIndex;
        for (unsigned j = 0; j < 3; ++j) {
            auto p = parte.matriz * m.mVertices[f.mIndices[j]];
            face.pontos[j] = {escala*p.x, escala*(yVertical ? -p.z : p.y), escala*(yVertical ? p.y : p.z)};
        }
        if (espelhada) std::swap(face.pontos[1], face.pontos[2]);
        faces.push_back(face);
    }
}
