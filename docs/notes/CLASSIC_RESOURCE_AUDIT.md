# GT4 original: inventário de recursos — 13/10/2026

Auditoria documental do código e dos testes registrados, antes de qualquer
implementação. Substitui a lista funcional de 12/10. A base é única, conforme as
regras do projeto; medir fidelidade no preset Original. Classic nos títulos antigos
não estabelece uma edição separada.

F = caminho original ausente; A = implementado com comparação incompleta.
E/C certificam somente as fronteiras explicitadas em FIDELITY.md. N = ausente,
fora do escopo documentado até nova escolha; não significa recurso terminado.

## Faltas reais, por impacto para o jogador

Estimativas de esforço para implementação, leitura do original e provas
positiva/negativa, por item, em dias úteis. Não são prazos prometidos nem
incluem descobrir/corrigir divergências anteriores de toda a suíte. A ordem
considera carreira, configuração, funções de foto e detalhes de apresentação.

| Ordem | Item delimitado | Nota | Tamanho estimado | Código/evidência e consequência |
|---|---|---|---|---|
| 1 | Evolução persistente da experiência B-Spec | F | G: 5–10 dias | career_home.c e gtmode_game.c só leem statistics.b_spec_experience_ps2; save_ps2.c importa 0x180 bytes. Não há produtor de progressão ao correr. Getters originais 00467C58/00467D90/00467F60/004680D8 têm prova; gtmode_home/purchase_menu semeiam dados. Piloto/ordens/monitor já funcionam, mas não provam evolução de uma carreira nova. Localizar o produtor original antes de escrever código. |
| 2 | Favoritos de carros/pistas, listas e filtros | F | M: 2–4 dias | menu.c getSize/has apontam a nat_empty_favorites, que retorna 0; favorite_course sem métodos funcionais. arcade.adc CarRoot/MakerRoot/buildFavoriteCourse e option.adc SecretRoot precisam has/isFull/append/remove/buildLabelList. Salvar/importar dados não faz o recurso funcionar. |
| 3 | Screen Setting e correção de cor na corrida/replay | F | M: 2–5 dias | race_pause.c grava brightness/contrast; só draw_license_screen consome race_pause_display_options. race_color_correction/replay_color_correction apenas inicializados/persistidos. Primitivas 003EFDF0/003EFC10 existentes não provam aplicação à imagem normal inteira. |
| 4 | Sombra noturna projetada por cada lâmpada | F | M: 3–6 dias | race_view.c draw_shadow declara passe 0038F168/00451EE0 ausente. Há luz noturna RNW4 e sombra solar/contato; não falta todo o sistema noturno. |
| 5 | Photo Travel: movimentação/regiões, limites/oclusão e autorização de disparo | F | M: 4–7 dias | photo_mode.c marca 003FECB0/00371530 ausentes, usa região0 e limites globais; menu_widgets.inc can_shoot só retorna ready. Movimento/mapa 003704D0 incompleto. photo_mode_data prova dados dos 15 destinos; photo_travel_flow prova foto/export básico, não esses ramos. |
| 6 | Foto no replay: câmera original completa e restrições de zoom | F/A | M: 4–8 dias | PhotoPause já abre, tem banco até 64 câmeras, memória de ajustes, zoom de 18 frames, ajuda e captura. photo_travel.c substitui rejeição/partição/parede de 003FFC68/003FFD10 por altura do piso e não aplica mínimo dinâmico de zoom. photo_pause_movie/race_resources cobrem caminhos presentes; controle/composição integral A. Não listar Photo Drive inteiro como ausente. |
| 7 | Foto com obturador lento: histórico de objetos, partículas e equipe do box | F | M: 4–8 dias | photo_motion/race_resources cobrem carros/rodas/sombras/64 canais do piloto, 32 amostras e restauração sem avançar física. Falta interpolação temporal dos demais objetos; câmera editada recebe delta histórico host (A). Tempos escalares 0036F628/0036FA20 já E. |
| 8 | Importar ghost de melhor volta PS2 | F | M: 3–6 dias | FileReplayLicense 001CFE20/001D0360 não entra como ghost. Ghost GHST nativo funciona e tem prova tt_game_probe --saved: 5661 passos a 0 m, negativo 16,2 m. Aceitar ghost de outro carro precisa conferir o original antes de classificar como falta. |
| 9 | Revelação fiel de filmes PS2: detalhes restantes de cena/projeção | F/A | G: 5–10 dias | ps2_media.c/photo_travel.c já restauram wheel/wing/piloto/SMOK/PTCL/ENVP; faltam body/LOD CDTL, GDGT, plano de recorte SMOK, partículas entre snapshots e far clip original no develop. Aspecto/retrato já aplicado; rotação/composição integral e filme pessoal de referência A. film_develop/film_menu não provam os detalhes ausentes. |
| 10 | Sombra de cones/gadgets/objetos de start e finish | F | P/M: 2–4 dias | draw_gadgets não desenha modelo ímpar de sombra 003E4A28 +F0..1A8. course_objects/coffee_objects já cobrem modelos e matrizes, não o passe. Não alterar física/colisões. |
| 11 | Backfire visual: mistura de frames/cor e brilho no chão | F | P/M: 2–4 dias | draw_afterfire já desenha chama básica; faltam 00456850/004A60E0 e passe a2=1/model1. race_backfire/race_audio_clock comprovam produtor/áudio nas fronteiras descritas, não os pixels. |
| 12 | LAN, hoje bloqueado com Coming soon | N, funcionalmente ausente | GG: 3–6 semanas, alta incerteza | menu_widgets.inc lan_blocked e menu.c recusam o caminho; conductor NetBattle/NetRally/NetSinglePlayer não portado. Precisa protocolo original, duas instâncias, conexão/desconexão e comparação de sincronismo; opção explícita de escopo, não converter para Arcade nem inventar um protocolo e chamá-lo fiel. |

