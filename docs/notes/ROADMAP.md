# Roadmap do OpenGT4

Atualizado em 29/09/2026. Este é o mapa geral: onde o projeto está, o que
vem depois e em que ordem. Os detalhes de cada área ficam nos documentos
indicados. Quando uma etapa terminar ou a ordem mudar, atualize aqui.

## A visão em uma frase

**Como a Polyphony teria feito, e mantido vivo, o GT4 no PC.** Não é só
rodar o jogo igual ao original.

Primeiro o **OpenGT4 Classic** (o GT4 do PS2 rodando nativo no PC, o mais
fiel possível), depois o **OpenGT4 Enhanced** (a edição moderna, no espírito
do OpenTTD), e um dia o **multiplayer competitivo online** no Enhanced.
Decisões e regras: [EDITIONS.md](../EDITIONS.md).

## Decisões já tomadas

| Data | Decisão | Onde |
|---|---|---|
| 27/09 | Port nativo, não emulador: a lógica do jogo é traduzida para C; o que fala com o hardware do PS2 é trocado pelo equivalente do PC. O recompilador só serve para análise e para validar (oráculo). | `docs/HOW_IT_WORKS.md` |
| 27/09 | Menus rodam os scripts Adhoc originais num interpretador nativo. | [ADHOC.md](../ADHOC.md) |
| 29/09 | Classic primeiro, Enhanced depois. | [EDITIONS.md](../EDITIONS.md) |
| 02/10 | Classic e Enhanced são dois modos do mesmo executável (não dois exe); mods só no Enhanced; o Classic imita o aspecto do PS2. | [EDITIONS.md](../EDITIONS.md) |
| 29/09 | Física e regras iguais nas duas edições (tempos, ghosts e saves valem nos dois). | [EDITIONS.md](../EDITIONS.md) |
| 29/09 | Enhanced com melhorias ligadas por padrão (render moderno, teclado e mouse). | [EDITIONS.md](../EDITIONS.md) |
| 29/09 | Multiplayer futuro: servidor confere as corridas pela física headless; save online só no servidor. | [EDITIONS.md](../EDITIONS.md) |
| 29/09 | A vista de corrida (`race_view.c`) é separada do showroom dos menus. | [PHYSICS_STATUS.md](PHYSICS_STATUS.md) |

## Onde estamos

**Pronto (Classic):**
- Leitura direta do disco (volume, imagens, textos, fontes, modelos, pistas).
- Menus do Arcade nativos, com vídeos, música, cursor e rodapé originais;
  GT Mode com mapa e páginas em andamento.
- Showroom com os modelos originais dos carros e as cores.
- Vídeo em OpenGL com upscaling (FSR 1).
- Física do carro inteira em C, validada bit a bit contra o original:
  motor, câmbio, pneus, suspensão, colisão com a pista e paredes, auxílios,
  voltas, pit, relógio e posições da corrida.
- Corrida montada sem savestate, com largada lançada como no jogo em 94 das
  110 pistas.
- Todas as pistas desenhadas com o asfalto inteiro no lugar certo.
- Demo de direção (`opengt4_drive`): Time Trial sozinho, qualquer carro e
  pista, teclado ou controle, rodas que viram e giram.

**Ainda não existe:** adversários, HUD e telas da corrida, som da corrida,
corrida a partir do menu com física, modos de jogo completos, saves.

**Dívida principal:** cerca de 250 funções originais ainda rodam pelo
recompilador durante a montagem e o passo da corrida (IA, carro contra
carro, partes do init e da sessão, utilitários). O exe final do Classic não
pode depender do recompilador; elas precisam ser traduzidas para C.

## Fase 1: OpenGT4 Classic

Na ordem. Cada etapa termina validada (bit a bit onde é lógica do jogo,
por comparação com o PCSX2 onde é visual ou som).

### 1. Adversários (próximo)
- Corrida com 6 carros: garagem, piloto e vaga no grid de cada um.
- IA dos pilotos (`func_00463C28`, `func_00465078` e o que chamam) em C.
- Colisão carro contra carro (`func_003F4638`) em C.
- Desenhar todos os carros na vista de corrida.
- **Precisa do usuário:** savestates de uma corrida do Arcade contra a IA
  (na largada, no meio da primeira volta, com carros se encostando).

### 2. Corrida 100% nativa
- Traduzir para C o que ainda roda recompilado na corrida.
- Mover a corrida de `tests/physics/drive.c` para o jogo (`src/`), sem o
  recompilador e sem o executável do PS2 (`core.elf`) em tempo de execução:
  os objetos da corrida passam a ser montados em C, como o jogo monta.

### 3. Corrida dentro do jogo
- Do menu Arcade para a pista, com física (hoje o F5 abre a pista sem física).
- Contagem, largada, bandeiras, fim de corrida e tela de resultados.
- HUD original: posição, volta, tempos, velocímetro, conta-giros, mapa.
- Pausa, reiniciar, sair.

### 4. Som da corrida
- Motor, pneus, batidas, vento, a partir dos dados de som originais.

### 5. Visual fiel da corrida
- Céu, luz e reflexo de cada pista, sombra do carro, transparências e
  passes de desenho como no PS2; câmeras (perseguição, para-choque,
  cockpit); replay.
- Referência: prints e savestates do PCSX2.

### 6. Modos de jogo
- Arcade completo (corrida simples, Time Trial, ghost e recordes).
- GT Mode: licenças, eventos, dinheiro, garagem, concessionárias, oficina
  e ajustes, B-Spec.
- Saves (o memory card vira arquivo no PC).

### 7. Controles
- Controle e teclado mapeáveis; volante com force feedback (ex.: G29).

### 8. Lançamento do Classic 1.0
- Sem recompilador e sem arquivos do PS2 além do disco do próprio usuário.
- Instalação simples, aviso de projeto não oficial, testes automáticos.

## Fase 2: OpenGT4 Enhanced

Começa quando o Classic estiver jogável. Mesmo código e mesmo exe, outro modo
([EDITIONS.md](../EDITIONS.md)).
- Renderer moderno: sombras reais, reflexos em tempo real, iluminação nova,
  widescreen, resoluções altas, campo de visão.
- Teclado e mouse nativos na interface.
- Melhorias de interface e qualidade de vida.
- Suporte a mods (carros, pistas, regras) com formato documentado.
- A física continua a do original.

## Fase 3: online (longo prazo, Enhanced)

- Corridas competitivas: o servidor roda a física sem gráficos e confere
  cada corrida pelos inputs; tempos que não se reproduzem são recusados.
- Save do multiplayer só no servidor; save offline livre no PC.
- Antes: garantir que a física dá o mesmo resultado em qualquer CPU,
  sistema e compilador.
- Pesar na hora o risco de visibilidade de um serviço online.

## Em paralelo, a qualquer momento

- Outros sistemas (Linux, macOS) e consoles: o núcleo só fala com o
  hardware por `include/platform/platform.h`.
- Documentar os formatos do jogo à medida que são entendidos.

## Documentos por área

