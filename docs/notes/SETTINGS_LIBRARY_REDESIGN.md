# Plano: configurações, biblioteca e dispositivos

Data: 18/10/2026. Estado: proposta para revisão, sem implementação nesta etapa.
Pedido: presets pessoais com importação/exportação, playlists e músicas externas,
melhorias nos saves e nas configurações do jogo e do launcher oficial, incluindo
uma reorganização dos inputs e do suporte a volantes.

## Direção de produto e visual

Uma configuração compartilhada entre jogo e launcher oficial. Usar o launcher
existente, sem criar outro aplicativo ou uma segunda fonte de preferências.

Direção confirmada pelo usuário: o layout de configurações criado pela Polyphony
para GT4 é a base. Não substituir por um layout de COD. Otimizar espaços,
reordenar informações e recriar componentes somente quando isso melhorar o uso,
preservando a estrutura e a linguagem visual do menu original. A referência de
COD se limita à comunicação visual dos efeitos: ver antes de decidir.

Toda melhoria deve ser opcional, com controles independentes registrados em
OPTIONS: reorganização, demonstrações visuais, novos hovers e fluxos adicionais.
Classic restaura a apresentação e o comportamento originais; nenhuma tela nova,
prévia ou biblioteca externa deve ser obrigatória para jogar. Presets não devem
ser a única forma de desligar melhorias. Funcionalidades novas ficam acessíveis
por entradas opcionais, sem obrigar o jogador a trocar o menu original.

A opção Classic é um compromisso de preservar e testar a referência PS2, não
uma declaração de que o port inteiro já atingiu fidelidade 1:1. Diferenças ainda
existentes continuam registradas em FIDELITY.

Conservar tipografia, cinzas, seleção, sons e transições da Polyphony. Adaptar
mouse, controle e touch sem impor a composição de desktop às outras plataformas.
O launcher segue a mesma organização conceitual, sem alterar o menu original
quando as melhorias estiverem desligadas.

## Comunicação visual como requisito principal

Refinamento do usuário após a referência de COD: a pessoa deve conseguir julgar
o efeito vendo o resultado antes de aplicar. A prévia é parte central da tela,
não uma ilustração pequena ao lado de uma explicação extensa.

Composição a validar dentro do layout original: preservar categorias e controles
da Polyphony e acomodar a demonstração em uma área contextual, ou abri-la por
uma ação opcional quando faltar espaço. Não impor a divisão esquerda/direita
do COD. Identificar valor atual e proposto. No controle, acesso
simples à comparação; no mouse/touch, botões explícitos. A comparação precisa
usar a mesma cena, câmera, iluminação e instante para isolar a opção alterada.

Escolher a demonstração adequada para cada opção:

| Opção ou grupo | Demonstração |
|---|---|
| Sombras, filtros, reflexos, nitidez | Comparação com/sem ou atual/proposto, lado a lado ou divisor móvel; recorte ampliado quando necessário. |
| HUD, tamanho de texto, aspecto e área segura | Composição real sobre cena representativa, atualizada ao mover o ajuste. |
| Animações, blur e transições | Clipe curto em loop, sincronizado entre os estados; movimento revela o que uma imagem fixa não mostra. |
| Direção, zona morta, pedais e sensibilidade | Dispositivo e eixos animados em tempo real, mostrando entrada física e resposta configurada. |
| Mixagem e músicas | Prévia audível com volume controlado, identidade da faixa, progresso e visualização complementar; imagem não substitui ouvir. |
| Presets | Resumo visual das diferenças por categoria e acesso às prévias das opções que mudam. |
| Saves | Dados reais do perfil e miniatura quando disponível, permitindo identificar o conteúdo antes de carregar/importar. |

Priorizar renderização real quando for barata e segura. Para efeitos caros ou
que exigem reinício, usar clipes/imagens representativos, identificados como
“demonstração”, sem fingir que são o resultado ao vivo no equipamento do jogador.
Não prometer desempenho por vídeo. Reusar capturas locais de conteúdo do disco
do usuário ou uma cena própria distribuível; não empacotar assets do jogo.

