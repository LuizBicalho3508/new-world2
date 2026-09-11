# New World 2

Codename de um action RPG 3D procedural em terceira pessoa, PvPvE, construido em Unreal Engine 5.8.

> `New World 2` e um codename de desenvolvimento. O projeto nao reutiliza codigo, historia, personagens, marcas ou identidade de New World, Throne and Liberty ou qualquer outro jogo. Referencias servem apenas para direcao de genero/gameplay. Antes de publicacao comercial, o produto deve receber nome e identidade proprios.

## Direcao do projeto

- sem level tradicional de personagem;
- progressao horizontal por equipamento, afixos, sinergias, passivas e dominio mecanico;
- duas armas equipadas simultaneamente;
- sete familias de arma, cada uma com ataque basico, tres habilidades ativas e tres passivas;
- as tres passivas das duas armas equipadas ficam ativas simultaneamente;
- cooldowns independentes por arma: 7 familias x 3 habilidades = 21 estados;
- combo entre armas por troca de loadout;
- arco com aljava elemental: fisica, fogo, veneno, eletrica e gelo;
- PvE e PvP com servidor autoritativo;
- loot procedural de armaduras, armas e consumiveis;
- bag sem limite logico e organizacao automatica por categoria/familia;
- armaduras leves, medias e pesadas com identidades mecanicas diferentes;
- HUD, stamina, block, parry, dodge, stagger e hit reactions;
- cidades, civis, criaturas, zumbis, fantasmas, invasoes e world bosses;
- bosses mundiais balanceados para serem soloaveis com build lendaria forte;
- castelos sombrios e cavernas procedurais com guardioes e drops lendarios;
- Pocao Lendaria de Metamorfose da Armadura Brutal em bosses;
- fast travel validado pelo servidor para viagens longas;
- seis biomas e seis estados climaticos;
- ciclo dinamico de dia/noite;
- VFX Niagara e SFX descobertos automaticamente entre os packs instalados;
- World Partition + PCG runtime particionado preparados para streaming por celulas;
- direcao visual realista com conteudo gratuito licenciado instalado localmente;
- fallback completo para o repositorio continuar funcional sem redistribuir assets de terceiros.

## Combate e armas

Familias implementadas:

1. Cajado;
2. Espada Grande de duas maos;
3. Duas Espadas;
4. Espada e Escudo;
5. Adagas;
6. Arco;
7. Arma de Fogo.

Cada arma possui ataque basico, `Q` e `E` ofensivos e `C` de cura. O cooldown base fica proximo de 3 segundos. `F` troca rapidamente entre as duas armas e uma habilidade conectada pela segunda arma dentro da janela de 2,5 segundos recebe o bonus de combo do prototipo.

Os cooldowns pertencem a cada familia. Usar `Q` da Greatsword nao coloca `Q` do Staff em cooldown.

### Passivas

Cada arma fornece tres passivas e as passivas das duas armas equipadas acumulam.

- Cajado: cooldown, cura e poder elemental;
- Espada Grande: dano, stamina e execucao;
- Duas Espadas: critico, bonus de esquiva e roubo de vida;
- Espada e Escudo: armadura, guarda e sustentacao por parry;
- Adagas: critico, veneno/sangramento e execucao;
- Arco: alcance, aljava elemental e critico;
- Arma de Fogo: dano, critico e recarga/cooldown.

### Flechas elementais

Com um arco no loadout, `V` alterna entre:

- Fisica;
- Fogo: dano continuo;
- Veneno: dano continuo prolongado;
- Eletrica: dano encadeado para alvo proximo;
- Gelo: reduz mobilidade do alvo.

O tipo elemental tambem e usado pelo sistema de Niagara/SFX para procurar efeitos visuais/sonoros compatíveis instalados no projeto.

### Defesa ativa

- stamina;
- block com botao direito;
- janela curta de parry ao levantar a guarda;
- parry perfeito anula dano e causa stagger;
- guard break quando a stamina acaba;
- dodge no `Alt esquerdo`;
- i-frames na parte inicial da esquiva;
- stagger por parry/golpes fortes;
- hit reactions e montages quando assets compativeis estao instalados.

## VFX e audio

`ANWWorldEventDirector` cataloga localmente os Niagara Systems e sons existentes em `/Game`.

A apresentacao das habilidades procura efeitos por tema da arma e, para o arco, tambem pelo elemento atual da flecha.

A mesma camada procura SFX equivalentes. Quanto mais packs compativeis forem instalados localmente, maior o repertorio visual/sonoro sem alterar o codigo de combate.

