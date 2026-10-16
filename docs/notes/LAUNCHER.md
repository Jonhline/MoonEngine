# Launcher em duas etapas

16/10/2026: este é o launcher **secundário**. O oficial do Moon Engine é
outra contribuição, ainda fora do repositório, e fica com os nomes
`launcher*` na pasta de dados; este usa `secondary_launcher.ini`,
`secondary_launcher-options` e `secondary_launcher_photos` (os antigos são
movidos uma vez), classes Windows `MoonEngineSecondary*` e título
"Moon Engine - secondary launcher". Ver `docs/LAUNCHER.md`.

13/10/2026. O seletor anterior à validação mostra somente Moon Engine, o pedido
da ISO NTSC-U/SCUS-97328 e o botão do seletor Windows já existente.
`launcher_validate_disc` decide a etapa; escolher uma ISO inválida remove
o menu completo, mesmo que exista cache de outra imagem.

Sem disco, fundo quadriculado, faixa, tipografia do sistema e botão dourado
são uma adaptação PC (P), solicitada pelo usuário. Não dependem de assets
do jogo. Não há cursor desenhado, imagens de carros ou slideshow nessa etapa.

## Fonte do menu completo

`launcher_skin.c` lê `/projects/option/option.mproject` e
`/projects/option/US/OptionRoot.gpb` da ISO escolhida, via os leitores VOL,
GPB e TEX1 existentes. Os 12 gráficos são fundo, janela, Race, Replay,
Screen, Sound, JP/Controller, Album, Misc, exit, exit_over e Color.
Fontes hb_24, hc_20 e hm_16 vêm de `/font/system.fnt` e `/font/menu.fnt`.
O avanço dos glifos usa o port de 00492978.

Os widgets Item, Group e ScrollWindow são consultados no mproject em tempo
de execução: Item 102x91; imagem y=9; label y=54; ScrollWindow x=26/y=62.
A montagem usa cinco colunas como OptionRoot em option.ad. Geometrias
640x480 são escaladas para a janela PC 900x700. ColorFace usa as quatro
cores originais; os tiles preservam cantos e degradês do próprio gráfico.

As ações PC são Play, ISO, vídeo, áudio, controle, pasta de dados, startup
e Exit. A área de detalhes ocupa três espaços da segunda linha usando
opt_window2L. Essa composição e os diálogos PC são P; não correspondem às
ações e à quantidade de grupos do menu PS2. O texto de detalhes e os
campos PC ainda usam fonte do sistema. Rasterização GDI não tem prova de
igualdade com GS; comparação visual integral permanece A.

## Cache

`moon_pref_path()/secondary_launcher-options`, no AppData.
Nome inclui caminho da ISO, tamanho e última modificação. Pacote OGS3:
mproject, 12 imagens RGBA, dois arquivos de fonte e CRC32. Cache truncado
ou corrompido é descartado e reconstruído da ISO. Nenhum gráfico ou fonte
do disco é distribuído no repositório ou pacote.

## Evidência

- `launcher_skin`: compara 356.489 texels dos 12 gráficos com decodificação
  independente do GPB, compara fontes entre abertura e cache, consulta
  geometria, rejeita ISO inválida e reconstrói cache corrompido.
- `launcher_skin_negative`: altera um canal do Screen e o comparador falha.
- `launcher_window`: janela real, uma ação sem ISO e oito com ISO válida;
  troca válida/inválida/válida; cache reutilizado; navegação; Apply/Cancel;
  conflitos, remapeamento 1P/2P, persistência e parâmetros de Play.
- `launcher_window_negative`: expectativa errada de resolução falha.
- Somente `ctest -R '^launcher_'`: 8/8, incluindo config/negativos e picker.
- Captura do original: `MoonEngine.exe <iso> option OptionRoot --shot
  build/launcher-option-reference.png`. Comparação visual feita com as
  capturas do launcher; não é certificação pixel a pixel.
- `build/launcher-simple-refined.png` registra a tela inicial refinada.

Nota E limitada aos texels e à geometria comparados exatamente; P para as
adaptações PC e A para equivalência completa do raster final. Física não
foi alterada. Só os testes do launcher foram executados, conforme pedido.

## Atualização Oracle (13/10/2026)

Recurso PC, nota P. `MOON_VERSION` vem do CMake e é gravada no código e
no VERSIONINFO do exe. `tools/configure_update.py` gera o header somente
no build, lendo URL/token de `%APPDATA%/MoonEngine/publish.ini`, fora do repo.
Sem configuração de servidor, a checagem é inativa.