GIF é referência de comportamento: o formato final pode ser vídeo curto ou
animação renderizada, escolhido pelo custo de memória/decodificação de cada
plataforma. Carregar apenas a demonstração em foco; pausar ao sair da página.
Oferecer pausar/reproduzir e respeitar redução de movimento. Prévia não altera
o save nem inicia uma corrida; alterações só se tornam definitivas ao aplicar.

Texto fica como apoio: nome, legenda curta do que observar e limitações relevantes.
A prévia não pode depender apenas de cor; rótulos e foco devem continuar legíveis.
Se uma opção não tem efeito visual, usar resposta prática apropriada, sem inventar
animação decorativa para simular benefício.

Critério de revisão dos protótipos: o jogador consegue apontar o que mudou e
comparar os estados usando a demonstração, sem precisar ler um parágrafo. A
primeira rodada deve mostrar pelo menos uma comparação estática, uma animada e
uma resposta de input ao vivo, com a direção de arte do GT4.

## O que já existe e precisa ser preservado

- `docs/OPTIONS.md`: Classic/Enhanced, IDs de opções, regra de modificado e
  exigência de armazenamento compartilhado. Presets pessoais ainda não estão
  concluídos; conferir a cobertura opção por opção antes da migração.
- `src/app/video_settings.c`: edição das opções de vídeo no jogo.
- `src/app/launcher*.c`: launcher existente; seguir `docs/LAUNCHER.md` para
  distinguir o oficial do secundário. O oficial é o alvo deste projeto.
- `src/app/save_ui.c`: perfis, salvar, renomear, excluir, importar save e mídia.
  A exportação explícita precisa entrar no fluxo visível ao jogador.
- `src/core/music.c`, `pcm_music.c`, `src/app/race_audio.c`: reprodução e
  integração musical. Há playlists originais de corrida e slideshow, com
  persistências próprias; músicas externas são uma extensão separada.
- `src/platform/sdl/pad_sdl.c` e `src/app/desktop_input.c`: controles, detecção
  de volantes e adaptação desktop. Auditar capacidades antes de prometer FFB.

## Organização das telas

Preservar a entrada e a hierarquia de Options da Polyphony como referência.
Na reorganização opcional, agrupar opções relacionadas (imagem, áudio, controles,
interface, presets e dados salvos) aproveitando os componentes existentes, sem
substituir automaticamente a tela principal por um dashboard novo. Resumos e
busca são extensões opcionais; a versão original permanece selecionável.

Cada opção terá nome legível, descrição curta, valor atual, padrão e indicação
quando exigir reinício ou valer apenas na próxima corrida. Prévia visual quando
explicar melhor que texto: filtro, HUD, sombra e aspecto. Evitar miniaturas
puramente decorativas e números de desempenho sem medição.

Aplicar, cancelar e restaurar a categoria devem ser consistentes. Presets podem
ser pré-visualizados sem gravar imediatamente. Trocar de categoria preserva o
rascunho; sair com alterações pede aplicar ou descartar.

## Etapas e critérios de conclusão

### 1. Inventário e proposta visual

Mapear cada opção, seu armazenamento, disponibilidade por plataforma, aplicação
em tempo real/reinício e exposição no launcher. Separar dispositivo físico de
perfil de bindings e protocolo legado. Inventariar formatos reais de save,
backups e música. Produzir protótipos das telas principal, opção detalhada,
saves, playlist e volante; conferir mouse, controle e touch nos protótipos.

Concluído quando a hierarquia, componentes e fluxos tiverem exemplos visuais
revisáveis e cada opção existente tiver destino definido. Não remover recursos
apenas para reduzir a lista.

### 2. Presets pessoais e modelo compartilhado

Criar, duplicar, salvar como, renomear e excluir presets. Classic e Enhanced
permanecem protegidos; alterações mostram o nome seguido de “modificado”.
Permitir preset completo ou por categoria, com escopo explícito antes de aplicar.

