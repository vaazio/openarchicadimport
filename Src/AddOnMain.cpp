#include "APIEnvir.h"
#include "ACAPinc.h"
#include "BuiltInLibrary.hpp"
#include "GSUnID.hpp"
#include "ResourceIds.hpp"
#include "previa.hpp"
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <memory>
#include <exception>
#include <cstdio>

extern "C" IMAGE_DOS_HEADER __ImageBase;
namespace {
void verificar(GSErrCode erro, const char* etapa)
{
    if (erro != NoError) throw std::runtime_error(std::string(etapa) + " (" + std::to_string(erro) + ")");
}

std::wstring unicode(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    if (!w.empty()) w.pop_back();
    return w;
}

struct Opcoes {
    std::vector<Parte> partes;
    std::vector<unsigned> selecionadas;
    std::vector<std::wstring> materiais, superficies;
    std::vector<int> indices, vinculos;
    nlohmann::json superficiesPrevia = nlohmann::json::array();
    std::exception_ptr erroDialogo;
    bool eixoY = false;
    double escala = 1;
};

void carregarMateriais(const aiScene& cena, Opcoes& op)
{
    GS::Array<API_Attribute> atributos;
    verificar(ACAPI_Attribute_GetAttributesByType(API_MaterialID, atributos), "Ler superficies");
    for (auto& a : atributos) {
        GS::UniString nome(a.header.name);
        op.superficies.emplace_back(nome.ToUStr().Get());
        op.indices.push_back(a.header.index.ToInt32_Deprecated());
        auto cor = a.material.surfaceRGB;
        op.superficiesPrevia.push_back({{"indice",op.indices.back()},{"nome", nome.ToCStr(0,MaxUSize,CC_UTF8).Get()},
                                        {"cor",{cor.f_red,cor.f_green,cor.f_blue}}});
        delete a.material.texture.fileLoc;
    }
    if (op.indices.empty()) throw std::runtime_error("Projeto sem superficies disponiveis.");
    for (unsigned i = 0; i < cena.mNumMaterials; ++i) {
        aiString nome;
        cena.mMaterials[i]->Get(AI_MATKEY_NAME, nome);
        auto label = unicode(nome.C_Str());
        if (label.empty()) label = L"Material " + std::to_wstring(i + 1);
        op.materiais.push_back(label);
        auto it = std::find(op.superficies.begin(), op.superficies.end(), label);
        op.vinculos.push_back(it == op.superficies.end() ? 0 : static_cast<int>(it - op.superficies.begin()));
    }
}

INT_PTR tratarSelecao(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    auto enviar = [h](int id, UINT comando, WPARAM a = 0, LPARAM b = 0) {
        auto resultado = SendDlgItemMessageW(h, id, comando, a, b);
        if ((comando == LB_ADDSTRING || comando == CB_ADDSTRING || comando == CB_SETCURSEL ||
             comando == CB_GETCURSEL) && resultado < 0)
            throw std::runtime_error("Falha ao preencher ou ler a selecao do modelo.");
        return resultado;
    };
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(h, GWLP_USERDATA, l);
        auto& op = *reinterpret_cast<Opcoes*>(l);
        for (auto& p : op.partes) enviar(101, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(unicode(p.nome).c_str()));
        enviar(101, LB_SETHORIZONTALEXTENT, 1600);
        enviar(101, LB_SETSEL, TRUE, -1);
        for (auto label : {L"metros", L"centimetros", L"milimetros", L"polegadas"})
            enviar(102, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        for (auto label : {L"Z", L"Y"}) enviar(103, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        enviar(102, CB_SETCURSEL);
        enviar(103, CB_SETCURSEL, op.eixoY);
        for (auto& nome : op.materiais) enviar(104, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nome.c_str()));
        for (auto& nome : op.superficies) enviar(105, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nome.c_str()));
        enviar(104, CB_SETCURSEL);
        enviar(105, CB_SETCURSEL, op.vinculos.front());
        return TRUE;
    }
    if (msg != WM_COMMAND) return FALSE;
    auto& op = *reinterpret_cast<Opcoes*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (HIWORD(w) == CBN_SELCHANGE) {
        auto i = enviar(104, CB_GETCURSEL);
        if (LOWORD(w) == 104) enviar(105, CB_SETCURSEL, op.vinculos.at(i));
        if (LOWORD(w) == 105) op.vinculos.at(i) = static_cast<int>(enviar(105, CB_GETCURSEL));
    }
    if (LOWORD(w) == IDOK) {
        if (enviar(101, LB_GETSELCOUNT) < 1) {
            MessageBoxW(h, L"Selecione pelo menos uma geometria.", L"Importar modelo", MB_OK);
            return TRUE;
        }
        for (unsigned i = 0; i < op.partes.size(); ++i)
            if (enviar(101, LB_GETSEL, i) > 0) op.selecionadas.push_back(i);
        constexpr double fatores[] = {1, 0.01, 0.001, 0.0254};
        op.escala = fatores[enviar(102, CB_GETCURSEL)];
        op.eixoY = enviar(103, CB_GETCURSEL) == 1;
    }
    if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
        EndDialog(h, LOWORD(w));
        return TRUE;
    }
    return FALSE;
}

