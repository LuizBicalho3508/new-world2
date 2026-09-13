# Premium V7 — revisão técnica e baseline de produção

## Objetivo desta revisão

Esta revisão fecha os problemas que estavam degradando o primeiro vertical slice jogável sem transformar o projeto em uma reescrita de alto risco. O foco é deixar o build atual testável com apresentação mais coerente, reduzir hitch de primeira utilização e fazer o código aproveitar automaticamente os packs Fab/Epic que estiverem realmente instalados em `Content/`.

## Evidências do playtest anterior

O log anterior mostrou quatro problemas prioritários:

1. Todos os arquétipos de inimigo acabavam no mesmo `ParagonGreystone`.
2. Itens de armadura eram equipados pelo gameplay, mas não tinham representação visual por incompatibilidade de skeleton.
3. Niagara continuava compilando sistemas durante o combate e os PSOs eram criados na primeira utilização.
4. O playtest estava limitado a 45 FPS mesmo quando o restante da configuração permitia 60 FPS.

## Causas encontradas

### Inimigos

`NWEnemyVisualDirector` exigia encontrar um `AnimBlueprint` compatível para aceitar uma mesh e, depois de aceitar, removia o `AnimBlueprint` e colocava a mesh em `AnimationSingleNode`. Isso eliminava candidatos válidos e empurrava o catálogo para um fallback conhecido. O mesmo código bloqueava `ParagonMinions`, justamente um dos melhores packs gratuitos para diversidade de IA.

A V7 deixa a seleção de mesh e a animação com responsabilidades separadas: o visual seleciona uma skeletal mesh segura; `NWEnemyAnimationDirector` escolhe `AnimSequence` pelo skeleton da mesh.

### Equipamento do jogador

O caminho antigo só aceitava armadura skeletal quando `ArmorMesh->GetSkeleton() == PlayerSkeleton`. Isso é correto para `Leader Pose`, mas significa que packs de skeleton diferente nunca podem aparecer no Greystone.

A V7 mantém `Leader Pose` como primeira opção e completa o fallback que já estava previsto no header: peças `StaticMesh` realistas podem ser presas aos bones/sockets do personagem quando não existe peça skeletal compatível. Isso permite visualizar equipamento sem tentar fazer retarget inseguro em runtime.

### Hitch de shaders/VFX

O loading gate antigo olhava apenas `FShaderPipelineCache::NumPrecompilesRemaining()`. Os sistemas Niagara escolhidos pelo diretor de VFX ainda podiam ter compilação pendente após o jogo ser liberado. A V7 seleciona um conjunto pequeno de Niagara de combate, força a solicitação de compilação e mantém o gate enquanto PSO/Niagara prioritários ainda estiverem pendentes, com timeout de segurança.

### Frame pacing

O `DefaultEngine.ini` e o launcher agora usam 60 FPS como alvo do vertical slice. O limite continua configurável pelo script de teste.

## Comparação arquitetural usada

### Lyra

Lyra separa inventário de equipamento. O item pode existir apenas como dado no inventário; quando equipado, nasce a representação usada/vestida pelo Pawn e o equipamento pode conceder habilidades. A V7 segue a mesma separação conceitual: os dados do loot/equip continuam independentes da camada de apresentação visual.

### Gameplay Ability System

O GAS é a direção recomendada para um RPG online de longo prazo porque centraliza abilities, attributes, gameplay effects, cooldowns, custos, buffs/debuffs e predição/replicação. O projeto atual ainda usa o sistema C++ próprio e a V7 não migra tudo para GAS durante um recovery pass, pois isso aumentaria muito o risco de quebrar o vertical slice. A migração para GAS deve ser uma etapa arquitetural posterior, com testes próprios.

### Game Animation Sample

O Game Animation Sample demonstra Motion Matching e um sistema de locomação responsivo/extensível para personagens humanos. Ele é a referência recomendada para a próxima troca do Greystone como corpo-base do jogador. O V7 não troca o skeleton-base automaticamente porque os montages/animations atuais ainda dependem do Greystone e os assets do Game Animation Sample são conteúdo local/binarizado.

### Modular Characters

A documentação da Epic confirma que `Leader Pose` requer estrutura de bones correspondente. Por isso a V7 não tenta conectar uma armadura skeletal arbitrária a um skeleton diferente. Para o produto final, a melhor solução é padronizar um skeleton humano e importar/retargetar todas as peças vestíveis para esse skeleton; para muitos componentes visuais, `Skeletal Mesh Merge` também deve ser avaliado para reduzir custo de render.

## Packs Fab/Epic prioritários para o próximo playtest

### Variedade de inimigos

Prioridade alta: `Paragon: Minions`, `Paragon: Grux`, `Paragon: Khaimera`, `Paragon: Rampage`, `Paragon: Sevarog`, `Paragon: Revenant`, `Paragon: Countess`.

A V7 procura esses nomes/arquétipos automaticamente. Eles precisam existir fisicamente em `Content/`; estar apenas na Library da Fab não é suficiente.

### Jogador e equipamentos

Prioridade alta: armas realistas já presentes na Library (longswords, sword packs, daggers, Ethereal Recurve Bow, shields) e um conjunto de armadura que use um skeleton padronizado. O pack `Lowpoly Modular Armors` é útil como prova funcional, mas não deve definir a direção de arte final se o objetivo é visual realista.

O Game Animation Sample deve ser instalado e avaliado como base de locomação humana antes da migração definitiva do player skeleton.

### Ambiente

O diretor atual já prioriza `Megascans`, `Quixel`, `Fab`, `Realistic`, `PBR` e `Nanite`; quando não encontra, ele recorre ao conteúdo antigo. Portanto European Beech/Hornbeam, ambientes medievais, ruínas, cave/dungeon e materiais Megascans devem ser instalados antes do próximo passe visual de cenário.

## O que a V7 deliberadamente não faz

- Não redistribui `.uasset` da Fab/Epic pelo GitHub.
- Não faz retarget automático destrutivo de skeleton em runtime.
- Não troca a IA atual por NavMesh sem antes validar um NavMesh gerado para o mundo procedural.
- Não migra o combate inteiro para GAS nesta branch de recovery.
- Não declara o build aprovado até `premium-v7-test-biglinux.sh` compilar contra a instalação local UE 5.8 e os assets locais.

## Critérios do playtest V7

O script `scripts/premium-v7-test-biglinux.sh` deve:

1. validar os contratos V7;
2. auditar os packs instalados em `Content/`;
3. compilar `NewWorld2Editor` na UE 5.8;
4. abrir o vertical slice a 60 FPS por padrão;
5. registrar V7 enemy meshes, animações, gear e HUD;
6. resumir quantas meshes diferentes de inimigos foram usadas;
7. sinalizar qualquer inimigo que ainda tenha caído em Greystone;
8. sinalizar erros Blueprint/divide-by-zero/fatal/GPU crash;
9. registrar build, runtime, GPU e inventário de assets em arquivos separados.

## Próximas etapas depois do primeiro teste V7

Depois que o V7 compilar e o log real estiver disponível, as duas maiores evoluções estruturais são:

1. substituir o steering manual dos mobs por uma solução de navegação validada para o mundo procedural (NavMesh/AIController quando o mapa suportar isso);
2. migrar o player para um skeleton humano padronizado com Game Animation Sample/Motion Matching e um pipeline de armadura modular realmente autorado para esse skeleton.

Essas duas mudanças são maiores que um recovery patch e devem ser feitas em branches próprias depois que o V7 estabelecer um baseline estável.
