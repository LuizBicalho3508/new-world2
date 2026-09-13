# Premium V9 - Hybrid World / Visual Recovery

## Objetivo

A V9 troca a filosofia do vertical slice: o macro do mapa passa a ser fixo/deterministico e a proceduralidade fica restrita a sistemas que realmente ganham com variacao (eventos, invasoes, populacao e microdetalhe barato).

## Mudancas aplicadas

- Terrain macro usa seed fixa e nao muda a cada epoch.
- Relevo tem frequencias menores e amplitude reduzida para leitura/movimento mais uniforme.
- Spawn, dois nucleos de assentamento e tres corredores de viagem recebem flatten/feather autorado.
- UV do ProceduralMesh virou world-space tiling em vez de uma unica textura esticada pelo mapa inteiro.
- Vegetacao premium respeita trilhas e zonas de assentamento.
- HISM pesado foi reduzido; grama/arbusto nao projetam sombra dinamica.
- PremiumV6EnvironmentBooster deixou de ser criado pelo GameMode, removendo uma segunda camada de 2.980 micro-instancias.
- EuropeanBeech/EuropeanHornbeam/Megascans/Quixel recebem prioridade; KiteDemo vira fallback legado.
- Materiais dos meshes sao preparados para MATUSAGE_InstancedStaticMeshes antes de SetStaticMesh/AddInstance.
- Script de reparo persistente resalva meshes/materiais locais para reduzir warnings e recompilacao a cada boot.
- Dungeon exige correspondencia com vocabulario do tema antes de pontuar Fab/Medieval/PBR; arma/escudo/adaga/staff nao podem mais virar parede/caverna.
- Color grade global moderado melhora separacao de cor sem ativar Lumen/RT.
- HUD usa monogramas ASCII coloridos no lugar de glyphs ausentes no Roboto Linux e oculta o HUD de combate ao abrir o inventario.
- Preparacao Manny/Quinn foi reforcada para Feature Packs cujo .upack possui prefixos antes de Content/.

## O que continua dependente de asset local

### Corpo/armadura

O codigo nao forca uma armadura de skeleton incompatível. Para peito/pernas/luvas/botas realmente vestirem, o pack instalado deve compartilhar a mesma familia de bones do corpo-base (Manny/UEFN ou outro corpo modular definitivo). Isso evita novamente pecas flutuando ou atravessando o personagem.

### Cajado

O gameplay do Staff continua funcional, mas a V9 nao cria uma arma premium falsa com primitive. Se nenhum StaticMesh de staff/scepter/wand/quarterstaff/polearm existir fisicamente em /Game, o log registra a ausencia e o slot visual permanece sem mesh. Adicionar um pack de cajado real e a solucao correta.

### Greystone antigo

O pack Paragon Greystone local ainda contem Blueprints antigos com referencias VR removidas na UE atual. Com Manny/UEFN completo, o objetivo e deixar Greystone apenas como fallback temporario e migrar/retargetar o combate para o skeleton definitivo.

## Performance

A V9 reduz trabalho redundante de ambiente, mas os PSO hitches reportados pelo editor Development podem continuar enquanto o cache de pipeline nao for produzido/treinado para um build empacotado. O teste V9 continua contando esses hitches separadamente para nao confundir gargalo de editor com custo normal de gameplay.

## Teste

Use:

```bash
bash scripts/premium-v9-test-biglinux.sh --fps 60 --resolution 1600x900 --max-parallel 3
```

Por padrao nao existe overlay `stat` na tela. `--profile` e opt-in.