INT_PTR CALLBACK selecao(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    try {
        return tratarSelecao(h, msg, w, l);
    } catch (...) {
        // Devolva o erro ao importador sem propagar excecoes pela chamada Win32.
        auto* op = reinterpret_cast<Opcoes*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (op) op->erroDialogo = std::current_exception();
        EndDialog(h, -1);
        return TRUE;
    }
}

struct Secao {
    GSHandle dados = nullptr;
    ~Secao() { if (dados) BMKillHandle(&dados); }
};

void desenhar(const modelo::Resultado& r, Secao& simbolo)
{
    API_Element padrao {};
    padrao.header.type = API_LineID;
    verificar(ACAPI_Element_GetDefaults(&padrao, nullptr), "Ler padrao de linha");
    verificar(ACAPI_LibraryPart_SetUpSect_2DDrawHdl(), "Iniciar simbolo");
    GSErrCode erro = NoError;
    for (auto& linha : r.linhas) {
        API_Element el = padrao;
        el.header.layer = ACAPI_CreateAttributeIndex(1);
        el.line.begC = {linha[0][0], linha[0][1]};
        el.line.endC = {linha[1][0], linha[1][1]};
        erro = ACAPI_Element_Create(&el, nullptr);
        if (erro != NoError) break;
    }
    auto fechar = ACAPI_LibraryPart_GetSect_2DDrawHdl(&simbolo.dados);
    verificar(erro, "Criar linha do simbolo");
    verificar(fechar, "Concluir simbolo");
}

