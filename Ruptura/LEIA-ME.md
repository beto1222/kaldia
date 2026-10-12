# RUPTURA: PROTOCOLO ZERO — A Cidade Morta (survival horror)

Projeto Unreal Engine 5 (C++). A fase inteira é gerada por código, então funciona num mapa vazio.

Setor 7, 03:13. Setenta e duas horas sem contato. Um sinal de socorro ainda sai da instalação
militar no fim da avenida. Kael vai sozinho, com uma pistola, uma espingarda quase sem cartuchos
e uma lanterna com pouca bateria. O que aconteceu lá dentro se chamava **Protocolo Zero**.

## Como abrir e jogar
1. Instale **UE 5.4 ou 5.5** e **Visual Studio 2022** (workload "Desenvolvimento de jogos com C++").
2. Clique com o botão direito em `Ruptura.uproject` > **Generate Visual Studio project files**.
3. Abra `Ruptura.sln`, configuração **Development Editor / Win64**, e compile (Ctrl+Shift+B).
4. Abra `Ruptura.uproject`. No editor: **Tools > Execute Python Script** > `Scripts/setup_projeto.py`
   (cria o mapa `L_CidadeMorta`, o volume de navmesh, o material PBR e **importa os 61 sons** de `AudioFonte/`).
5. Clique em **Play**. Use fones de ouvido e jogue no escuro.

Se o script não criar o volume de navegação: Place Actors > Volumes > **Nav Mesh Bounds Volume**,
posição (8000, 0, -200), escala (95, 35, 10). Sem ele os infectados ainda andam, mas em linha reta com desvio simples.

## Controles
| Ação | Teclado/mouse | Controle |
|---|---|---|
| Mover / olhar | WASD / mouse | analógicos |
| Correr (fôlego limitado) | Shift | L3 |
| Agachar (silencioso) | C ou Ctrl | B / Círculo |
| Esquivar | Alt | D-pad ↓ |
| **Lanterna** | **F** | D-pad ↑ |
| **1ª / 3ª pessoa** | **V** | D-pad → |
| Mirar / atirar | botão direito / esquerdo | LT / RT |
| Recarregar | R | RB |
| Trocar arma | 1 / 2 / roda | Y / Triângulo |
| **Faca** | **Q** ou botão lateral do mouse | R3 |
| Granada | G | LB |
| Interagir (segure em terminais) | E | X / Quadrado |
| **Estado, inventário e objetivo** | **Tab** (segure) | View/Select |
| Ajuda · dificuldade · qualidade | H · F1–F3 · F5–F8 | |

## O que mudou na versão de terror
**Atmosfera e imagem**
- Noite fechada: lua quase invisível, luz ambiente mínima, exposição automática limitada para o
  "olho" não se acostumar com o escuro. A lanterna é a principal fonte de luz.
- Neblina volumétrica densa: o feixe da lanterna aparece no ar e engole o fim da avenida.
- Pós-processamento de filme de terror (granulado, contraste, sombras frias, desfoque de movimento sutil).
  A câmera de Kael ainda modula vinheta, aberração cromática, saturação e tom avermelhado conforme
  o **medo** e os ferimentos.
- Cidade morta: quase todos os postes queimados ou falhando, janelas apagadas, corpos cobertos por
  lençóis, rastros de sangue, ambulância abandonada com a sirene piscando, barreira policial derrubada.
- Salão do reator tomado por **carne**: montes de biomassa, tentáculos pendurados do teto, casulos humanos.

**Kael**
- Câmera sobre o ombro bem fechada (estilo survival horror) ou **primeira pessoa** (V), com o corpo
  escondido mas projetando sombra.
- **Lanterna com bateria**: fica fraca e amarelada abaixo de 25%, falha no fim da carga e com interferência
  (o Hospedeiro e os apagões fazem ela piscar). Pilhas são raras.
- **Fôlego** limitado para correr e esquivar; respiração ofegante.
- **Medo** (infectados por perto, perseguição, escuridão, ferimentos): batimentos cardíacos,
  respiração, câmera instável e **mira trêmula de verdade**.
- Sem regeneração de vida: só kits de primeiros socorros. Ferido, Kael manca.
- **Pistola 9 mm** e **espingarda calibre 12** (9 chumbos), munição escassa, sem traçantes.
  Cada tiro ilumina o ambiente por um instante… e atrai todos os infectados que ouvirem.
- **Faca** (Q) para quando a munição acaba: empurra o infectado e interrompe o ataque.
- HUD mínimo: sem barra de vida e sem marcadores de inimigos. Munição aparece ao mirar; bateria e
  fôlego quando importam; estado completo e direção do objetivo só segurando **Tab**.

**Infectados** (substituem os soldados)
- Corpo procedural com andar arrastado e mancando, tronco curvado, espasmos bruscos da cabeça,
  braços estendidos na perseguição, golpe com o braço e "bote" para frente.
- Enxergam mal no escuro e ouvem muito bem: a **lanterna acesa** (principalmente apontada para eles)
  e os **tiros** denunciam você. Agachado e no escuro, dá para passar despercebido.
