# Validação contínua da carreira — 14/10/2026

## Escopo

Nenhuma fórmula, coeficiente, cadência ou implementação da física foi alterada por esta tarefa.
As alterações desta tarefa são nos testes e no registro da evidência. Carros,
peças e decisões dos pilotos automáticos são fixtures explícitas; isto não
prova uma campanha nova comprando e conquistando todos os pré-requisitos.
Não transformar tentativas perdidas em vitórias nem resultados controlados
na fronteira corrida/menu em prova física.

## Carreira: um perfil entre processos

`career_continuous.py` executa novamente as provas físicas, mantendo um
único slot isolado. Os recibos antigos escolhem somente os inputs. Cada
etapa usa chegada/posição nativas; quick menus originais pagam o dinheiro;
a lista original de presentes decide os carros. O teste confere saldo,
contador de prêmios, corpo completo dos carros anteriores, registros
anteriores e recarga do slot. Checkpoints e hashes permitem retomar sem
reescrever os resultados anteriores. Uma tentativa paga preservada no
checkpoint é registrada à parte, em vez de apagada para caber no catálogo.

O teste antigo retornava ao GT Mode entre etapas de um campeonato. Isso
não é o percurso original de quick-championship. Ao atingir 25%, podia
reabrir a página antiga de prêmio ainda não encerrada e repetir Nuvolari.
O teste corrigido volta ao GT Mode apenas ao terminar o campeonato; nas
séries independentes, encerra os presentes a cada retorno. Confirma os
botões originais de PresentRoot até encerrar sua sequência. O limiar de
50% no meio da série Altezza também é entregue uma única vez.
Dream Car entregou GT-R Concept LM e Nuvolari, uma vez cada. Evidência:
`build/career-continuous-resumed-v2/ex_dream_1000.log`. O ramo anterior,
com premiação repetida pelo percurso errado do teste, foi rejeitado.

Fontes: gtmode.ad PresentRoot.play L14450..14515, CarPane.OK.onActivate
L14694..14699, achievement L14245..14250 e TopRoot.onInitialize L1152 /
checkPresent L354..385. Dados PresentData/PresentTimeData lidos do disco.
`present_cycle` verifica a marca de entrega e a recusa de um prêmio já
entregue após save/load e troca para a VM de quick-championship; a
eligibilidade inicial é controlada nesse teste, não uma vitória física.
São exercitados os prêmios de 25%, 50% e 100%, corpo inteiro e cor fixa
(original one-based em gtmode.ad L14562..14575), marca, save/load e recusa
de repetição. Negativos alteram ID, corpo, cor e marca de entrega.

`career_profile_run` retorna pelo código original, confere o calendário e
as 25 entradas de pista de PresentTimeData, incluindo os sentidos reversos
quando CourseData.has_reverse exige, e repete após save/load. Os limiares
vêm do disco, consultando MCalendar.getElapsedWeek (001331A0 +0x6C).
Negativos alteram comparações de registro, corpo, pista e sentido reverso.
C vale para a persistência/integração exercitada, não para toda a campanha.

O resultado completo do catálogo exige 144 séries válidas no mesmo ramo.
Arquivos incompletos e campeonatos perdidos continuam incompletos; não se
reaproveita o título de campeão de um recibo antigo. Na execução atual,
Fórmula GT perdeu a classificação geral com alguns pilotos de teste; esses
ramos não receberam aprovação. Há outro percurso mantendo essa série para
o fim e validando as demais. Evidências locais: `build/career-continuous-*`.
Resultado do ramo aceito: **142/144 séries, 510 etapas físicas, 144
carros-prêmio**, saldo Cr. 13.628.090 e 655 dias. O contador original tem
511 corridas, pois preserva também a tentativa paga anterior registrada no
checkpoint. O perfil contém 288 carros: presentes mais carros/peças de
propriedade controlada para dirigir os eventos. Isto não é uma campanha
comprando todos os carros com o saldo. Todas as séries endurance do catálogo
estão incluídas; ainda faltam `rh_nursnow_h_0000` e `ex_formula_1500` nesse
mesmo ramo. Ambas tiveram tentativas físicas perdidas, sem prêmio da série.