P/M/G/GG nesta tabela são tamanhos (pequeno/médio/grande/muito grande),
nunca notas de fidelidade; a nota está na coluna própria.

## Implementado: antigos F removidos, sem declarar equivalência integral

| Família | Evidência existente | Limite atual |
|---|---|---|
| Time Trial, ghost PC e ranking | tt_game_probe --saved/negativo; course_ranking e negativo; session_best_update | Integração C; passcode algoritmo E/caller A, relógio PC P; ghost PS2 F. |
| Replay e Replay Theater | replay_file; replay_types (48 fixtures); disc_replay_oracle (93 arquivos/556738 comandos); disc_demo_probe/disc_demo_app; replay_hud, negativos | Comandos E e fluxos C; trajetória/câmeras/datas/UI integral A. Famílias orbit/fixed/heli/car-point já implementadas em course_tvcam.c. |
| Saves, perfis e importação GT4 PS2 | profile_storage; gtmode_home; gtmode_setting_race; ciclos/reabertura e negativos | Formato/backend PC P, integração C; herança GT3 usa fonte sintética, containers/save real A. |
| Licenças, demos, ghost e Logger | license_load (80); license_menu_demo/disc_demo_playback; gtmode_logger_drive; logger_view; race_resources trilhas A/B; license_information/result_input/aids e negativos | Regras E/integração C, sincronismo e apresentação integrais A; não confundir demo guiada com volta destinada a ouro. |
| Setting/tuning | setting_defaults_oracle; gtmode_setting_race (722 carros, 150054 instalações, 2166 entradas); catálogo/curvas/reference; negativos | Instaladores E, setup C; todas as combinações/trajetórias e render/UI integral A. |
| Home, usados, GT Auto e carreira | gtmode_home/progress/status_start; purchase_menu; shop-catalog; extras_menu; ciclos de 144 séries/527 etapas/144 prêmios, 16 endurance | Getters/regras E, ciclos C em perfis independentes; experiência B-Spec F, carreira contínua 100%/apresentação A. |
| Missões | gtmode_mission_disc/flow/exit/cycle/catalog; mission_app; negativos | ResultArcade é a classe ORIGINAL (003BF6E8), falha/saída já implementadas. Missão9 ganha fisicamente,10 perde. 136 ciclos controlados não provam as outras 33 vitórias físicas/carros-prêmio. |
| Power & Speed | machine_test_tests, machine_flow, pregrid_menu e negativos | Três provas e Session Record funcionam; integração C, sessão/Las Vegas/apresentação integrais A. |
| Demos da abertura, slideshow, remapeamento, garagem/espelho/replay2P | outside_menu/playlist_cache; controller_input/controller_defaults_oracle; split_owned/mirror/replay e negativos | Fluxos ligados, backend PC P e provas específicas C/E; composição/mixer/periférico físico A. |
| Photo Travel, Photo Drive, PhotoPause e revelação | photo_mode_data, photo_travel_flow, photo_optics, photo_motion, photo_pause_movie, photo_drive_menu, film_menu/develop e negativos | Dados/coeficientes E, fluxos C, imagem A e subcaminhos F identificados acima. Ambiente CRSS não falta: photo_ambient executa 00457F60 em15 destinos/120 blocos, todos volume0/pitch1, com negativo. Isso não certifica todo o áudio do modo. |
| Idioma US | config-main.adc init_main caso us instruções130..138 e /projects/language/US | O disco suportado só oferece US. Sem oracle automático do seletor: A, não F. PAL/KR são outras versões. |

