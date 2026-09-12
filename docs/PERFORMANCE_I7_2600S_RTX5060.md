# Perfil de performance: i7-2600S + RTX 5060 + 16 GB

Este projeto agora usa um perfil de desenvolvimento orientado a CPU antiga e GPU moderna.

## Principios

- limitar o Game Thread em vez de reduzir toda a qualidade visual;
- manter 45 FPS como alvo inicial para reduzir frequencia de simulacao/ticks;
- usar Vulkan + SM6 no BigLinux;
- usar Lumen sem Hardware Ray Tracing;
- permitir Nanite, Virtual Shadow Maps e TSR para deslocar mais trabalho de renderizacao para a GPU;
- manter texturas, efeitos e pos-processamento altos;
- reduzir distancia/grama/foliage pequeno, que aumentam numero de objetos e trabalho do CPU;
- manter arvores/rochas do gerador como HISM/instancing;
- usar AI LOD: inimigos distantes entram em sleep e deixam de procurar alvos/mover continuamente;
- cachear selecao de alvo;
- reduzir frequencia de replicacao de mobs e civis;
- reduzir quantidade de civis, invasores e mobs de dungeon, preservando os 3 world bosses;
- limitar compilacao Unreal a 3 acoes paralelas por padrao para evitar swap em 16 GB.

## Rodar normalmente

```bash
cd ~/Projetos/new-world2
bash scripts/clone-build-run-linux.sh
```

Padrao: 1920x1080, 45 FPS, Vulkan SM6.

## Perfil CPU x GPU

```bash
cd ~/Projetos/new-world2
bash scripts/clone-build-run-linux.sh --profile
```

Na tela observe `stat unit`, `stat game`, `stat gpu` e `stat fps`.

Interpretacao simples:

- `Game` maior que `GPU`: gargalo ainda esta no processador/Game Thread;
- `GPU` maior que `Game`: RTX 5060 virou o limite, que e o comportamento desejado para esta maquina;
- `Draw` alto: Render Thread/numero de draw calls precisa de mais HLOD/Nanite/instancing;
- picos ao entrar em regioes: revisar streaming/PCG/Asset Registry.

## Alterar FPS

```bash
bash scripts/clone-build-run-linux.sh --fps 30
bash scripts/clone-build-run-linux.sh --fps 45
bash scripts/clone-build-run-linux.sh --fps 60
```

45 FPS e o ponto inicial recomendado. Depois do primeiro profile, 60 FPS so deve virar padrao se `Game` permanecer confortavelmente abaixo de aproximadamente 16 ms em areas com combate.

## Compilacao

O padrao e `-MaxParallelActions=3`. Para manter o desktop ainda mais leve:

```bash
bash scripts/clone-build-run-linux.sh --max-parallel 2
```

Para tentar compilar mais rapido, com risco maior de swap/engasgos:

```bash
bash scripts/clone-build-run-linux.sh --max-parallel 4
```

## Observacao sobre GPU

Nem todo processamento pode ser enviado para a RTX. IA, regras de combate, replicacao, parte da fisica e Game Thread continuam sendo CPU. O objetivo deste perfil e evitar trabalho desnecessario e fazer o frame ficar limitado por renderizacao/GPU sempre que possivel, sem comprometer a resposta do combate.