`launcher_update.c` executa WinHTTP HTTPS num worker com memória própria,
sem depender da janela depois de fechada. Certificados são verificados pelo
Windows; redirects são recusados para não encaminhar o Bearer. Resposta
não-200, erro, manifesto malformado e versão igual/menor não mostram aviso.
Versões aceitas: X.Y.Z, sem pré-release. Manifesto limitado a 16 KiB e setup
a 1 GiB; nome do setup é um basename .exe, sem traversal. O manifesto usa
version/setup/sha256/size/notes. Textos JSON podem ser UTF-8; escapes Unicode
não-ASCII em notes são validados e substituídos por '?' (notes não é exibido).

Aviso e Update ficam no rodapé; Check for updates persiste em launcher.ini.
Download fica numa pasta temporária exclusiva. Tamanho e SHA-256 BCrypt são
conferidos antes de oferecer o arquivo ao instalador e novamente ao executar.
Falha nunca executa o setup; arquivos temporários recusados são removidos.
O botão permanece desabilitado durante o download. A janela é fechada após
CreateProcess do setup com /VERYSILENT /CURRENTUSER /SUPPRESSMSGBOXES
/NORESTART /DIR=<diretório atual>. Inno usa AppId estável, mantém AppData e
saves fora do destino, e reabre Moon Engine mesmo em execução silenciosa.
O temporário de um setup executado fica para limpeza posterior pelo Windows.

`tools/publish_update.py VERSION` configura e compila o release com runtime
estático, chama package_release.py (ícones antigos agora opcionais), compila
Inno, calcula hash/tamanho e gera latest.json. SCP envia .incoming; SSH
promove o instalador primeiro e o manifesto por último. Host/usuário/chave,
diretório remoto, token e domínio só ficam em publish.ini. SSH valida a chave
do host previamente conhecida. .gitignore exclui publish.ini e chaves.

Configuração externa:

```ini
[update]
url = https://<domínio>/moonengine/
token = <token de leitura dos binários>
[ssh]
host = <host>
user = <usuário>
key = <caminho da chave privada>
directory = <pasta remota>
[package]
iscc = <caminho de ISCC.exe>
; ffmpeg e ffmpeg-license são opcionais
```

Infraestrutura: rota estática no Caddy já existente do nuntius. Token
independente do upload do nuntius; autenticação é executada antes do
file_server dentro de route. Só latest.json e setups são servidos. Rota
original do nuntius preservada; backup local/servidor feito antes da edição.
Credenciais e Caddyfile com token não entram neste repositório.

Prova: 12/12 testes `ctest -R '^launcher_'`, incluindo launcher_update,
negativo, launcher_publish, janela e HTTPS real. Casos: versão maior/igual,
desligar/persistir, JSON inválido, nome inseguro, tamanho/hash incorretos,
upload atômico e config privada. `launcher_update_https` usa somente o
publish.ini externo: sem token/token errado dá 401 para manifesto e setup;
WinHTTP baixa o setup real e recusa o download com hash adulterado. É opt-in
via MOON_UPDATE_LIVE_TEST=ON; os testes normais não exigem rede.
Release 0.3.0 gerado, hash validado e publicado via SCP/SSH. FileVersion do
exe de desenvolvimento 0.2.0 conferida no Windows. Captura update-offer.png
inspecionada em build/launcher-window. `tests/ui/launcher_installer.py 0.3.0`
provou instalação silenciosa, atualização por cima, arquivo preexistente
preservado e reabertura do exe em dois ciclos. Teste em
build/launcher-install-proof, com AppId isolado e sem atalhos, mesmos Files/Run
do setup; processo e instalação de teste removidos. A instalação real do
usuário não foi modificada.

## Cursor, sound and launcher presentation (13/10/2026)

Latest user request replaces the earlier no-cursor presentation: a native
Windows cursor uses image/cursor/cursor.png from CursorRoot.gpb, hotspot
(16,0) of the original 32x32 sprite (00230CF8/0022FFD0). Physical mouse
coordinates own it; it does not move to a focused button. Invalid/no ISO
restores the system arrow and stops disc audio.

Options cache is now OGS6: 34 RGBA assets, mproject and the two font files.
SoundRoot.gpb contributes the original bt_*/bt_o* button pieces and the
pulldown frames. All launcher pages, Update and Updates: On/Off use these
pieces with the original hm_16 font. Original font contours determine
paragraph wrapping in the details panel; long field labels fit the PC
control bounds. Closed combo drawing covers the Windows arrow while the
native hit testing, drop list and keyboard input remain functional.
Game disc uses the ISO's GT4logo instead of the Replay camera. Screen/Sound
page icons retain their aspect ratio. Centered PC forms and On/Off update
control are P; they are not the original PS2 Options action layout.