## Validações pendentes — não são recursos inteiros ausentes

- **Missões:** outras 33 vitórias físicas e entrega dos carros-prêmio pelos
  requisitos completos, resultado/falha/render/áudio contra original.
  G: 1–3 semanas, dependente de trajetórias e referências.
- **Licenças:** sincronismo integral das 80 provas, alcance das medalhas e
  apresentação/câmeras. Demos existem; o ensaio teacher-as-player não prova
  sozinho ausência ou impossibilidade de ouro. G: 1–3 semanas.
- **Carreira:** uma carreira nova contínua passando séries/endurance,
  desbloqueios/estatísticas e todos os prêmios. As 144 séries já têm ciclos
  físicos; não repetir a alegação de 132 séries não implementadas.
  G: 1–3 semanas; duração da reprodução depende do método original permitido.
- **Tuning/GT Auto/box/replays/HUD/áudio:** apresentação, sincronismo e
  combinações integrais ainda A; dividir por tela/passe/ciclo antes de
  estimar. Timbre CR-X não certificado; som existe, causa pendente e usuário
  adiou essa investigação. Comparar mixagem SPU2, não ajustar no ouvido.
- **Volante:** efeitos/256 forças virtuais e desconexão testados; ensaio e
  calibração física G29 A, adiado pelo usuário. Não listar rumble do controle
  como ausente: SDL_GameControllerRumble já chamado em race_game.c.

## Escopo e diferenças deliberadas

LAN, EyeToy e impressão ausentes N precisam decisão de escopo; EyeToy/print
envolvem adaptadores de periféricos (G: 1–3 semanas cada, alta incerteza).
Online original/DNAS indisponível: backend novo é outra decisão, sem estimativa
de equivalência a um serviço encerrado. Tourist Trophy não é recurso do GT4.
PNG, arquivos/perfis locais, teclas e resolução PC são P; comparação integral
não exige pixels idênticos ao GS (N documentado). Empacotamento ficou para depois.

## Execução e proveniência desta auditoria

Fontes: regras do projeto, ARCHITECTURE.md, FIDELITY.md, código de produção,
testes/registro CMake e disassembly local dos scripts originais arcade,
option e config-main. Arquivos locais de evidência em
build/resource-audit-20261013; nenhum asset/desmonte entra no Git.

Execução focal: replay_file, replay_types, photo_mode_data, photo_pause_movie,
gtmode_home, gtmode_setting_race, gtmode_mission_disc/flow/exit e license_aids,
com negativos registrados: **18/18 passaram**, 759,50s, executáveis
Release locais existentes. Log: build/resource-audit-20261013/selected-tests.log.
Não é aprovação da suíte completa nem comparação PS2 dos modos inteiros.
A execução concorrente apresentou falha no fixture gtmode_extras_menu,
corrigida em trabalho alheio à auditoria: pin/unpin do manager durante GC
([SUITE_FIXES.md](SUITE_FIXES.md)). O log build/suite_fixes/extras-focused.log
confirma reexecução 5/5, incluindo quatro negativos. A suíte completa ainda
não é certificada por este inventário.

Somente documentação alterada nesta tarefa. Nenhum item selecionado ou
implementado; aguardar escolha do usuário.