Proposta de intercâmbio: JSON versionado com nome, escopo e IDs estáveis das
opções. Exportar sem caminhos pessoais, identidade do jogador ou dados de save.
Importar com resumo de alterações, chaves desconhecidas, valores incompatíveis
e opções indisponíveis na plataforma. Não converter silenciosamente em Classic.
Aplicar somente valores validados; preservar a configuração anterior em caso de
falha. IDs de dispositivos locais precisam de reassociação, não cópia cega.

Concluído com ida e volta de exportação/importação, migração de versão,
recuperação de arquivo inválido e igualdade dos valores entre jogo e launcher.

### 3. Tela de saves e portabilidade

Lista de perfis/cartões com nome, data, progresso e carro atual quando esses dados
existirem. Painel de detalhes e ações Salvar, Carregar, Renomear, Duplicar,
Importar, Exportar e Excluir. Diferenciar perfil ativo de item apenas selecionado.

Exportação padrão: pacote versionado do perfil, com save e manifesto. Replays,
fotos e outros conteúdos grandes são seleções explícitas com tamanho estimado;
configurações/presets são opcionais e identificados. Nunca incluir ISO.
Importar como novo perfil por padrão; substituição exige identificar o destino
e manter backup. Arquivo corrupto ou incompatível não altera o save atual.
Escrita temporária com substituição segura e validação dos caminhos do pacote.

Concluído quando um perfil exportado abrir em outra instalação e mantiver seus
dados; testar cancelamento, colisão de nomes, falta de espaço e importação
inválida. Conferir também o caminho de importação PS2 já existente.

### 4. Biblioteca musical e playlists

Biblioteca unifica visualmente faixas do disco e faixas adicionadas, identificando
a origem. Adicionar arquivos ou pasta, ler metadados quando disponíveis e permitir
editar o nome exibido. Oferecer ouvir prévia, ordenar, remover da biblioteca e
localizar arquivos ausentes. Remover da biblioteca não apaga o arquivo original.

Criar/duplicar/renomear/excluir playlists; reordenar, ativar faixas, shuffle e
escolher os contextos: menus, corrida, replay e slideshow. Manter seleção musical
original independente e disponível. Respeitar pausa da corrida e prioridade de
jingles/transições; não sobrepor dois players.

Auditar decoders por plataforma antes de fechar a lista de formatos. Copiar para
a biblioteca gerenciada deve ser uma escolha clara; referências externas podem
sumir ao mover a instalação. Exportar playlists com referências relativas;
incluir arquivos de áudio somente quando solicitado. Mostrar ausências na
importação em vez de descartar faixas silenciosamente.

Concluído com ordem persistida, arquivos ausentes, formatos inválidos, pausa,
troca de contexto e nenhuma alteração nas playlists originais ao desativar a
extensão. Validar custo de memória e streaming no Switch e Android.

### 5. Inputs por dispositivo

Entrada simples: Teclado e mouse, Controle ou Volante. Mostrar equipamentos
conectados com nome e porta/jogador. Protocolos e modelos legados ficam em
Avançado quando necessários; preservar compatibilidade e mappings existentes.

Controle: captura por ação, conflitos, restaurar padrões, zonas mortas e inversão.
Teclado/mouse: corrida e menus separados, atalhos e conflitos visíveis.
Volante: assistente para centro e extremos, direção, acelerador, freio e embreagem;
pedais combinados/separados, inversão e visualização ao vivo. Permitir atribuir
câmbio e botões de outro dispositivo sem perder o volante.

Mostrar somente capacidades detectadas. FFB exige auditoria e implementação
próprias: não apresentar ajuste funcional se houver apenas detecção do volante.
Hotplug e desconexão precisam preservar perfil e oferecer reassociação. Perfis
exportados não devem atribuir um dispositivo errado por índice de conexão.

Concluído com controle real, teclado/mouse e volante real; ausência de hardware
fica registrada como limitação. Replays e resultados devem preservar os parâmetros
que alterem a condução, conforme `docs/OPTIONS.md`.

### 6. Integração visual e validação final

