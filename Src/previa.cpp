#include "APIEnvir.h"
#include "ACAPinc.h"
#include "DGModule.hpp"
#include "AppearanceManager/DGAppearanceManager.hpp"
#include "DGBrowser.hpp"
#include "DGCheckItem.hpp"
#include "previa.hpp"
#include "reducao.hpp"
#include <condition_variable>
#include <atomic>
#include <thread>
#include <mutex>
#include <windows.h>
#include <wincrypt.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;
using Json=nlohmann::json;
namespace {
GS::UniString pagina()
{
    auto modulo=reinterpret_cast<HMODULE>(&__ImageBase);
    auto res=FindResourceW(modulo,MAKEINTRESOURCEW(4101),RT_RCDATA);
    auto dados=res?LockResource(LoadResource(modulo,res)):nullptr;
    if(!dados) throw std::runtime_error("Interface da previa ausente.");
    return GS::UniString(static_cast<const char*>(dados),SizeofResource(modulo,res),CC_UTF8);
}

std::string codificar(const std::vector<float>& pos)
{
    const auto bytes=pos.size()*sizeof(float);
    if(bytes>MAXDWORD) throw std::runtime_error("Peça grande demais para a prévia.");
    DWORD tamanho=0;
    auto dados=reinterpret_cast<const BYTE*>(pos.data());
    constexpr DWORD formato=CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    if(!CryptBinaryToStringA(dados,static_cast<DWORD>(bytes),formato,nullptr,&tamanho))
        throw std::runtime_error("Não foi possível preparar a malha.");
    std::string texto(tamanho,'\0');
    if(!CryptBinaryToStringA(dados,static_cast<DWORD>(bytes),formato,texto.data(),&tamanho))
        throw std::runtime_error("Não foi possível transferir a malha.");
    texto.resize(tamanho);
    return texto;
}

struct Revisao {
    std::vector<modelo::Face> faces;
    std::vector<modelo::Segmento> linhas;
    previa::Caixa referencia{};
    std::vector<previa::Caixa> caixas;
    std::string json;
};

// Somente a thread da janela toca o Archicad. O trabalhador lê a cena Assimp
// enquanto o diálogo é modal e publica apenas uma revisão completa.
class Processamento {
    const std::vector<Parte>& partes;
    std::mutex mutex;
    std::condition_variable mudou;
    std::atomic<int> revisao{0};
    Json pedido;
    Revisao pronto;
    int publicada=0;
    std::vector<std::vector<modelo::Face>> base;
    std::vector<std::vector<size_t>> ilhas;
    // Apenas o trabalhador acessa este cache. A chave contém tudo que muda a projeção.
    Json chavePlanta;
    std::vector<modelo::Segmento> linhasPlanta;
    std::thread thread;