Hover fades original silver/gold button art over 140 ms, including footer
buttons and update controls. Tile tint is OptionItem.bgf from the ISO.
The duration and its application to the mouse are P, explicitly requested.

launcher_audio owns an SDL output device and a cancellable preparation
worker. It uses the existing native GTSE and SQT decoders: cursor/select/
ok/cancel from bank 0 of gt4sys.ins, named by gt4se.inf; navigation source
00232E00/002359B0, confirmation/cancel from option.ad. The standalone
launcher chooses map sequence 0, since OptionRoot inherits parent BGM
rather than selecting a track itself. Selection is P. Menu BGM/SE volumes
apply to preview immediately; Cancel restores persisted settings. Audio
stops before the game starts, before another disc loads and on close.
No assets are embedded or distributed.

launcher_fsr reconstructs image assets with AMD FSR1 EASU/RCAS, the same
shader sources and constants as the game backend, in an owned hidden GL
context. Derived pixels are retained only in RAM; raw AppData cache remains
lossless. Unsupported GPU/context falls back to original decoded images.
Text positions, contours and layout stay at the window resolution. This
launcher enhancement is P. Shader programs and targets belong to the
launcher and are destroyed before the game creates its rendering context.

Proof: launcher_skin compares all 34 decoded/cache assets exactly,
validates OS cursor hotspot and paragraph fit/rejection. launcher_audio
compares all four routed effect waveforms to independent GTSE bank lookup,
and the BGM prefix to a separate native SQT decoder; also checks audible
music, mute, absent disc and deliberate sample corruption. This proves the
integration, not complete SPU2 parity (existing reverb remains provisional).
launcher_fsr compares every RGBA byte to the existing game FSR backend on
this GPU, rejects invalid input, recreates the context and detects a changed
pixel. launcher_window checks valid/invalid transitions, asset-only-after-ISO,
hover midpoint/end rendered differences, update persistence and all forms.
Scripted UI probes ignore physical controller input to avoid interference
from another application. No gameplay or physics changes were made.


### Launcher without a startup console (13/10/2026)

P: the Windows player executable now defaults to the GUI subsystem,
including local builds. Console test harnesses remain unchanged; developers
can explicitly opt into MOON_WINDOWS_GUI=OFF. Existing CMake caches need
-DMOON_WINDOWS_GUI=ON (applied to build/Release/MoonEngine.exe here).
launcher_gui checks the actual player PE subsystem; launcher_gui_negative
rejects the console test harness. Only launcher tests were run.
Launching a .cmd file still starts its own command interpreter; open the
executable or a direct Windows shortcut instead.


### Composited launcher page transitions (13/10/2026)

P: native Windows page creation is enclosed in WM_SETREDRAW suspension;
the launcher uses WS_EX_COMPOSITED so the parent and descendant controls
are buffered together. Redrawing resumes after layout, values and focus
are ready, with RDW_ALLCHILDREN | RDW_UPDATENOW. This prevents newly
created Controls fields painting over the previous Home background.
launcher_window and its negative cover page navigation, settings and the
rendered completed pages; they do not measure display scanout during the
transition. No PS2 animation timing or gameplay was changed.


### Launcher panel margins and music mute (13/10/2026)

P: footer buttons were moved inside the details panel; the Controls player
selector now fits inside its header without overlapping the title. Removed
the duplicate Cancel/Default/OK footer text. Music: On/Off uses the disc
button artwork and persists mute_music in launcher.ini. Muting passes zero
BGM volume to the existing mixer while retaining menu effects and all game
volume settings; Audio preview and Test audio respect the launcher mute.
launcher_window exercises mute, persistence across reopen and restoration;
its negative remains active. launcher_config and launcher_audio cover
configuration and the mixer (including mute) with their negative cases.
Screenshots are build/launcher-window/controls.png and audio.png. Layout
is a deliberate PC adaptation rather than a claim of PS2 pixel parity.


### Launcher Play consumed by SDL event pump (13/10/2026)

P/C (PC integration): SDL WIN_PumpEvents consumes WM_QUIT with
PeekMessage(PM_REMOVE). Play dispatched during controller polling destroyed
the native launcher, but GetMessage then waited forever with BGM alive.
Exit now also checks IsWindow before waiting; controller polling returns
when SDL dispatched destruction. pump-play queues Play through SDL itself.
The uncorrected build timed out; the corrected launcher forwards BOOT and
the packaged 0.3.2 opens the native game window. 18/18 launcher tests pass.
Release 0.3.2 also restores the existing FFmpeg binary and license omitted
from 0.3.1. No physics or disc-version logic changed. Friend PC validation
remains pending; local reproduction covers the reported hidden-process hang.