Aplicar os componentes aprovados ao jogo e launcher oficial somente nos fluxos
opcionais correspondentes. Conferir a restauração do menu original ao desativar
cada melhoria e ao selecionar Classic. Estados hover,
foco por controle, pressionado, desabilitado, arraste e erro devem ser distintos.
Hover sem selecionar/aplicar valores; touch usa toque e arraste sem depender de
hover. Listas precisam de clipping correto, scroll e foco visível; textos longos
usam quebra ou reticências conforme o espaço.

Concluído com capturas dos fluxos principais nas plataformas disponíveis,
validação de legibilidade, escala e áreas de clique, e testes focados por etapa
com negativos relevantes. Registrar cada entrega em FIDELITY e OPTIONS, marcando
a apresentação nova como adaptação e preservando a referência original.

## Ordem sugerida de entrega

1. Inventário e protótipos para revisão.
2. Presets pessoais, importação/exportação e sincronização com launcher.
3. Saves e pacotes portáteis.
4. Biblioteca musical e playlists externas.
5. Configuração de dispositivos e calibração de volante.
6. Consolidação visual e validação por plataforma.

Cada entrega deve funcionar isoladamente e vir com build identificável, testes
focados e limitações registradas. Este plano não declara nenhuma dessas extensões
implementada nem autoriza publicar releases, substituir saves ou remover opções.

## Registro da primeira entrega — 18/10/2026

Implementados arquivos pessoais `.moonpreset` no menu existente e no launcher
oficial, com importação em rascunho, exportação, nome persistido, estado modificado
e opção independente para esconder as ações. Formato inicial é texto versionado
por chave/valor, em vez da proposta JSON. Não inclui caminhos pessoais nem campos
de monitor/dispositivo. Seleção nativa de arquivo disponível no Windows; biblioteca
visual dedicada, gerenciamento próprio de nomes/exclusão e seleção de arquivos
no Switch/Android continuam pendentes. As abas existentes tiveram espaçamento e
inset dos rótulos corrigidos após o print do usuário.

Prévias visuais comparativas, saves, playlists externas e a reorganização de
inputs continuam na sequência prevista; não foram implementados nesta entrega.

## Segunda entrega — transferência nativa de saves, 18/10/2026

Importação/exportação do perfil `.ogt4` no Windows, por ação opcional na tela
existente. A opção Save Transfer restaura o rodapé anterior quando desligada.
Roundtrip de progresso/garagem validado byte a byte; imports inválidos ou slots
ocupados não substituem o jogador ativo. Exportação de fotos/replays/filmes em
pacote e a ampliação visual dos detalhes do perfil continuam pendentes. Nenhuma
mudança de formato dos saves, playlist ou física faz parte desta entrega.

## Ajuste pedido: exportar tudo do jogador

18/10/2026: a exportação por padrão passou a ser um pacote `.moonplayer`, com o
perfil selecionado e a biblioteca compartilhada inteira de fotos, replays,
ghosts, filmes, playlists e arquivos auxiliares/importados. A limitação da
segunda entrega (mídias separadas) foi superada. Testes verificam a transferência
entre pastas, integridade dos arquivos e preservação dos dados em falhas ou
conflitos; os detalhes e limites estão em OPTIONS e FIDELITY. A reformulação
visual mais ampla da tela de saves permanece uma etapa separada.


## Primeiro uso e comparação — implementação de 18/10

Guia opcional depois da escolha/validação do disco, com ícones e materiais originais
em composição própria derivada do launcher. Direção refinada pelo usuário: sem café
desenhado, sem copiar o dock; usar Ford GT e Laguna Seca diurna. Comparação de gráficos
Classic/atuais por Tab/X, lado a lado por Space/Y, acesso por F7 no launcher.
Amostra real renderizada, sem corrida, sem trocar carro/save nem persistir a prévia.
O/O Settings retorna às opções existentes para a escolha efetiva do jogador.

