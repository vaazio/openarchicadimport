# OPENARCHICADIMPORT para Archicad 30 no Windows

## Licença

O código próprio deste projeto está disponibilizado sob a GNU General Public License versão 3.0, conforme o arquivo [LICENSE](LICENSE). Componentes de terceiros mantêm suas próprias licenças. O Archicad e o DevKit da Graphisoft são proprietários e não estão incluídos nesta licença. A compatibilidade dos termos do DevKit e a distribuição de binários ainda precisam ser verificadas antes de uma publicação.

O comando **Importar modelo 3D (\*.\*)...** cria um objeto na biblioteca interna do projeto. A árvore mostra o nome de cada peça. Passe o mouse sobre o nome para ver o caminho da malha. Instâncias podem ser incluídas separadamente. O leitor Assimp está configurado para OBJ, STL, FBX, DAE (COLLADA), glTF e GLB. Mantenha arquivos auxiliares, como .mtl e .bin, junto ao modelo nas pastas indicadas por ele.

## Compilar e carregar

No Windows x64, instale o Development Kit do Archicad 30, Visual Studio 2022 com desenvolvimento para desktop em C++, Git, Python 3.10+ e CMake 3.22 ou superior. Abra o PowerShell na pasta deste projeto. Ajuste o caminho no comando abaixo para a pasta Support do seu SDK.

```powershell
py -3 compilar.py "C:\Graphisoft\API Development Kit 30\Support"
```

A primeira compilação baixa o template da Graphisoft e o Assimp. O script imprime o caminho do APX ao concluir. O MDID de exemplo do template serve para testar no Archicad iniciado com `-DEMO`. Para carregar na instalação licenciada, passe os dois números do seu MDID Graphisoft com `--mdid DESENVOLVEDOR ADDON`. No Gerenciador de Extensões do Archicad, adicione o APX compilado. [Template e requisitos da Graphisoft](https://github.com/GRAPHISOFT/archicad-addon-cmake)

O script também aceita `--versao 28` ou `--versao 29`, junto à pasta Support do DevKit da mesma versão. As compilações ficam em pastas separadas. A compatibilidade com 28 e 29 ainda não foi verificada: compilar e testar em cada versão é necessário antes da distribuição. Para obter um MDID, consulte o [cadastro de desenvolvedores Graphisoft](https://archicadapi.graphisoft.com/start).

## Usar e conferir

1. Abra uma planta e, em **Arquivo > Bibliotecas e Objetos**, execute **Importar modelo 3D (\*.\*)...**. Escolha o arquivo. Na árvore, use Ctrl+clique para alternar peças e Shift+clique para selecionar um intervalo. Os ícones ao lado do nome permitem excluir ou isolar o grupo e desfazer a última exclusão, restauração ou redução da peça indicada.
2. Confira o eixo vertical e a unidade das coordenadas lidas. STL e OBJ frequentemente dependem da convenção usada pelo exportador. Compare uma dimensão conhecida na primeira importação.
3. Em **Superfícies do projeto**, clique no vínculo do material de origem para abrir o seletor do Archicad. Use a busca ou navegue pelas pastas de superfícies. Nomes idênticos são vinculados automaticamente. Para nomes sem correspondência, revise a primeira superfície sugerida. O objeto armazena os vínculos em parâmetros de superfície que também podem ser alterados nas definições de cada instância. A aparência vem da superfície escolhida. Texturas e coordenadas UV de origem ficam fora desta versão.
4. Os pontos quentes seguem o padrão do Archicad. Para acrescentar encaixes, selecione as peças e clique com o botão direito em uma delas no 3D. **Criar caixa de pontos quentes** acrescenta pontos nos vértices da caixa da seleção e nos cantos correspondentes em planta. Cada caixa acompanha a escala, o corte e as exclusões. Use **Remover** no painel de caixas para apagar uma delas. Clique em **Criar objeto**. Na ferramenta **Objeto**, pesquise o nome exibido na confirmação e coloque uma instância. O campo **Nome do objeto** começa com o nome do arquivo e pode ser editado. O canto mínimo da geometria com área é a origem local.
5. Abra a definição pela opção **Abrir objeto**, entre em **Símbolo 2D**, edite as linhas com as ferramentas de desenho e salve. Confira a alteração na planta e em outra instância. O símbolo pertence à definição compartilhada. O script 2D exibe o desenho salvo e a geometria 3D permanece independente.

Em **Simplificação**, escolha **Modelo inteiro** para definir o ajuste geral ou **Por seleção** para alterar todas as peças e ilhas selecionadas. **Voltar ao ajuste geral** restaura a herança para a seleção.

A aba **Planta 2D** usa a projeção superior após a escala, o corte e as exclusões, antes de reduzir a malha. A intensidade da redução e a comparação **Antes / Depois** preservam o símbolo e suas dimensões. Para limpar apenas o símbolo, selecione as peças e use **Ocultar linhas das peças selecionadas**. A geometria continua presente no 3D e participa da oclusão em planta. Use **Separar peças desconectadas** para trabalhar com ilhas individuais.

A janela acompanha o tema claro ou escuro do Archicad. Clique com o botão direito em uma peça no **Modelo 3D** para ocultar, isolar ou trocar a superfície. Com várias peças selecionadas, escolha o alcance da ação no menu. **Duplicar superfície e vincular** permite mudar nome e cor de uma cópia da superfície selecionada. A cópia usada é criada no projeto ao confirmar **Criar objeto**.

O desfazer de cada peça preserva os acabamentos. Para rever uma limpeza do desenho em planta, use **Desfazer edição da planta**.

## Modelo para a primeira conferência

Abra `exemplo/bancada.obj` em metros com **Z vertical**. O tampo mede 1,20 × 0,60 m. Com ambas as peças selecionadas, ele fica a 0,90 m da base. Vincule Madeira e Metal a duas superfícies distintas. Repita a importação com a lateral desmarcada e confira que apenas o tampo entra no objeto. Em planta, edite uma das quatro linhas do tampo em **Símbolo 2D**, salve e reabra o projeto para conferir a persistência.

## Estado de validação

Na pasta deste projeto, execute o teste de geometria no Fedora com:

```bash
g++ -std=c++17 -Wall -Wextra -Werror testes/geometria.cpp -o /tmp/teste-geometria && /tmp/teste-geometria
```

O teste cobre faces duplicadas, diagonais coplanares, limites de material, arestas compartilhadas por três faces e o efeito de faces sem área sobre as dimensões e a origem. Também verifica a rejeição de coordenadas inválidas. O Assimp disponível no Fedora reconheceu as seis extensões e leu as duas malhas do OBJ de exemplo.

No Windows x64, o APX Release compilou com MSVC 19.44 e DevKit 30.3000 e carregou com as DLLs do Archicad 30 BRA build 3001. O teste de geometria passou. Com o Assimp da compilação, o conversor leu duas malhas e dois materiais de `bancada.obj` e gerou quatro linhas em planta para 1,20 × 0,60 m. A criação e colocação do objeto, a troca de superfície e a edição do símbolo ainda precisam de ensaio dentro do Archicad. [Seções de objetos na API da Graphisoft](https://graphisoft.github.io/archicad-api-devkit/group___library_part.html)