void salvar(const std::wstring& nome, modelo::Resultado& r, const Opcoes& op)
{
    GS::Array<API_LibraryInfo> bibliotecas;
    Int32 indice = -1;
    verificar(ACAPI_LibraryManagement_GetLibraries(&bibliotecas, &indice), "Abrir biblioteca interna");
    if (indice < 0 || static_cast<UIndex>(indice) >= bibliotecas.GetSize())
        throw std::runtime_error("Biblioteca interna indisponivel.");
    API_LibPart lp {};
    lp.typeID = APILib_ObjectID;
    lp.isPlaceable = true;
    auto pai = BL::BuiltInLibraryMainGuidContainer::GetInstance().GetUnIDWithNullRevGuid(BL::BuiltInLibPartID::ModelElementLibPartID);
    CHCopyC(pai.ToUniString().ToCStr(), lp.parentUnID);
    GS::ucsncpy(lp.docu_UName, nome.c_str(), std::size(lp.docu_UName)-1);
    Secao simbolo, parametros;
    if (r.materiais.size() > 64) throw std::runtime_error("Selecione ate 64 materiais distintos por objeto.");
    desenhar(r, simbolo);
    const auto bytesParametros = static_cast<GSSize>(r.materiais.size() * sizeof(API_AddParType));
    auto pars = reinterpret_cast<API_AddParType**>(BMAllocateHandle(bytesParametros, ALLOCATE_CLEAR, 0));
    if (!pars) throw std::bad_alloc();
    short i = 0;
    for (auto id : r.materiais) {
        auto& p = (*pars)[i];
        p.index = i++;
        p.typeID = APIParT_Mater;
        std::snprintf(p.name, sizeof(p.name), "sup_%u", id);
        GS::ucsncpy(p.uDescname, op.materiais[id].c_str(), std::size(p.uDescname)-1);
        p.value.real = op.indices[op.vinculos[id]];
    }
    auto err = ACAPI_LibraryPart_GetSect_ParamDef(&lp, pars, &r.largura, &r.profundidade, nullptr, &parametros.dados);
    ACAPI_DisposeAddParHdl(&pars);
    verificar(err, "Preparar parametros de superficie");
    API_LibPartDetails detalhes {};
    detalhes.object.autoHotspot = true;
    verificar(ACAPI_LibraryPart_SetDetails_ParamDef(&lp, parametros.dados, &detalhes), "Definir pontos de insercao");
    GS::UniString script3d(r.script.c_str());
    std::ostringstream planta;
    planta.imbue(std::locale::classic());
    planta << std::setprecision(15) << "MUL2 A / " << r.largura << ", B / " << r.profundidade << "\nFRAGMENT2 ALL, 0";
    size_t id=1;
    for(const auto& p:r.pontos2d) planta<<"\nHOTSPOT2 "<<p[0]<<", "<<p[1]<<", "<<id++;
    GS::UniString script2d(planta.str().c_str());
    lp.location = new IO::Location(bibliotecas[indice].location);
    auto criar = ACAPI_LibraryPart_Create(&lp);
    std::unique_ptr<IO::Location> local(lp.location);
    verificar(criar, "Criar objeto na biblioteca");
    API_LibPartSection sec {};
    sec.sectType = API_SectParamDef;
    verificar(ACAPI_LibraryPart_AddSection(&sec, parametros.dados, nullptr), "Gravar parametros");
    sec.sectType = API_Sect2DDraw;
    verificar(ACAPI_LibraryPart_AddSection(&sec, simbolo.dados, nullptr), "Gravar simbolo");
    sec.sectType = API_Sect3DScript;
    verificar(ACAPI_LibraryPart_AddSection(&sec, nullptr, &script3d), "Gravar geometria");
    sec.sectType = API_Sect2DScript;
    verificar(ACAPI_LibraryPart_AddSection(&sec, nullptr, &script2d), "Gravar script 2D");
    verificar(ACAPI_LibraryPart_Save(&lp), "Salvar objeto");
    std::wstring mensagem = L"Objeto criado na biblioteca interna: ";
    mensagem += lp.docu_UName;
    MessageBoxW(GetActiveWindow(), mensagem.c_str(), L"Importar modelo", MB_OK);
}

// A cópia conserva a trama vetorial, reflexos e referência de textura da base.
int duplicarSuperficie(const nlohmann::json& dados,const Opcoes& origem)
{
    GS::UniString nome(dados.at("nome").get<std::string>().c_str(),CC_UTF8);
    if(nome.IsEmpty() || nome.GetLength()>255) throw std::runtime_error("Nome da superfície inválido.");
    API_Attribute consulta{};consulta.header.typeID=API_MaterialID;consulta.header.uniStringNamePtr=&nome;
    auto busca=ACAPI_Attribute_Get(&consulta);
    delete consulta.material.texture.fileLoc;
    if(busca==NoError) throw std::runtime_error("Já existe uma superfície com este nome.");
    if(busca!=APIERR_BADNAME) verificar(busca,"Consultar nome da superfície");
    API_Attribute copia{};copia.header.typeID=API_MaterialID;
    copia.header.index=ACAPI_CreateAttributeIndex(origem.indices.at(dados.at("base").get<size_t>()));
    verificar(ACAPI_Attribute_Get(&copia),"Ler superfície de base");
    std::unique_ptr<IO::Location> textura(copia.material.texture.fileLoc);
    copia.header={};copia.header.typeID=API_MaterialID;copia.header.uniStringNamePtr=&nome;
    auto rgb=dados.at("cor").get<std::array<double,3>>();
    for(double v:rgb) if(!(v>=0 && v<=1)) throw std::runtime_error("Cor inválida.");
    copia.material.surfaceRGB={rgb[0],rgb[1],rgb[2]};
    verificar(ACAPI_Attribute_Create(&copia,nullptr),"Criar superfície");
    return copia.header.index.ToInt32_Deprecated();
}