Veja `docs/CONTENT_EXPANSION.md`.

## HUD

O HUD em C++/UMG mostra:

- vida;
- stamina;
- armas dos slots 1/2;
- arma ativa;
- seis passivas combinadas do loadout;
- elemento atual da flecha quando o arco esta ativo;
- estado defensivo;
- nomes de `Q/E/C`;
- cooldown da arma ativa;
- destino selecionado de fast travel;
- estado da Armadura Brutal Lendaria;
- prompt de loot;
- bag/inventario/equipamento.

## Bag, armaduras, armas e loot procedural

Fluxo:

`mob morre -> pickup no mundo -> G coleta -> bag -> I abre -> setas selecionam -> Enter equipa/usa`.

A bag nao possui limite logico de slots no prototipo. Ela e rolavel e organizada automaticamente:

1. armas, agrupadas por familia;
2. armaduras, agrupadas por classe de peso e slot;
3. consumiveis.

Dentro de cada grupo, raridade e score ajudam a ordenar os melhores itens primeiro.

O loot procedural agora pode gerar tanto armaduras quanto armas. Cada familia possui diversos `StyleId` para permitir grande variedade visual quando os meshes gratuitos/licenciados forem ligados localmente.

### Armadura leve

Prioriza aceleracao, cura e esquiva. Estilos logicos incluem Arcanist, Shadowweave, Ranger, Duelist, Moonveil e Wanderer.

### Armadura media

Prioriza precisao e poder com perfil equilibrado. Estilos incluem Warden, Mercenary, Hunter, Battlemage, Corsair e Pathfinder.

### Armadura pesada

Prioriza armadura, vitalidade e guarda. Estilos incluem DreadKnight, RoyalGuard, IronVanguard, Dragonplate, Crusader e Obsidian.

Afixos atuais incluem Poder, Vitalidade, Precisao, Aceleracao, Cura, Veneno, Roubo de Vida, Armadura, Fogo, Gelo, Corrente Eletrica, Eco de Habilidade, reducao de cooldown em critico, Guarda Fortificada, Parry Restaurador, Impulso da Esquiva, Sangramento e Executor.

## Zumbis, fantasmas e world bosses

O diretor do mundo adiciona, alem dos mobs existentes:

- 18 zumbis errantes;
- 12 fantasmas errantes;
- 3 world bosses simultaneos.

Os bosses reaparecem periodicamente quando o total cai abaixo de tres. Podem usar os arquetipos Bruto, Zumbi ou Fantasma e surgem em regioes distantes do centro.

O balanceamento inicial de boss usa aproximadamente 1.040 a 1.380 de vida nos tiers atuais. Uma build lendaria bem montada deve conseguir vence-los sozinha; builds inferiores ainda podem tentar, mas exigem mais tempo, cura, parry, dodge e execucao mecanica.

Cada world boss derruba:

- 3 itens lendarios garantidos;
- 1 Pocao Lendaria de Metamorfose da Armadura Brutal garantida.

## Armadura Brutal Lendaria

A pocao e um consumivel lendario. Ao selecionar na bag e pressionar `Enter`, ela e consumida e ativa uma metamorfose de 300 segundos com:

- +120 de vida maxima;
- +35 de stamina maxima;
- +28% de dano;
- +15% de cura;
- +65 de armadura;
- +5% de roubo de vida;
- +12 de guarda.

A ativacao restaura vida e stamina para o novo maximo. A troca fisica para um mesh de armadura brutal especifico sera ligada quando o pack visual final correspondente estiver instalado; a mecanica e os atributos ja existem.

Guardioes de dungeon mantem 2-3 lendarios garantidos e agora possuem 35% de chance de soltar a mesma pocao.

## Fast travel

Fast travel e validado pelo servidor, so funciona fora de estados defensivos/dodge/stagger e possui cooldown inicial de 15 segundos.

`T` seleciona o proximo destino e `Y` confirma.

Destinos iniciais:

- Refugio Central;
- Portao do Castelo Sombrio;
- Entrada da Caverna Ancestral;
- Fronteira Norte;
- Fronteira Sul.

O servidor recalcula a altura final usando o terreno procedural antes de teleportar o personagem.

## Dungeons e loot lendario

### Castelo Sombrio

`ANWDungeonSite` gera um complexo fortificado procedural e procura meshes instalados contendo termos como `Gothic`, `Castle`, `Fortress`, `Wall`, `Arch`, `Pillar`, `Statue`, `Gargoyle`, `Ruins` e `Gate`.

Possui mobs e guardioes elite. Guardioes soltam 2 a 3 recompensas lendarias garantidas. O set `DarkCastle` favorece roubo de vida, execucao e eco de habilidade.

