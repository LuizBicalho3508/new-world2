# New World 2

Codename de um action RPG 3D procedural em terceira pessoa, PvPvE, construido em Unreal Engine 5.8.

> `New World 2` e um codename de desenvolvimento. O projeto nao reutiliza codigo, historia, personagens, marcas ou identidade de New World, Throne and Liberty ou qualquer outro jogo. Referencias servem apenas para direcao de genero/gameplay. Antes de publicacao comercial, o produto deve receber nome e identidade proprios.

## Estado atual do projeto - 13/09/2026

A base principal esta em **BigLinux / Linux nativo**, usando Unreal Engine **5.8.2**, Vulkan e build C++ nativo.

Hardware atual usado como referencia de performance:

- CPU: Intel Core i7-2600S, 4c/8t;
- GPU: NVIDIA GeForce RTX 5060, 8 GB VRAM;
- RAM: 16 GB;
- sistema: BigLinux;
- renderer de teste: Vulkan + SM6;
- alvo atual de playtest: 45 FPS;
- build local limitado por padrao a 3 acoes paralelas para evitar swap excessivo.

A `main` ja contem a passagem de estabilizacao do PR #18. Essa rodada foi criada para transformar o prototipo em um playtest previsivel e inclui:

- normalizacao dos inputs em runtime;
- `Q/E/R` reservados para as 3 habilidades;
- `RMB` para block/parry;
- `Shift` para sprint;
- `1/2` para armas e `F` para quick swap;
- `I` para bag;
- regeneracao destrutiva do mundo isolada em `F10`;
- desativacao explicita de timers legados de regeneracao automatica;
- reforco do spawn inicial e da colisao do terreno procedural;
- piso/proxy de seguranca alinhado a superficie para impedir quedas infinitas;
- recuperacao emergencial do player somente quando atravessa o terreno;
- iluminacao de seguranca aplicada no final do frame;
- Asset Registry sincronizado antes do play para detectar os assets Fab locais;
- launcher de playtest direto no game;
- diagnostico automatico do log.

### Ultimo estado observado antes do PR #18

Antes dessa estabilizacao, o jogo ja:

- compilava e abria no Linux com UE 5.8.2;
- carregava o personagem Paragon Greystone em terceira pessoa;
- aceitava WASD e algumas acoes de combate;
- executava habilidades e trocas de loadout no codigo;
- carregava clima, biomas, invasoes e geracao procedural;
- mantinha aproximadamente 45 FPS no primeiro profile;
- apresentava problemas graves de colisao, quedas atraves do terreno, reloads de epoch e conflito de input local.

O principal bug identificado foi um mapping local antigo onde `R` podia acionar habilidade e regeneracao do mundo ao mesmo tempo. Isso recriava terreno, NPCs, colisao e apresentacao durante o combate, causando flicker, queda no vazio e falsa impressao de que varios inputs falhavam simultaneamente.

**Importante:** a versao estabilizada do PR #18 ainda precisa ser recompilada e validada localmente na UE 5.8.2 depois da limpeza do conflito Git local em `Config/DefaultInput.ini`. Nao considerar essa rodada como validada ate concluir esse playtest.

## Ambiente principal de desenvolvimento

- Unreal Engine 5.8.2 Linux;
- Vulkan + SM6;
- build C++ nativo com `Engine/Build/BatchFiles/Linux/Build.sh`;
- toolchain nativo da Unreal via `SetupToolchain.sh` quando necessario;
- Fab para Linux como plugin opcional;
- scripts Bash para instalacao, build, World Partition, assets, playtest e diagnostico;
- scripts PowerShell mantidos somente como legado para Windows.

Guia de bootstrap: `docs/BIGLINUX_FIRST_TEST.md`.

### Primeira instalacao no BigLinux

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/LuizBicalho3508/new-world2/main/scripts/first-test-biglinux.sh)
```

O bootstrap instala dependencias do BigLinux, clona/atualiza o projeto, testa Vulkan, procura a UE 5.8, prepara toolchain, instala o plugin Fab quando o ZIP estiver disponivel, valida assets, compila e inicia o fluxo de teste.

A instalacao atual usada no desenvolvimento esta em:

```text
~/Aplicativos/UnrealEngine-5.8
```

O ZIP ja usado com sucesso foi Unreal Engine Linux 5.8.2. O plugin Fab Linux 5.8 tambem ja foi instalado localmente.

## Playtest atual recomendado

Depois de atualizar a `main`, o fluxo recomendado e:

```bash
cd ~/Projetos/new-world2

