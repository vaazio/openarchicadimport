# Leitura de SketchUp no Importador3D

**Para editar peças antes de criar o objeto, a alternativa indicada é a SketchUp C API.** Para inserir o modelo diretamente na biblioteca interna, o Archicad 30 já oferece um fluxo de importação. Este parecer se baseia na documentação e na inspeção da instalação Windows e do DevKit 30. O conjunto de leitores Assimp compilado neste projeto abrange OBJ, STL, FBX, DAE e glTF/GLB.

## Importador nativo do Archicad

O comando de mesclar ou arrastar um SKP para o projeto gera um objeto GDL na biblioteca interna. A instalação BRA contém a extensão SketchUp In-Out. A documentação pública consultada descreve o resultado dessa conversão. Não identifiquei um ponto de integração documentado que entregue ao nosso editor, antes da criação do GDL, as definições dos componentes, suas instâncias e os materiais originais de cada face. Recuperar a geometria resultante de um objeto colocado exigiria reconstruir agrupamentos e introduziria alterações no projeto antes da confirmação da prévia. [Formatos 3D no Archicad 30](https://help.graphisoft.com/AC/30/INT/Topics/120_Interoperability/120_Interoperability-6.html)

## Leitor direto

A API C permite abrir um SKP, percorrer grupos e definições de componentes e aplicar as transformações de cada instância. A integração proposta atribuiria uma identidade a cada caminho de instância, acumularia as transformações dos grupos aninhados e converteria polegadas para metros uma única vez. Os dados lidos pela API precisam ser copiados na thread principal antes de seguir para o processamento de malhas da prévia. [SketchUp C API](https://extensions.sketchup.com/developers/sketchup_c_api/sketchup/index.html)

A leitura deve distinguir material de face, material herdado da instância e materiais de frente e verso. Para reproduzir uma textura do arquivo, seria necessário preservar imagem e coordenadas UV e ampliar a escrita da geometria GDL. Hoje o importador vincula as faces às superfícies do projeto. Ler imagens do SKP, por si só, não reproduz o mapeamento de textura na peça.

## Condições para implementação

- **Versões.** Selecionar uma versão do SDK e usar o estado devolvido por `SUModelCreateFromFileWithStatus`. Um arquivo mais recente pode abrir com perda de informação. A compatibilidade anunciada deverá corresponder às versões efetivamente ensaiadas com arquivos antigos, atuais e posteriores ao SDK escolhido. [Leitura de arquivos e estado de compatibilidade](https://extensions.sketchup.com/developers/sketchup_c_api/sketchup/struct_s_u_model_ref.html)
- **Distribuição.** A instalação local contém as DLLs do SketchUp. Para entregar um leitor independente é preciso obter os cabeçalhos e a biblioteca de ligação oficiais, verificar os termos de redistribuição da versão escolhida e testar a coexistência com a versão já carregada pelo Archicad. A Trimble disponibiliza um formulário para solicitar acesso. [Acesso ao SDK](https://extensions.sketchup.com/sketchup-sdk) · [Integração no Windows](https://extensions.sketchup.com/developers/sketchup_c_api/sketchup/index.html)
- **Decisão.** Avançar com o leitor direto quando o SDK e suas condições de distribuição estiverem confirmados. A primeira prova de compatibilidade deverá usar um componente repetido com materiais diferentes, uma instância espelhada e uma face texturizada com furo. Esses casos verificam o que é essencial para selecionar, simplificar e revestir peças individualmente.