### Caverna Ancestral

Gera varias camaras e procura assets como `Cave`, `Rock`, `Boulder`, `Cliff`, `Crystal`, `Stalag` e `Mushroom`.

Tambem possui mobs/guardioes e loot lendario `AncientCave`, com afinidades como gelo, corrente eletrica e vitalidade conforme compatibilidade de item.

Tipo, seed e tier da dungeon sao replicados para que servidor e clientes reconstruam a mesma estrutura.

## Biomas, clima e dia/noite

Biomas iniciais:

- Floresta Temperada;
- Deserto;
- Neve;
- Pantano;
- Vulcanico;
- Amaldicoado.

Clima:

- Ceu Limpo;
- Chuva;
- Tempestade;
- Nevasca;
- Tempestade de Areia;
- Neblina Densa.

O ciclo completo de 24 horas dura inicialmente 12 minutos de playtest. Direcional, skylight e fog sao atualizados dinamicamente. Biomas tambem adaptam o clima local, como chuva global virando nevasca em regiao de neve ou tempestade de areia no deserto.

## Mundo procedural / streaming

O mundo ainda usa o gerador C++ como fallback, com terreno, foliage, recursos, assentamentos, civis, mobs e invasoes. O `ANWProceduralWorldManager` possui `UPCGComponent` em runtime/particionado e seed derivada do epoch.

`scripts/prepare-worldpartition.ps1` prepara localmente `/Game/GeneratedWorld/NW2_OpenWorld` via `WorldPartitionConvertCommandlet`. Os binarios/External Actors gerados ficam fora do Git.

## Conteudo externo

O projeto detecta/usa packs instalados localmente. O repositorio nunca redistribui esses arquivos.

Documentacao:

- `docs/REALISTIC_ASSETS.md`;
- `docs/CONTENT_EXPANSION.md`;
- `docs/COMBAT_INVENTORY.md`;
- `docs/ARSENAL_PASSIVES_BOSSES_TRAVEL.md`;
- `docs/WORLD_PARTITION_PCG.md`.

## Executar no Windows

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& .\scripts\clone-build-run.ps1
```

Informando a UE 5.8 manualmente:

```powershell
& .\scripts\clone-build-run.ps1 -UERoot "D:\Epic Games\UE_5.8"
```

Sem preparar World Partition:

```powershell
& .\scripts\clone-build-run.ps1 -SkipWorldPartition
```

## Controles

| Controle | Acao |
|---|---|
| WASD | mover |
| Mouse | camera |
| Espaco | pular |
| Shift | correr |
| Mouse esquerdo | ataque basico |
| Mouse direito | bloquear / janela de parry |
| Alt esquerdo | dodge |
| Q | habilidade ofensiva 1 |
| E | habilidade ofensiva 2 |
| C | habilidade de cura |
| 1 / 2 | selecionar armas |
| F | troca rapida / combo cross-weapon |
| Z / X | trocar familia das armas (debug) |
| V | alternar flecha Fisica/Fogo/Veneno/Eletrica/Gelo |
| G | coletar loot |
| I | abrir/fechar bag |
| Seta cima/baixo | selecionar item |
| Enter | equipar arma/armadura ou usar consumivel |
| T | selecionar destino de fast travel |
| Y | confirmar teleporte |
| R | novo epoch |

## Arquitetura relevante

```text
Source/NewWorld2/
├─ NWCombatTypes.h
├─ NWCombatLibrary.cpp/.h
├─ NWCombatHUDWidget.cpp/.h
├─ NWLootPickup.cpp/.h
├─ NWCharacter.cpp/.h
├─ NWEnemy.cpp/.h
├─ NWDungeonGuardian.cpp/.h
├─ NWDungeonSite.cpp/.h
├─ NWWorldTypes.h
├─ NWWorldEventDirector.cpp/.h
├─ NWProceduralWorldManager.cpp/.h
├─ NWCivilian.cpp/.h
├─ NWSettlementCore.cpp/.h
├─ PCGGraphInterface.h
└─ NWGameMode.cpp/.h
```

## Validacao

O repositorio nao possui runner com Unreal Engine 5.8, portanto a revisao remota e estrutural/API. A compilacao definitiva acontece pelo UnrealBuildTool no Windows no primeiro `clone-build-run.ps1`; o script nao inicia o jogo se o build C++ falhar.

## Licenca

O codigo proprio segue `LICENSE`. Conteudo de terceiros mantem suas licencas e nao deve ser republicado isoladamente no repositorio.