- Ao perceber o jogador, gritam e atraem os outros próximos. Os **mutantes** são maiores, mais rápidos e gritam mais longe.
- Alguns estão **caídos como cadáveres** e levantam quando você chega perto (ou atira por perto).
- Tiro na cabeça é muito mais eficiente; tiro de pistola ou rajada de chumbos atordoa.

**O Hospedeiro** (substitui o robô COLOSSO)
- Massa de carne com braços longos e garras, dormente num casulo até você entrar no salão.
- **Investida**: se errar e bater num pilar ou parede, fica atordoado com o **coração exposto nas costas**
  (ponto fraco, dano extra). **Pancada no chão** com onda de choque. **Vômito de bile ácida** em arco.
- A 50% de vida fica furioso: mais rápido, ataques mais frequentes, luzes do salão falham.

**Sustos roteirizados**
- Os postes da avenida estouram um a um na sua direção… e no fim da escuridão há **alguém parado**,
  recortado contra as luzes vermelhas do portão. Ele some quando você ilumina ou se aproxima.
- No posto da guarda, a luz estoura ao pegar o crachá — e o armário não estava vazio.
- Na rampa do bunker, a **porta bate e trava atrás de você** e as luzes se apagam em sequência; depois, batidas na porta.
- Na ala subterrânea, algo enorme esmurra a porta do reator por dentro.
- Ao religar a energia, as lâmpadas brancas estouram e só a luz vermelha de emergência fica — e a porta do reator se abre.
- Na fuga, o subsolo desaba: poeira e destroços caindo do teto, tremores, horda pelo caminho.
- Diretor de medo: sussurros, passos, choro, rangidos e gritos distantes tocam **atrás de você** de tempos em tempos.

**Áudio**
- 61 sons gerados proceduralmente por `Scripts/gerar_audio.py` (Python puro, sem dependências) e já
  incluídos em `AudioFonte/`: tiros com reverberação, passos molhados, gemidos/rosnados/gritos dos
  infectados (voz sintética com formantes), rugido do Hospedeiro, batimentos, respiração, sustos,
  sussurros, chuva, trovões, drone ambiente e trilha de perseguição.
- Para um resultado ainda mais realista, troque qualquer arquivo por uma gravação real **com o mesmo nome**
  e rode `setup_projeto.py` de novo.

## Ajustes finos (sem precisar programar muito)
- **Brilho/escuridão**: `World/CityBuilder.cpp`, constantes no topo (`MoonLux`, `SkyIntensity`,
  `ExposureMinEV`, `ExposureMaxEV`, `ExposureBias`). Se estiver escuro demais no seu monitor, suba `ExposureMinEV` (ex.: −2).
- **Lanterna**: `FlashlightCandelas` e `BatteryLife` em `AKaelCharacter` (editáveis no editor).
- **Dificuldade dos infectados**: `Core/RupturaTypes.cpp` (`ForDifficulty`).
- **Munição e armas**: `Combat/WeaponComponent.cpp` (`InitPlayerLoadout`) e quantidades em `Mission/MissionDirector.cpp`.
- **Câmera inicial em 1ª pessoa**: `bStartInFirstPerson` em `AKaelCharacter`.

## Mapa do código
| Pasta | Conteúdo |
|---|---|
| `Core/` | tipos/dificuldade, assets, efeitos (sangue, ácido, poeira), corpo procedural do infectado (`InfectedBody`) e do Kael |
| `AI/` | infectado (`EnemyCharacter`), IA de percepção/perseguição (`EnemyAIController`), chefe (`Abomination`) |
| `Combat/` | armas com chumbos, vida, granada/bile ácida |
| `Player/` | Kael (lanterna, medo, fôlego, faca, 1ª pessoa), controlador (cenas, Tab), HUD |
| `Mission/` | roteiro e sustos (`MissionDirector`), portas, terminal, núcleo, aparição, itens |
| `World/` | construção da fase, luzes, apagões, neblina e pós-processamento |

## Limitações honestas
- **Não foi compilado dentro da Unreal**: o ambiente onde foi escrito não tem a engine. O código passou
  por uma verificação de sintaxe com Clang usando cabeçalhos simulados da Unreal (pega erros de digitação
  e de consistência entre arquivos, mas não garante 100% a API real). Se aparecer algum erro na primeira
  build, mande o log que eu corrijo.
- Personagens e cenário continuam sendo **formas básicas** com animação procedural. A iluminação, a neblina,
  o pós-processamento e o som fazem o trabalho pesado do clima, mas para um visual realmente
  fotorrealista o próximo passo é trocar as formas por assets: **MetaHumans** ou modelos do **Fab** para
  Kael e os infectados (com Animation Blueprint), **Quixel Megascans** com **Nanite** para a cidade, e
  Lumen com ray tracing por hardware ligado (`r.Lumen.HardwareRayTracing=True`) em placas compatíveis.
- Os valores de luz são físicos aproximados; o ajuste fino depende do monitor (veja "Ajustes finos").
- Os sons são sintetizados: funcionam e criam clima, mas gravações reais soam melhor.