bash scripts/play-biglinux.sh \
  --ue-root "$HOME/Aplicativos/UnrealEngine-5.8" \
  --max-parallel 3
```

Esse launcher:

- faz build incremental do `NewWorld2Editor`;
- evita que overrides antigos de input contaminem o teste;
- abre **direto o game**, nao o Editor;
- usa Vulkan + SM6;
- usa 1920x1080 e limite de 45 FPS por padrao;
- grava o log de runtime em `~/nw2-playable.log`.

Para profile:

```bash
bash scripts/play-biglinux.sh \
  --ue-root "$HOME/Aplicativos/UnrealEngine-5.8" \
  --max-parallel 3 \
  --profile
```

Depois do jogo fechar:

```bash
cd ~/Projetos/new-world2
bash scripts/check-playable-log.sh
```

Esse diagnostico resume habilidades, loadout, bag/HUD, epochs, recuperacoes de terreno, catalogos Fab e erros relevantes.

## Recuperacao do checkout local antes do proximo teste

Na ultima sessao local, `Config/DefaultInput.ini` ficou em estado de conflito Git (`UU`). Os assets Fab locais nao devem ser apagados.

Antes de continuar em uma maquina que ainda esteja nesse estado:

```bash
cd ~/Projetos/new-world2 || exit 1

BACKUP="$HOME/nw2-config-conflito-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$BACKUP"
cp -a Config "$BACKUP/" 2>/dev/null || true

git merge --abort 2>/dev/null || true
git rebase --abort 2>/dev/null || true
git cherry-pick --abort 2>/dev/null || true
git am --abort 2>/dev/null || true

git reset --hard HEAD
git fetch origin main
git switch main
git reset --hard origin/main

git config core.fileMode false

git log -1 --oneline
find Content -type f -name '*.uasset' | wc -l
```

Nao usar `git clean -fdx`, pois o conteudo Fab instalado localmente fica em `Content/` e nao deve ser removido.

## Fab / conteudo externo local

O repositorio **nao redistribui** os assets Fab/Epic. Eles permanecem apenas na maquina local.

Ultimo inventario confirmado na maquina de desenvolvimento:

- **6.040 `.uasset`** locais;
- **20/33 grupos** detectados pelo verificador;
- todos os 6.040 arquivos testados tinham cabecalho Unreal valido;
- downloads parciais do Fab chegaram a gerar warnings temporarios de `PACKAGE_FILE_TAG`, mas apos o download terminar a validacao retornou 0 arquivos invalidos.

Principais diretorios presentes em `Content/`:

```text
AnimStarterPack
ArrowTrail
Atris_swords
DeformableSnowSystem
Demonslayer_Akaza
EuropeanBeech
Fab
FantasyRuins
Free_Magic
GeneratedWorld
KiteDemo
MSPresets
NiagaraExamples
ParagonGreystone
ParagonMinions
RealisticSwordSoundEffects
_SplineVFX
Swords_Pack_Project
TA_Sample_Statue
TorchFire
```

Contagem aproximada por packs principais na ultima verificacao:

```text
2105  ParagonMinions
1733  ParagonGreystone
 667  NiagaraExamples
 270  KiteDemo
 257  EuropeanBeech
 191  Free_Magic
 177  DeformableSnowSystem
 112  _SplineVFX
  69  AnimStarterPack
  61  Atris_swords
  60  RealisticSwordSoundEffects
  50  ArrowTrail
  31  FantasyRuins
  26  Swords_Pack_Project
  18  TA_Sample_Statue
  17  TorchFire
  17  Demonslayer_Akaza