void gravarComSuperficies(ResultadoPrevia& previa,const nlohmann::json& q,const Opcoes& origem,
                          std::map<std::string,int>& criadas)
{
    const auto& escolhas=q.at("acabamentos");
    const auto& copias=q.at("rascunhos");
    Opcoes op;
    op.indices=origem.indices;op.materiais=origem.materiais;op.vinculos=previa.vinculos;
    std::map<std::pair<unsigned,int>,unsigned> parametros;
    std::set<unsigned> usados;
    for(auto& f:previa.faces){
        const auto& escolha=escolhas.at(f.parte);
        if(!escolha.is_null()){
            int v=escolha.get<int>();
            auto [it,novo]=parametros.emplace(std::make_pair(f.material,v),static_cast<unsigned>(op.materiais.size()));
            if(novo){
                auto nome=v>=0?origem.superficies.at(v):unicode(copias.at(-1LL-v).at("nome").get<std::string>());
                op.materiais.push_back(origem.materiais.at(f.material)+L" / "+nome);
                op.vinculos.push_back(v);
            }
            f.material=it->second;
        }
        usados.insert(f.material);
    }
    if(usados.size()>64)throw std::runtime_error("O objeto admite até 64 parâmetros de superfície.");
    auto resultado=modelo::converter(previa.faces,&previa.linhas,&previa.referencia,previa.caixas);
    for(auto id:usados){
        int& v=op.vinculos.at(id);
        if(v>=0)continue;
        const auto& dados=copias.at(-1LL-v);
        auto chave=dados.dump();
        if(!criadas.contains(chave))criadas[chave]=duplicarSuperficie(dados,origem);
        v=static_cast<int>(op.indices.size());op.indices.push_back(criadas.at(chave));
    }
    salvar(previa.nome,resultado,op);
}

void importar(bool simples)
{
    HWND janela = GetActiveWindow();
    wchar_t arquivo[32768] {};
    OPENFILENAMEW dialogo {};
    dialogo.lStructSize = sizeof(dialogo);
    dialogo.hwndOwner = janela;
    dialogo.lpstrFilter = L"Modelos 3D\0*.obj;*.stl;*.fbx;*.dae;*.gltf;*.glb\0\0";
    dialogo.lpstrFile = arquivo;
    dialogo.nMaxFile = static_cast<DWORD>(std::size(arquivo));
    dialogo.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialogo)) {
        if (CommDlgExtendedError()) throw std::runtime_error("Falha ao abrir seletor de arquivo.");
        return;
    }
    std::filesystem::path caminho(arquivo);
    Assimp::Importer leitor;
    auto utf8 = caminho.u8string();
    auto* cena = leitor.ReadFile(reinterpret_cast<const char*>(utf8.c_str()), aiProcess_ValidateDataStructure);
    if (!cena || !cena->mRootNode) throw std::runtime_error(leitor.GetErrorString());
    auto origem=quadrilateros(*cena);
    cena=leitor.ApplyPostProcessing(aiProcess_Triangulate);
    if(!cena) throw std::runtime_error(leitor.GetErrorString());
    Opcoes op;
    listar(*cena, *cena->mRootNode, {}, "", op.partes,&origem);
    std::set<std::string> usados;
    unsigned anonimo=0;
    for(auto& p:op.partes) {
        if(p.nome.empty()) {
            ++anonimo;
            p.nome=std::string("Peça ")+(anonimo<10?"0":"")+std::to_string(anonimo);
        }
        auto base=p.nome;
        for(int n=2;!usados.insert(p.nome).second;++n) p.nome=base+" "+std::to_string(n);
    }
    if (op.partes.empty() || !cena->mNumMaterials) throw std::runtime_error("Arquivo sem malhas de superficie.");
    op.eixoY = _wcsicmp(caminho.extension().c_str(), L".stl") != 0;
    carregarMateriais(*cena, op);
    if (!simples) {
        auto materiais=nlohmann::json::array();
        for (const auto& nome:op.materiais)
            materiais.push_back(GS::UniString(nome.c_str()).ToCStr(0,MaxUSize,CC_UTF8).Get());
        std::map<std::string,int> criadas;
        API_LibPart limiteNome{};
        GS::UniString nomeInicial(caminho.stem().c_str());
        API_Element linha{}; linha.header.type=API_LineID;
        verificar(ACAPI_Element_GetDefaults(&linha,nullptr),"Ler caneta da planta");
        API_Pen caneta{}; caneta.index=linha.line.linePen.penIndex;
        nlohmann::json cor={0.1,0.12,0.13};
        if(ACAPI_Attribute_GetPen(caneta)==NoError)
            cor={caneta.rgb.f_red,caneta.rgb.f_green,caneta.rgb.f_blue};
        abrirPrevia(op.partes, {{"nome",nomeInicial.ToCStr(0,MaxUSize,CC_UTF8).Get()},
                                      {"limiteNome",std::size(limiteNome.docu_UName)-1},{"caneta",cor},{"materiais",materiais},{"superficies",op.superficiesPrevia},
                                      {"vinculos",op.vinculos},{"y",op.eixoY }}, [&](ResultadoPrevia& previa,const nlohmann::json& q){
            gravarComSuperficies(previa,q,op,criadas);
        });
        return;
    }
    auto escolha = DialogBoxParamW(reinterpret_cast<HINSTANCE>(&__ImageBase), MAKEINTRESOURCEW(4100), janela, selecao, reinterpret_cast<LPARAM>(&op));
    if (op.erroDialogo) std::rethrow_exception(op.erroDialogo);
    if (escolha == -1) throw std::runtime_error("Falha ao abrir a selecao de geometrias.");
    if (escolha != IDOK) return;
    std::vector<modelo::Face> faces;
    for (auto i : op.selecionadas) adicionar(op.partes[i], op.escala, op.eixoY, faces);
    auto resultado = modelo::converter(faces);
    salvar(caminho.stem().wstring(), resultado, op);
}