    Revisao calcular(const Json& q,const previa::Verificar& verificar)
    {
        auto escala=q.at("escala").get<double>();
        if(!(escala>0 && std::isfinite(escala))) throw std::runtime_error("Escala inválida.");
        bool eixoY=q.at("y"), antes=q.at("antes");
        const auto& geral=q.at("geral");
        const auto& excecoes=q.at("excecoes");
        auto excluidos=q.at("excluidos").get<std::set<std::string>>();
        std::optional<previa::Caixa> corte;
        if(!q.at("corte").is_null()) {
            corte=q.at("corte").get<previa::Caixa>();
            for(size_t j=0;j<6;j+=2) if(corte->at(j)>corte->at(j+1)) throw std::runtime_error("Limites de corte invertidos.");
        }
        Json chave={escala,eixoY,q.at("corte"),excluidos,q.at("contornos")};
        bool recalcular=chave!=chavePlanta;
        std::vector<modelo::Face> facesPlanta;
        std::map<std::string,previa::Caixa> porPeca;
        Revisao resultado;
        Json grupos=Json::array(), arvore=Json::array();
        Json contagens=Json::object();
        struct Malha {
            std::string id, metodo;
            double valor;
            size_t naFonte;
            std::vector<modelo::Face> integra, originalGrade;
        };
        std::vector<Malha> malhas;
        std::vector<std::string> donos;
        size_t original=0,recortado=0,reduzido=0;
        constexpr auto maximo=std::numeric_limits<double>::max();
        previa::Caixa limites{maximo,-maximo,maximo,-maximo,maximo,-maximo};
        auto recortar=[&](const std::vector<modelo::Face>& entrada){
            if(!corte) return entrada;
            std::vector<modelo::Face> saida;
            for(size_t k=0;k<entrada.size();++k){if(k%1024==0) verificar();previa::recortar(entrada[k],*corte,saida);}
            return saida;
        };
        for(size_t i=0;i<partes.size();++i) {
            verificar();
            auto& fonte=base[i];
            if(fonte.empty()) {
                std::vector<modelo::Face> todas;
                adicionar(partes[i],1,false,todas);
                for(const auto& f:todas) if(modelo::normalUnitaria(f)!=modelo::Ponto{}) fonte.push_back(f);
            }
            if(ilhas[i].empty()) ilhas[i]=previa::ilhas(fonte,verificar);
            auto pai=std::to_string(i);
            size_t n=ilhas[i].empty()?1:1+*std::max_element(ilhas[i].begin(),ilhas[i].end());
            arvore.push_back({{"id",pai},{"nome",partes[i].nome},{"caminho",partes[i].caminho},{"ilhas",n}});
            std::vector<std::vector<modelo::Face>> blocos(n);
            for(size_t k=0;k<fonte.size();++k) {
                if(k%1024==0) verificar();
                auto f=fonte[k];
                for(auto& p:f.pontos) {
                    if(eixoY) p={p[0],-p[2],p[1]};
                    for(auto& v:p) v*=escala;
                }
                blocos[ilhas[i][k]].push_back(f);
            }
            for(size_t j=0;j<n;++j) {
                auto id=n==1?pai:pai+"."+std::to_string(j);
                auto& bloco=blocos[j];
                auto& herdado=excecoes.contains(id)?excecoes.at(id):excecoes.contains(pai)?excecoes.at(pai):geral;
                auto& ajuste=herdado.is_null()?geral:herdado;
                auto metodo=ajuste.at("metodo").get<std::string>();
                auto valor=ajuste.at("valor").get<double>();
                if(excluidos.contains(id) || excluidos.contains(pai)){
                    contagens[id]={{"fonte",bloco.size()},{"antes",0},{"depois",0}};
                    continue;
                }
                size_t naFonte=bloco.size();
                original+=naFonte;
                auto integra=!corte && metodo!="grade"?std::move(bloco):recortar(bloco);
                recortado+=integra.size();
                if(integra.empty()){
                    contagens[id]={{"fonte",naFonte},{"antes",0},{"depois",0}};
                    continue;
                }
                auto p0=integra.front().pontos.front();
                previa::Caixa local{p0[0],p0[0],p0[1],p0[1],p0[2],p0[2]};
                for(auto& f:integra){
                    for(const auto& p:f.pontos) for(size_t k=0;k<3;++k){
                        local[2*k]=std::min(local[2*k],p[k]);
                        local[2*k+1]=std::max(local[2*k+1],p[k]);
                    }
                    if(recalcular){facesPlanta.push_back(f);facesPlanta.back().parte=donos.size();}
                }
                donos.push_back(id);porPeca.emplace(id,local);
                for(size_t k=0;k<6;k+=2){
                    limites[k]=std::min(limites[k],local[k]);
                    limites[k+1]=std::max(limites[k+1],local[k+1]);
                }
                malhas.push_back({id,metodo,valor,naFonte,std::move(integra),
                                  metodo=="grade"?std::move(bloco):std::vector<modelo::Face>{}});
            }
        }
        verificar();
        if(recalcular){
            auto novas=modelo::plantaVisivel(facesPlanta,q.at("contornos").get<bool>(),verificar);
            linhasPlanta.swap(novas);chavePlanta=std::move(chave);
        }
        std::vector<modelo::Face>().swap(facesPlanta);
        for(auto& [id,metodo,valor,naFonte,integra,bloco]:malhas){
            verificar();
            const auto antesDaReducao=integra.size();
            bool indisponivel=false;
            std::vector<modelo::Face> faces;
            if(metodo=="grade") {
                if(!(valor>=1 && valor<=8 && valor==std::floor(valor))) throw std::runtime_error("Número de iterações inválido.");
                auto grade=previa::reconhecer(bloco,verificar);
                indisponivel=!grade;
                if(grade){previa::reverter(bloco,*grade,static_cast<int>(valor),corte,verificar);faces=recortar(bloco);}
                else faces=antes?integra:std::move(integra);
            } else {
                faces=antes?integra:std::move(integra);
                if(metodo=="planar" && valor>=0 && valor<=89) previa::planar(faces,valor,verificar);
                else if(metodo=="collapse" && valor>=1 && valor<=100) previa::reduzir(faces,valor/100.,verificar);
                else throw std::runtime_error("Ajuste de redução inválido.");
            }
            reduzido+=faces.size();
            contagens[id]={{"fonte",naFonte},{"antes",antesDaReducao},{"depois",faces.size()},{"indisponivel",indisponivel}};
            if(antes) faces=std::move(integra);
            if(faces.empty()) continue;
            const auto origem=faces.front().pontos.front();
            std::vector<float> pos;
            pos.reserve(faces.size()*9);
            for(const auto& f:faces) {
                if(pos.size()%9216==0) verificar();
                for(const auto& p:f.pontos) for(size_t eixo=0;eixo<3;++eixo){
                    pos.push_back(static_cast<float>(p[eixo]-origem[eixo]));
                }
            }
            for(auto& f:faces) f.parte=grupos.size();
            grupos.push_back({{"id",id},{"mat",faces.front().material},
                              {"origem",origem},{"pos",codificar(pos)}});
            resultado.faces.insert(resultado.faces.end(),faces.begin(),faces.end());
        }
        resultado.linhas=linhasPlanta;
        resultado.referencia=limites;
        Json caixas=Json::array();
        for(const auto& selecao:q.at("caixas")){
            std::optional<previa::Caixa> caixa;
            for(const auto& membro:selecao){
                auto it=porPeca.find(membro.get<std::string>());
                if(it==porPeca.end())continue;
                if(!caixa)caixa=it->second;
                else for(size_t k=0;k<6;k+=2){
                    (*caixa)[k]=std::min((*caixa)[k],it->second[k]);
                    (*caixa)[k+1]=std::max((*caixa)[k+1],it->second[k+1]);
                }
            }
            if(caixa){resultado.caixas.push_back(*caixa);caixas.push_back(*caixa);}
            else caixas.push_back(nullptr);
        }
        Json desenho=Json::array();
        for(const auto& seg:resultado.linhas) desenho.push_back({donos.at(seg.parte),seg.linha});
        resultado.json=Json{{"caixas",caixas},{"linhas",std::move(desenho)},{"contagens",std::move(contagens)},{"reduzido",reduzido},{"grupos",std::move(grupos)},{"arvore",std::move(arvore)},{"limites",limites},{"original",original},{"corte",recortado},{"final",resultado.faces.size()}}.dump(-1,' ',false,Json::error_handler_t::replace);
        return resultado;
    }
    void executar()
    {
        int ultima=0;
        for(;;) {
            std::unique_lock lock(mutex);
            mudou.wait(lock,[&]{return revisao.load()!=ultima;});
            ultima=revisao.load();
            if(ultima<0) return;
            auto op=pedido;
            lock.unlock();
            Revisao resultado;
            try { resultado=calcular(op,[&]{if(revisao.load()!=ultima) throw std::runtime_error("Cancelado");}); }
            catch(const std::exception& e){resultado.json=Json{{"erro",e.what()}}.dump();}
            lock.lock();
            if(revisao.load()==ultima){pronto=std::move(resultado);publicada=ultima;}
        }
    }
public:
    explicit Processamento(const std::vector<Parte>& p):partes(p),base(p.size()),ilhas(p.size()),thread([this]{executar();}){}
    ~Processamento(){ {std::lock_guard lock(mutex);revisao=-1;} mudou.notify_one();thread.join();}
    int atualizar(Json q){std::lock_guard lock(mutex);pedido=std::move(q);int id=++revisao;mudou.notify_one();return id;}
    std::string ler(int id){std::lock_guard lock(mutex);return publicada==id?pronto.json:"";}
    bool obter(int id,const std::set<size_t>& ocultas,ResultadoPrevia& saida){
        std::lock_guard lock(mutex);
        if(id!=revisao || id!=publicada || pronto.faces.empty()) return false;
        if(!ocultas.empty() && *ocultas.rbegin()>=pronto.linhas.size()) throw std::runtime_error("Seleção de linhas inválida.");
        for(size_t i=0;i<pronto.linhas.size();++i) if(!ocultas.count(i)) saida.linhas.push_back(pronto.linhas[i]);
        saida.caixas=pronto.caixas;saida.referencia=pronto.referencia;
        saida.faces=pronto.faces;return true;
    }
};

class Janela final : public DG::ModalDialog,public DG::PanelObserver {
    DG::Browser browser;
    Processamento processamento;
    std::string escolherSuperficie(const Json& q,const Json& superficies)
    {
        // A âncora nativa existe apenas durante o popup e acompanha o botão HTML, mesmo após rolagem.
        auto x=static_cast<short>(std::clamp(q.at("x").get<double>(),0.,1.)*(browser.GetWidth()-1));
        auto y=static_cast<short>(std::clamp(q.at("y").get<double>(),0.,1.)*(browser.GetHeight()-1));
        DG::PushCheck ancora(GetReference(),DG::Rect(x,y,x+1,y+1));
        API_AttributePickerParams params;
        params.dialogID=GetId();
        params.itemID=ancora.GetId();
        params.type=APIUserControlType_Material;
        GS::Owner<API_AttributePicker> seletor;
        if(ACAPI_Dialog_CreateAttributePicker(params,seletor)!=NoError || seletor==nullptr)
            throw std::runtime_error("Falha ao abrir o seletor de superfícies do Archicad.");
        const auto& atual=superficies.at(q.at("vinculo").get<size_t>());
        seletor->SetSelectedAttributeIndex(ACAPI_CreateAttributeIndex(atual.at("indice").get<int>()));
        if(!seletor->Invoke()) return "null";
        auto escolha=seletor->GetSelectedAttributeIndex().ToInt32_Deprecated();
        for(size_t i=0;i<superficies.size();++i)
            if(superficies[i].at("indice")==escolha) return std::to_string(i);
        throw std::runtime_error("Superfície indisponível nesta importação.");
    }
    void PanelResized(const DG::PanelResizeEvent& e) override {browser.Resize(e.GetHorizontalChange(),e.GetVerticalChange());}
public:
    Janela(const std::vector<Parte>& partes,const Json& meta,const GravarObjeto& gravar):
        DG::ModalDialog(DG::NativePoint(),980,640,GS::Guid(),HVGrow),browser(GetReference(),DG::Rect(0,0,980,640)),processamento(partes)
    {
        SetTitle("OPENARCHICADIMPORT");
        GS::Ref<JS::Object> api=new JS::Object("Importador");
        api->AddItem(new JS::Function("enviar",[this,&meta,&gravar](GS::Ref<JS::Base> parametro)->GS::Ref<JS::Base>{
            try {
                auto v=GS::DynamicCast<JS::Value>(parametro);
                if(v==nullptr) throw std::runtime_error("Pedido inválido.");
                auto q=Json::parse(v->GetString().ToCStr(0,MaxUSize,CC_UTF8).Get());
                std::string resposta;
                auto acao=q.at("acao");
                if(acao=="inicio"){auto dados=meta;dados["escuro"]=DG::IsDarkMode();resposta=dados.dump();}
                else if(acao=="tema") resposta=DG::IsDarkMode()?"true":"false";
                else if(acao=="superficie") resposta=escolherSuperficie(q,meta.at("superficies"));
                else if(acao=="editar") resposta=std::to_string(processamento.atualizar(q));
                else if(acao=="ler") resposta=processamento.ler(q.at("id"));
                else if(acao=="cancelar") PostCloseRequest(Cancel);
                else if(acao=="criar") {
                    auto vinculos=q.at("vinculos").get<std::vector<int>>();
                    if(vinculos.size()!=meta.at("materiais").size()) throw std::runtime_error("Materiais inválidos.");
                    for(int i:vinculos) if(i<0 || static_cast<size_t>(i)>=meta.at("superficies").size()) throw std::runtime_error("Superfície inválida.");
                    GS::UniString nome(q.at("nome").get<std::string>().c_str(),CC_UTF8);
                    std::wstring solicitado=nome.ToUStr().Get();
                    auto livre=nomeDisponivel(solicitado);
                    if(livre!=solicitado) {
                        auto txt=Json{{"sugestao",GS::UniString(livre.c_str()).ToCStr(0,MaxUSize,CC_UTF8).Get()}}.dump();
                        return new JS::Value(GS::UniString(txt.c_str(),CC_UTF8));
                    }
                    ResultadoPrevia saida;
                    saida.nome=livre;
                    if(!processamento.obter(q.at("id"),q.at("ocultas").get<std::set<size_t>>(),saida)) throw std::runtime_error("Aguarde a atualização da prévia.");
                    saida.vinculos=std::move(vinculos);gravar(saida,q);PostCloseRequest(Accept);
                }
                return new JS::Value(GS::UniString(resposta.c_str(),CC_UTF8));
            } catch(const std::exception& e){return new JS::Value(GS::UniString(Json{{"erro",e.what()}}.dump().c_str(),CC_UTF8));}
        }));
        browser.LoadHTML(pagina());
        browser.RegisterAsynchJSObject(api);
        Attach(*this);
        ShowItems();
    }
    ~Janela(){Detach(*this);}
};
}
bool abrirPrevia(const std::vector<Parte>& partes,const Json& meta,const GravarObjeto& gravar)
{
    return Janela(partes,meta,gravar).Invoke();
}