Recibos e hashes: `build/career-continuous-remaining/results.json`; snapshot
antes da tentativa posterior rejeitada em `checkpoints/rh_nursnow_h_0000`.
`build/career-checkpoint142.json` permite retomar esse perfil. A cópia isolada
`build/career-continuous-142-profile.log` confirma 511 corridas, dinheiro,
144 presentes e todas as pistas por calendário/reversos após save/load.
As 142 linhas aceitas não contêm erros do script original; o runner recusa
logs com `script:`. O ramo com derrota posterior não é usado como se tivesse
terminado o catálogo. Vitórias isoladas antigas não substituem esse recibo.

Filmes de encerramento são registrados pelo despacho original; sua
reprodução e sua apresentação não são certificados pelo teste de prêmios.

## Missões: sete vitórias físicas

C, tolerância zero para registro/dinheiro/save: missões 1, 4, 8, 9, 21,
22 e 24 chegaram em primeiro com comandos de controle ou seleção do
controlador nativo. O teste exige painel original de sucesso, chegada e
primeiro lugar antes de aplicar o resultado. Confere PrizeRoot, AfterRoot,
créditos, registro e recarga. O teste padrão mantém também a derrota física
da missão 10, sem painel de sucesso nem pagamento.

A grade tem a quantidade e os carros reais da missão; o painel recebe os
ranks/tempos nativos. Pagamentos maiores usam makePrizeFast original e
seus watchers, em vez de pressupor que dez atualizações já pagaram tudo.
Negativos alteram o registro esperado e falham. Evidências locais:
`build/mission-final-focused.log`: 15/15 testes positivos/negativos.

Os pilotos em mission_pad_fixture.h são P de teste. Só produzem comandos
observando geometria; ganhos, alvo, antecipação e decisões de ultrapassagem
não são apresentados como IA original. As linhas, sua interpolação e todos
os efeitos de dinâmica/vácuo permanecem nas APIs nativas. Não há escrita de
posição, chegada, classificação ou parâmetros para obter uma vitória.

`mission_victories.py` mantém os fracassos e só aceita chegada, rank 1 e
estado 2; carregar a prova não basta. **As outras 27 vitórias físicas ainda
não estão provadas**, nem a apresentação integral das 34 no PS2. Os ciclos
controlados antigos nas 34 não substituem essa pendência.

## Licenças: comparação ampliada, não integral

E no método 003F8D70, dentro das fronteiras declaradas: as 80 provas reais
preparadas pelo disco deram 400 checkpoints idênticos na avaliação e RAM,
exceto a pilha do chamador do oráculo. O teste adicional usa as fitas do
disco como comandos do aluno e compara avaliação a cada frame da tentativa:
183.999 comparações nas 80. Compara conductor e janela do objeto de corrida,
como o oráculo sintético; callbacks de carro/evento são fronteiras comuns.
O negativo altera um byte e falha.

Isto **não** compara o construtor ou o solver completo, nem certifica a
trajetória integral, os pixels, o som, as medalhas ou a cena de demonstração
das 80. Algumas tentativas com fita de demonstração no papel de aluno
falham ou terminam antes da fita: esses resultados não foram convertidos
em aprovações de licença. O nome PASS nesses logs refere-se à comparação
da avaliação, não a uma medalha. A dívida integral das 80 continua A.

Evidências: `build/license-evaluation-tapes.log`,
`build/license-final-focused.log`: 8/8 positivos/negativos. A comparação sintética anterior
de 30.720 casos continua separada. Sem captura ou evidência original que
cubra os demais comportamentos, não ampliar a nota para a licença inteira.

## Reprodução

Compilar somente os alvos da área: gtmode_series_playable,
gtmode_series_pace, gtmode_career_profile, gtmode_present_cycle,
gtmode_mission_cycle e gtmode_license_session_oracle. Executar CTest com
regex de career_continuity/present_, mission_cycle e license_session /
license_actual_catalog / license_tape_evaluation. Nunca usar a suíte inteira
como substituto dessas provas. A execução longa precisa dos recibos locais
de inputs, da ISO do usuário e de um diretório novo. Assets, saves, dumps,
bytecode e código recompilado permanecem fora do repositório.

Regressões focadas da carreira/presentes: `build/career-final-focused.log`,
10/10 positivos/negativos. Somadas às missões e licenças, 33/33; isto não
transforma provas físicas faltantes em aprovações. A suíte inteira não foi
executada. Nenhum executável do jogo foi publicado ou instalado nesta tarefa.

Após reforçar a recusa de erro de script no harness e no runner, recompilados
os quatro alvos da carreira e repetidos positivo/presentes: 6/6 passaram
(`build/career-guard-focused.log`). Os hashes e a ausência de erros de script
nos 142 recibos aceitos foram conferidos novamente.