GSErrCode comando(const API_MenuParams* menu)
{
    try { importar(menu->menuItemRef.itemIndex == 2); }
    catch (const std::exception& e) { MessageBoxW(GetActiveWindow(), unicode(e.what()).c_str(), L"Erro ao importar modelo", MB_ICONERROR); }
    return NoError;
}
}

std::wstring nomeDisponivel(const std::wstring& nome)
{
    API_LibPart lp{};
    constexpr auto limite=std::size(lp.docu_UName)-1;
    if(nome.empty() || nome.find_first_not_of(L" \t\r\n")==std::wstring::npos) throw std::runtime_error("Informe o nome do objeto.");
    if(nome.size()>limite) throw std::runtime_error("Nome maior que o limite do Archicad.");
    if(nome.find_first_of(L"\\/:*?\"<>|")!=std::wstring::npos || nome.back()==L'.' || nome.back()==L' ')
        throw std::runtime_error("Confira os caracteres do nome do objeto.");
    auto candidato=nome;
    for(unsigned numero=2;;++numero) {
        API_LibPart busca{};
        busca.typeID=APILib_ObjectID;
        GS::ucsncpy(busca.docu_UName,candidato.c_str(),limite);
        auto err=ACAPI_LibraryPart_Search(&busca,false);
        delete busca.location;
        if(err==APIERR_BADNAME || err==APIERR_BADID) return candidato;
        if(err!=APIERR_MISSINGDEF) verificar(err,"Consultar nome na biblioteca");
        auto sufixo=L" "+std::to_wstring(numero);
        auto base=nome.substr(0,limite-sufixo.size());
        if(!base.empty() && base.back()>=0xd800 && base.back()<=0xdbff) base.pop_back();
        candidato=base+sufixo;
    }
}

API_AddonType CheckEnvironment(API_EnvirParams* env)
{
    env->addOnInfo.name = "OPENARCHICADIMPORT";
    env->addOnInfo.description = "Cria objetos com superficies do projeto e simbolo editavel.";
    return APIAddon_Normal;
}
GSErrCode RegisterInterface() { return ACAPI_MenuItem_RegisterMenu(ID_ADDON_MENU, 0, MenuCode_GDLObjects, MenuFlag_Default); }
GSErrCode Initialize() { return ACAPI_MenuItem_InstallMenuHandler(ID_ADDON_MENU, comando); }
GSErrCode FreeData() { return NoError; }
