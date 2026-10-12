# RUPTURA: PROTOCOLO ZERO — Missão 01: A Cidade Morta

Projeto Unreal Engine 5 (C++). A fase inteira é gerada por código, então funciona num mapa vazio.

## Como abrir e jogar
1. Instale **UE 5.4 ou 5.5** e **Visual Studio 2022** (workload "Desenvolvimento de jogos com C++").
2. Clique com o botão direito em `Ruptura.uproject` > **Generate Visual Studio project files**.
3. Abra `Ruptura.sln`, configuração **Development Editor / Win64**, e compile (Ctrl+Shift+B).
4. Abra `Ruptura.uproject`. No editor: **Tools > Execute Python Script** > `Scripts/setup_projeto.py`
   (cria o mapa `L_CidadeMorta`, o volume de navmesh e o material PBR).
5. Clique em **Play**.

Se o script não criar o volume de navegação: Place Actors > Volumes > **Nav Mesh Bounds Volume**,
posição (8000, 0, -200), escala (95, 35, 10). Sem ele a IA ainda anda, mas em linha reta com desvio simples.

## Controles
WASD mover · Mouse olhar · Shift correr · Espaço pular · C agachar · Alt/V esquivar · Q cobertura ·
Botão direito mirar · Botão esquerdo atirar · R recarregar · 1/2 ou roda trocar arma · G granada ·
E interagir (segurar no terminal) · H ajuda · F1–F3 dificuldade · F5–F8 qualidade Low→Epic · Espaço pula a abertura.

## O que está implementado (em código)
- Kael em 3ª pessoa: câmera de ombro com colisão e tremor, correr, pular, agachar, esquiva com invulnerabilidade, cobertura (baixa/alta), mira, recuo, reação a tiros e explosões.
- Pistola, rifle de assalto, granadas; disparo hitscan com tiro na cabeça, munição, recarga, dispersão, traçantes, clarões, impactos.
- IA com AI Perception (visão + audição): patrulha, suspeita, investigação, combate com cobertura e rajadas, flanco, busca, alerta em grupo, fuga de granadas; 3 dificuldades + soldados de elite.
- Chefe COLOSSO: metralhadora, salvas de foguetes, pisão, fase de fúria a 50%, ponto fraco nas costas.
- Missão com os 9 objetivos, portas, cartão, terminal, núcleo com escudo, fuga cronometrada, reforços, checkpoints, telas de vitória/derrota com estatísticas.
- HUD em português, marcador de objetivo, cena de abertura e apresentação do chefe.
- Cidade destruída, instalação, túnel, ala subterrânea e reator; Lumen, neblina volumétrica, sombras virtuais, chuva, relâmpagos, luzes de emergência piscando.

## Limitações honestas
- **Não foi compilado nem testado**: o ambiente onde foi escrito não tinha Unreal Engine. Espere alguns erros de compilação pequenos na primeira build — me mande o log que eu corrijo.
- Personagens e cenário são **placeholders 3D** (formas básicas) com animação procedural. Próximo passo: trocar por Manny/Quinn + Animation Blueprint e assets do Fab/Quixel com Nanite.
- Partículas são feitas com geometria + luz (não Niagara).
- **Sem áudio incluído**: o código toca sons de `/Game/Audio/<nome>.wav` se existirem (nomes em `Source/Ruptura`, ex.: `tiro_rifle`, `explosao`, `chuva_loop`). Coloque .wav em `AudioFonte/` e rode o script. Sem eles, o jogo roda em silêncio.
