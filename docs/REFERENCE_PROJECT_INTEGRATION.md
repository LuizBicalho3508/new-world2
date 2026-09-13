# Integracao de referencias externas

Data: 13/09/2026

Esta rodada compara o `new-world2` com dois projetos de referencia:

- `Hirvensaloa/rehti-mmorpg` (branch `dev`), licenciado sob MIT;
- `andrewRCr/ActionRPGProject` (branch `main`), sem arquivo de licenca publicado no repositorio na data da revisao.

## Regra de incorporacao

O objetivo nao e substituir Unreal Engine networking, assets, identidade ou regras existentes por codigo de outro jogo. A integracao foi feita no nivel de arquitetura e gameplay:

1. preservar a base UE 5.8 e a autoridade de servidor existente;
2. aproveitar conceitos compativeis com o produto;
3. nao copiar codigo de repositorio sem licenca explicita;
4. manter o projeto capaz de compilar sem assets Fab redistribuidos;
5. priorizar alteracoes que melhoram o playtest atual sem reconstruir o mundo procedural.

## O que foi aproveitado do Rehti MMORPG

O Rehti separa cliente, servidor e estado persistente e deixa o servidor responsavel pela logica autoritativa. No New World 2, essa ideia foi mantida usando o modelo nativo da Unreal: gameplay decisivo permanece no servidor (`HasAuthority`, RPCs e replicacao), enquanto apresentacao fica separada.

O codigo do Rehti nao foi transplantado para substituir o stack de rede da Unreal. Isso seria regressivo: UE ja fornece replicacao, relevancia, RPC, movimento de Character e infraestrutura de dedicated server.

Licenca da referencia: MIT, Copyright (c) 2023 Hirvensaloa. Consulte o repositorio original para o texto integral da licenca.

## O que foi incorporado do ActionRPGProject

O repositorio apresenta bons conceitos para combate de acao:

- diretor central de combate;
- pacing de ataques em grupo;
- stamina e custo por acao;
- poise/stagger;
- equipamento modular;
- inventario reutilizavel;
- inimigos com comportamentos distintos.

O New World 2 ja possuia stamina, block/parry/dodge, bag, equipamento procedural, familias de arma, bosses e archetypes. Nesta rodada foram incorporadas as lacunas mais relevantes:

### Diretor de combate server-side

`UNWCombatDirectorSubsystem` coordena permissoes curtas de ataque por alvo. O objetivo e evitar que varios mobs que alcancam o player no mesmo frame causem dano simultaneamente e criem combate injusto.

O diretor:

- vive como `UWorldSubsystem`;
- nao exige actor colocado no mapa;
- nao aplica dano;
- nao move inimigos;
- funciona apenas como scheduler de compromissos de ataque;
- limita concorrencia por alvo;
- aplica jitter deterministico para quebrar sincronizacao artificial.

### Ataque telegrafado

`ANWEnemy` nao aplica mais dano imediatamente ao entrar no alcance. O mob primeiro assume um compromisso de ataque, aguarda uma janela de wind-up e depois resolve o golpe.

O jogador pode evitar o dano saindo do alcance durante essa janela. Isso torna dodge, spacing e leitura do inimigo efetivamente relevantes.

Wind-up inicial:

- Zombie: 0,46 s;
- Ghost: 0,24 s;
- Brute: 0,38 s;
- World Boss: aproximadamente 0,34-0,49 s, conforme tier.

### Poise / quebra de postura

O stagger deixou de depender apenas de um unico hit ultrapassar uma porcentagem da vida. Cada inimigo agora possui poise acumulativo:

- dano reduz poise;
- impactos sucessivos quebram postura;
- poise volta a recuperar apos um intervalo sem sofrer dano;
- boss possui poise maior e resistencia superior, mas continua quebravel;
- stagger cancela um ataque comprometido ainda nao resolvido.

Isso permite armas rapidas, armas pesadas e grupos de habilidades terem papeis mecanicos diferentes sem depender somente de DPS bruto.

### Target stickiness

O alvo atual recebe um bonus de prioridade. Isso reduz troca de alvo excessiva quando dois jogadores ou NPCs estao a distancias muito proximas.

## O que deliberadamente nao foi copiado

`ActionRPGProject` nao tinha licenca publicada no repositorio revisado. Por isso, nenhum arquivo, funcao ou implementacao foi copiado. Os sistemas acima foram escritos de forma independente para a arquitetura do New World 2.

Tambem nao foram importados assets binarios, Blueprints, animacoes, modelos ou materiais de nenhum dos dois repositorios.

## Validacao obrigatoria

Nao existe runner com UE 5.8 no GitHub. A verificacao definitiva exige o build local no BigLinux:

```bash
cd ~/Projetos/new-world2
bash scripts/update-build-play-linux.sh
```

Validar no playtest:

1. dois ou mais inimigos nao devem acertar no mesmo frame continuamente;
2. aproximacao de melee deve apresentar pequena janela antes do dano;
3. recuar/dodge durante wind-up deve fazer o ataque errar;
4. varios hits devem quebrar poise mesmo quando nenhum hit isolado seria suficiente;
5. world boss deve continuar staggeravel, mas exigir pressao maior;
6. morte do mob deve continuar gerando loot normalmente;
7. nao deve haver regressao de World Partition, Fab assets, bag ou loadout.