```

Grupos ja detectados incluem, entre outros:

- Paragon Greystone;
- Paragon Sparrow;
- Paragon Rampage;
- Paragon Revenant;
- Paragon Terra;
- Paragon Minions;
- Orc Warrior Axe and Shield;
- Medieval King;
- Free Sword Pack / Realistic Melee;
- Medieval Dagger / Daggers;
- Atris Swords;
- Medieval Iron/Wood Shield;
- Realistic Viking Shield;
- Modular/Realistic Armor;
- Free Arrow Trail;
- Free Torch Fire;
- Fantasy Desert Ruins;
- Dark Fantasy Statue/Pedestal;
- Atmospheric Worlds Music;
- Procedural Sea Waves.

Validar o conteudo local:

```bash
cd ~/Projetos/new-world2
bash scripts/verify-fab-assets-linux.sh
```

Abrir o Editor somente para instalar/gerenciar packs Fab:

```bash
cd ~/Projetos/new-world2
bash scripts/open-editor-linux.sh
```

## Direcao do projeto

- sem level tradicional de personagem;
- progressao horizontal por equipamento, afixos, sinergias, passivas e dominio mecanico;
- duas armas equipadas simultaneamente;
- sete familias de arma;
- tres habilidades ativas por arma;
- passivas das duas armas equipadas ativas simultaneamente;
- 21 estados independentes de cooldown;
- combo por troca de loadout;
- arco com flechas fisica, fogo, veneno, eletrica e gelo;
- PvE/PvP com autoridade de servidor;
- loot procedural de armas, armaduras e consumiveis;
- bag sem limite logico e organizacao automatica;
- armaduras leves, medias e pesadas;
- block, parry, dodge, stamina, stagger e hit reactions;
- zumbis, fantasmas, invasoes, dungeons e world bosses;
- world bosses planejados para solo com build lendaria forte;
- Pocao Lendaria de Metamorfose da Armadura Brutal;
- fast travel validado pelo servidor;
- seis biomas, clima dinamico e ciclo dia/noite;
- VFX Niagara, audio e meshes externos descobertos via Asset Registry;
- World Partition + PCG runtime;
- direcao visual realista com assets gratuitos/licenciados locais;
- fallbacks para o repositorio funcionar sem redistribuir conteudo de terceiros.

## Armas e combate

Familias implementadas:

1. Cajado;
2. Espada Grande;
3. Duas Espadas;
4. Espada e Escudo;
5. Adagas;
6. Arco;
7. Arma de Fogo.

Cada arma possui ataque basico, tres habilidades e tres passivas. Cooldowns pertencem a cada familia.

Com arco, `V` alterna entre flecha Fisica, Fogo, Veneno, Eletrica e Gelo. O projectile visual e separado da autoridade de dano.

Os ultimos logs antes da estabilizacao ja mostravam execucao de habilidades para Espada Grande e Cajado Arcano, cura, loadout e VFX. O problema era a simultaneidade de regeneracao do mundo e mappings locais inconsistentes, nao ausencia completa da logica de habilidade.

## Controles canonicos atuais

| Controle | Acao |
|---|---|
| WASD | mover |
| Mouse | camera |
| Espaco | pular |
| Shift | correr |
| Mouse esquerdo | ataque basico |
| Mouse direito | bloquear / janela de parry |
| Alt esquerdo | dodge |
| Q | habilidade 1 |
| E | habilidade 2 |
| R | habilidade 3 |
| 1 | arma primaria |
| 2 | arma secundaria |
| F | troca rapida / combo cross-weapon |
| Z / X | trocar familia das armas em debug |
| V | alternar elemento da flecha |
| G | coletar loot |
| I | abrir/fechar bag |
| Seta cima/baixo | selecionar item da bag |
| Enter | equipar/usar item |
| T | selecionar fast travel |
| Y | confirmar fast travel |
| F10 | regenerar epoch/mundo manualmente - debug destrutivo |

`R` **nao** deve regenerar o mundo.

## Loot / bag

Fluxo:

```text
mob morre -> pickup -> G coleta -> I abre bag -> setas selecionam -> Enter equipa/usa
```

A bag e rolavel, sem limite logico de slots no prototipo, organizada por armas, armaduras e consumiveis.

## World bosses e Armadura Brutal

O mundo mantem tres bosses simultaneos. Cada world boss derruba lendarios e a Pocao da Armadura Brutal.

A metamorfose dura 300 segundos e atualmente fornece:

- +120 vida maxima;
- +35 stamina maxima;
- +28% dano;
- +15% cura;
- +65 armadura;
- +5% roubo de vida;
- +12 guarda.

## Mundo / streaming / colisao

O gerador C++ cria terreno, foliage, recursos, assentamentos, civis, mobs e invasoes. `ANWProceduralWorldManager` possui PCG runtime/particionado e seed por epoch.

O mapa principal e:

```text
/Game/GeneratedWorld/NW2_OpenWorld
```

No Linux, `scripts/prepare-worldpartition-linux.sh` usa `WorldPartitionConvertCommandlet` para preparar o mapa.

A evolucao automatica do mundo esta **desligada no playtest atual**. A regeneracao fica manual em `F10`, para nao reconstruir o terreno sob o jogador enquanto locomocao e combate sao estabilizados.

A ultima rodada adicionou um proxy de colisao de seguranca mais denso e alinhado a superficie, alem de recuperacao emergencial quando o Character atravessa o terreno. Esse ponto precisa ser validado no proximo playtest local.

## Render / performance atual

Perfil atual para CPU antiga + GPU moderna:

- Software Lumen;
- Hardware RT desativado por enquanto;
- Nanite habilitado no projeto;
- Virtual Shadow Maps;
- TSR;
- Vulkan SM6;
- alvo padrao 45 FPS;
- view distance, grass e foliage reduzidos para aliviar CPU;
- efeitos, texturas e pos-processamento mantidos mais altos para usar a RTX 5060;
- AI normal reduzida e mobs distantes usam intervalo maior de pensamento;
- build com `MaxParallelActions=3` por padrao na maquina de 16 GB RAM.

No primeiro profile jogavel antes dos assets maiores, foi observado aproximadamente:

```text
44.75 FPS
Frame ~22.35 ms
Game  ~9.82 ms
Draw  ~12.03 ms
GPU   ~4.76 ms
```

Isso indicou que o limite de 45 FPS estava ativo e a RTX 5060 ainda tinha margem.

## Problemas conhecidos / proxima prioridade

O proximo chat deve continuar a partir daqui:

1. limpar o conflito Git local em `Config/DefaultInput.ini` sem apagar os 6.040 assets Fab;
2. atualizar o checkout local para a `main` atual;
3. compilar a passagem de estabilizacao do PR #18 na UE 5.8.2 Linux;
4. executar `scripts/play-biglinux.sh`;
5. validar que nao existem epochs automaticos e que `R` executa apenas habilidade 3;
6. validar `RMB`, `Shift`, `Space`, `1/2/F`, `I`, Q/E/R;
7. validar que o personagem nao atravessa mais o terreno;
8. executar `scripts/check-playable-log.sh`;
9. somente depois continuar a integracao visual dos packs locais;
10. integrar melhor `EuropeanBeech`, `KiteDemo`, `FantasyRuins`, `ParagonMinions`, `Atris_swords`, `ArrowTrail`, `Free_Magic` e `NiagaraExamples` ao gerador procedural/apresentacao;
11. corrigir caminhos fixos de personagens quando o pack estiver instalado em caminho diferente;
12. remover warnings antigos de XR/VR vindos de Blueprints legados do Paragon quando necessario;
13. limpar placeholders antigos que ainda tentam `/Engine/BasicShapes/Capsule.Capsule`;
14. depois da base estavel, continuar baixando os grupos Fab restantes.

## Scripts principais

### Linux / BigLinux

- `scripts/first-test-biglinux.sh` - bootstrap completo;
- `scripts/play-biglinux.sh` - **launcher principal de playtest atual**;
- `scripts/check-playable-log.sh` - diagnostico do playtest;
- `scripts/clone-build-run-linux.sh` - build/test tecnico geral;
- `scripts/prepare-worldpartition-linux.sh` - World Partition;
- `scripts/verify-fab-assets-linux.sh` - inventario Fab local;
- `scripts/install-fab-plugin-linux.sh` - instala plugin Fab Linux;
- `scripts/open-editor-linux.sh` - abre Editor para gerenciamento Fab.

### Windows - legado

- `scripts/first-test-setup.ps1`;
- `scripts/clone-build-run.ps1`;
- `scripts/prepare-worldpartition.ps1`;
- `scripts/verify-fab-assets.ps1`.

## Documentacao

- `docs/BIGLINUX_FIRST_TEST.md`;
- `docs/PERFORMANCE_I7_2600S_RTX5060.md`;
- `docs/REALISTIC_ASSETS.md`;
- `docs/CONTENT_EXPANSION.md`;
- `docs/COMBAT_INVENTORY.md`;
- `docs/ARSENAL_PASSIVES_BOSSES_TRAVEL.md`;
- `docs/WORLD_PARTITION_PCG.md`;
- `docs/FAB_EXPANSION_PACKS.md`.

## Validacao

Nao existe runner GitHub com Unreal Engine 5.8 configurado. A validacao definitiva e feita localmente no BigLinux com o UnrealBuildTool e a UE 5.8.2 instalada.

Nao afirmar que uma alteracao C++ foi validada ate o build local completar com sucesso e o playtest correspondente ser executado.

## Licenca

O codigo proprio segue `LICENSE`. Conteudo de terceiros mantem suas proprias licencas e nao deve ser republicado isoladamente no repositorio.
