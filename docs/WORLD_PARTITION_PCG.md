# World Partition + PCG por celulas

## Objetivo

O mundo final nao deve existir como um unico mapa carregado integralmente. A arquitetura atual prepara duas camadas complementares:

- **World Partition** para streaming espacial do mapa/atores;
- **PCG particionado em runtime** para conteudo procedural gerado proximo das areas relevantes.

## World Partition local

O arquivo `.umap` e os External Actors sao binarios. Por isso o repositorio nao fabrica nem versiona um mapa convertido manualmente.

O script:

```powershell
.\scripts\prepare-worldpartition.ps1
```

faz o seguinte localmente:

1. localiza Unreal Engine 5.8;
2. copia um mapa-base interno para `Content/GeneratedWorld/NW2_OpenWorld.umap`;
3. cria configuracao de conversao;
4. executa `WorldPartitionConvertCommandlet`;
5. usa Spatial Hash para editor/runtime;
6. configura cell size inicial de `25600 cm` (256 m);
7. grava marcador em `Saved/` para nao repetir o trabalho a cada execucao.

Para recriar:

```powershell
.\scripts\prepare-worldpartition.ps1 -ForceRecreate
```

Os arquivos gerados ficam ignorados pelo Git.

## Streaming source

No gameplay normal, o PlayerController funciona como streaming source para o World Partition. Com multiplayer dedicado, cada jogador conectado passa a contribuir para as areas que precisam permanecer carregadas.

Na fase de escala, outros streaming sources poderao ser criados para:

- invasoes em andamento;
- cidades persistentes;
- chefes/eventos mundiais;
- caravanas ou objetos de interesse que precisem sobreviver sem um jogador imediatamente proximo.

## PCG runtime particionado

`ANWProceduralWorldManager` cria um `UPCGComponent` com:

```text
GenerationTrigger = GenerateAtRuntime
IsPartitioned = true
Seed = seed do WorldEpoch
```

A propriedade `RuntimePCGGraph` e um `TSoftObjectPtr<UPCGGraphInterface>`.

Se um grafo PCG local for atribuido:

- o componente registra o grafo;
- executa geracao local;
- usa a seed do epoch;
- o particionamento distribui a geracao pelas celulas PCG.

Se nenhum grafo existir, o gerador C++ de terreno/vegetacao/assentamentos continua funcionando. Isso preserva um build testavel antes de o primeiro grafo binario ser autorado no Editor.

## Grafo PCG alvo

Quando o primeiro grafo for autorado no Editor, ele deve ser dividido em responsabilidades:

```text
Surface / Terrain
  -> biome mask
  -> slope/height filters

Vegetation
  -> trees
  -> bushes
  -> ground cover

Resources
  -> rocks
  -> ores/crystals

POI
  -> ruins
  -> camps
  -> settlement outskirts

Population
  -> spawn descriptors/events
```

O grafo deve usar hierarchical generation/grid sizes diferentes quando a densidade justificar. Objetos distantes e caros nao devem usar a mesma granularidade de ground cover.

## Relacao com WorldEpoch

O epoch continua sendo a autoridade logica. Ao mudar:

1. atores procedurais da geracao anterior sao limpos;
2. `WorldEpoch` incrementa;
3. a seed do PCG e atualizada;
4. o PCG regenera quando houver grafo;
5. o gerador C++ fallback regenera seu conteudo;
6. jogadores sao reposicionados para uma altura segura do novo terreno.

No produto final, mudancas macro de terreno terao cadencia muito menor que recursos/eventos. O prototipo mantem 180 s apenas para teste acelerado.

## HLOD

World Partition nao resolve sozinho custo de desenho de cidades/vegetacao. Depois do primeiro teste integrado, a etapa de performance deve adicionar HLOD por categoria, principalmente para:

- assentamentos;
- estruturas repetidas;
- conjuntos de foliage distantes;
- POIs grandes.

## Bootstrap

`clone-build-run.ps1` compila o C++ antes de tentar preparar o mapa. Depois chama `prepare-worldpartition.ps1` em outro processo.

Se a conversao funcionar, abre:

```text
/Game/GeneratedWorld/NW2_OpenWorld
```

Se falhar, abre o mapa fallback e permite validar HUD, combate, inventario, loot, procedural e invasoes mesmo assim.