Refeito no mesmo dia (estilo do launcher, direção da Polyphony):
marca e status nos cantos como na home, painel de vidro do OPTIONS com as páginas
como abas e o ícone da página do Options original, linhas "<  valor  >" com seta
dourada para Graphics/View, faixa de metal dos dealers com BACK/SKIP/NEXT (ou
OPTIONS/SKIP/DONE) e o cursor do jogo, linha de ajuda no rodapé. Lado a lado dentro
do painel, com fundo escuro e nomes CLASSIC/CURRENT. Setas/D-pad navegam linhas e faixa.

Segunda versão (pedido: funcional e informativa): cinco páginas (Welcome, Preset,
Graphics, Menus, Ready). Rascunho aplicado só em DONE/OPTIONS via video_setting_set;
SKIP/Esc não gravam. Cada opção gráfica tem comparação própria (original x escolhido,
ou x valor do Enhanced quando ainda original), divisor arrastável, recorte onde o
efeito aparece, texto "o que faz / onde olhar / custo" e tela cheia com Tab.
Isso cobre as "demonstrações individuais por efeito" abaixo, em imagem fixa; loops
animados e comparação em corrida continuam pendentes.

Ainda pendentes: demonstrações individuais por efeito, loops contextuais nas categorias,
comparação dentro de corrida e reformulação completa de música/dispositivos. A direção
visual desta primeira composição continua sujeita ao feedback do usuário.


## Biblioteca de músicas externas — 18/10/2026

Implementada a primeira etapa funcional no PC: F9/Options > SOUND, opção independente
inicialmente desligada, listas locais por contexto, arquivos/pastas, edição de
nomes/ordem/ativação, shuffle, prévia, importação/exportação M3U/M3U8 e reprodução
com pausa/volume existentes. Classic restaura fontes originais. Catálogo separado,
sem alterar arquivos de áudio ou saves. Detalhes: [CUSTOM_MUSIC.md](CUSTOM_MUSIC.md).

A reformulação completa continua parcial: faltam catálogo unificado disco/externas,
tags embutidas, cópias gerenciadas/pacote de áudio, progresso/seek e seletores
validados nos consoles. Permanecem as demais etapas de inputs/volantes, gestão
visual de presets/saves e prévias contextuais animadas.


## Correcao de direcao: adaptar o Jukebox original

A tela nova Music Library foi substituida pelo projeto
slide/MusicRoot do disco. Seus scripts e widgets fazem biblioteca, playlist,
reproducao, ordem e shuffle; Manage acrescenta arquivos/pastas, gestao de listas,
contextos e M3U. Disc/Local/Manage ficam na lateral, deixando o titulo sozinho no
topo. O backend de streaming local permanece opcional; os objetos da lista externa
ficam isolados dos playlists PS2. A composicao separada anterior nao e mais a UI
entregue. Permanecem os limites de plataforma/tags/pacotes/seek descritos em
[CUSTOM_MUSIC.md](CUSTOM_MUSIC.md).


## Direcao final do Jukebox — substitui as entregas anteriores

Pedido explicito: sem F9 e sem separar musicas do disco das do usuario. A entrada
agora e o Jukebox original do GT Mode. Uma unica Library alimenta playlists mistas,
inclusive corrida e slideshow; os botoes e scripts originais fazem selecao, adicao,
remocao, ordem, Play/Stop e Shuffle. Playlists na lateral acrescenta importacao e
gestao de listas nomeadas. A UI anterior Disc/Local e o acesso pelo launcher foram
removidos. Toda decisao visual/fluxo deve partir da direcao da Polyphony da epoca,
com extensoes integradas a linguagem e aos recursos existentes do jogo.

A reproducao unificada e opcional, guarda referencias locais/do disco, preserva os
arquivos e os blobs PS2 e e desligada por Classic. Detalhes e testes atuais em
[CUSTOM_MUSIC.md](CUSTOM_MUSIC.md). Os registros anteriores desta nota sao historicos
e nao descrevem mais a entrada ou a composicao entregues.

## 19 October: user-requested simplification

The separate Playlists manager is removed. Music is read from a folder and appears
in the original Library; original racing/slideshow controls do the organization.
This supersedes the preceding manager/file-picker workflows. See CUSTOM_MUSIC.md.
