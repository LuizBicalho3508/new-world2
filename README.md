# New World 2

Codename de um action RPG 3D procedural em terceira pessoa, PvPvE, construido em Unreal Engine 5.8.

> `New World 2` e um codename de desenvolvimento. O projeto nao reutiliza codigo, historia, personagens, marcas ou identidade de New World, Throne and Liberty ou qualquer outro jogo. Referencias servem apenas para direcao de genero/gameplay. Antes de publicacao comercial, o produto deve receber nome e identidade proprios.

## Ambiente principal de desenvolvimento

O ambiente principal do projeto agora e **BigLinux / Linux nativo**.

- Unreal Engine 5.8 Linux;
- Vulkan;
- build C++ nativo com `Engine/Build/BatchFiles/Linux/Build.sh`;
- toolchain nativo da Unreal via `SetupToolchain.sh` quando necessario;
- Fab para Linux como plugin opcional;
- scripts Bash para instalacao, build, World Partition, assets e teste;
- scripts PowerShell mantidos somente como legado para Windows.

Guia completo: `docs/BIGLINUX_FIRST_TEST.md`.

### Primeira instalacao/teste no BigLinux

Abra o Konsole e execute:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/LuizBicalho3508/new-world2/main/scripts/first-test-biglinux.sh)
```

O bootstrap instala as dependencias do BigLinux, clona/atualiza o projeto, testa Vulkan, procura a UE 5.8, prepara o toolchain, valida assets, compila e inicia o jogo.

O download oficial da Unreal Engine para Linux exige login Epic. Se a Engine ainda nao estiver instalada, o script abre `https://www.unrealengine.com/en-US/linux` e pede que o ZIP da UE 5.8 seja deixado em `~/Downloads`. Na execucao seguinte, o proprio script detecta e extrai o ZIP.

Se o ZIP `Linux_Fab_5.8*.zip` estiver em Downloads, o bootstrap tambem instala o plugin Fab. O `.uproject` referencia `Fab` como opcional: sem o plugin, o projeto continua funcionando com fallbacks.

### Comandos Linux depois da primeira instalacao

Compilar e jogar:

```bash
cd ~/Projetos/new-world2
bash scripts/clone-build-run-linux.sh
```

Abrir o Editor para instalar/gerenciar conteudo Fab:

```bash
cd ~/Projetos/new-world2
bash scripts/open-editor-linux.sh
```

Validar assets Fab presentes em `Content/`:

```bash
cd ~/Projetos/new-world2
bash scripts/verify-fab-assets-linux.sh
```

Testar sem World Partition:

```bash
cd ~/Projetos/new-world2
bash scripts/clone-build-run-linux.sh --skip-world-partition
```

## Direcao do projeto

- sem level tradicional de personagem;
- progressao horizontal por equipamento, afixos, sinergias, passivas e dominio mecanico;
- duas armas equipadas simultaneamente;
- sete familias de arma, cada uma com ataque basico, tres habilidades ativas e tres passivas;
- passivas das duas armas equipadas ativas simultaneamente;
- 21 estados independentes de cooldown (7 familias x 3 habilidades);
- combo por troca de loadout;
- arco com flechas fisica, fogo, veneno, eletrica e gelo;
- PvE/PvP com autoridade de servidor;
- loot procedural de armas, armaduras e consumiveis;
- bag sem limite logico e organizacao automatica;
- armaduras leves, medias e pesadas;
- block, parry, dodge, stamina, stagger e hit reactions;
- zumbis, fantasmas, invasoes, dungeons e world bosses;
- world bosses planejados para solo por build lendaria forte;
- Pocao Lendaria de Metamorfose da Armadura Brutal;
- fast travel validado pelo servidor;
- seis biomas, clima dinamico e ciclo dia/noite;
- VFX Niagara, audio e meshes externos descobertos por Asset Registry;
- World Partition + PCG runtime;
- direcao visual realista com packs gratuitos/licenciados instalados apenas localmente;
- fallbacks para o repositorio funcionar sem redistribuir conteudo de terceiros.

## Armas e passivas

Familias implementadas:

1. Cajado;
2. Espada Grande;
3. Duas Espadas;
4. Espada e Escudo;
5. Adagas;
6. Arco;
7. Arma de Fogo.

Cada arma possui ataque basico, `Q`, `E`, `C` e tres passivas. As passivas das duas armas do loadout acumulam. Cooldowns pertencem a cada familia, portanto usar `Q` de uma Greatsword nao consome o `Q` do Staff.

Com arco, `V` alterna entre flecha Fisica, Fogo, Veneno, Eletrica e Gelo. O projectile visual e separado da autoridade de dano para evitar hits duplicados.

## Loot / bag

Fluxo:

`mob morre -> pickup -> G coleta -> I abre bag -> setas selecionam -> Enter equipa/usa`.

A bag e rolavel, sem limite logico de slots no prototipo, organizada por armas, armaduras e consumiveis. Armas podem mudar a familia do slot ativo; armaduras usam peso/slot; itens lendarios recebem visual/afixos especiais quando assets compativeis estao presentes.

## World bosses e Armadura Brutal

O mundo mantem tres bosses simultaneos. Cada world boss derruba tres lendarios e a Pocao da Armadura Brutal.

A metamorfose dura 300 segundos e atualmente fornece:

- +120 vida maxima;
- +35 stamina maxima;
- +28% dano;
- +15% cura;
- +65 armadura;
- +5% roubo de vida;
- +12 guarda.

A camada visual procura VFX/mesh compativel nos packs instalados; sem asset apropriado, usa fallback.

## Mundo / streaming

O gerador C++ continua como fallback e cria terreno, foliage, recursos, assentamentos, civis, mobs e invasoes. `ANWProceduralWorldManager` possui PCG runtime/particionado e seed por epoch.

No Linux, `scripts/prepare-worldpartition-linux.sh` prepara `/Game/GeneratedWorld/NW2_OpenWorld` via `WorldPartitionConvertCommandlet`. O mapa gerado, External Actors, caches e binarios ficam fora do Git.

## Fab / conteudo externo no Linux

O repositorio nunca redistribui assets Fab/Epic. O runtime procura automaticamente o conteudo instalado em `/Game`.

Packs ausentes nao devem impedir o boot. O projeto prioriza assets realistas/PBR e penaliza nomes `LowPoly`, `Stylized`, `Cartoon`, `Toon`, `Chibi` e equivalentes quando existem alternativas melhores.

Entre os grupos ja catalogados estao Paragon Greystone/Grux/Sparrow/Sevarog/Rampage/Khaimera/Countess/Revenant/Serath/Terra/Minions, armas realistas, Thornblade, Dark Knight Longsword, arco, shields, Free Arrow Trail, Torch Fire, Desert Ruins, dark fantasy statues e Atmospheric Worlds Music.

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
| Q / E | habilidades ofensivas |
| C | cura |
| 1 / 2 | selecionar armas |
| F | troca rapida / combo cross-weapon |
| Z / X | trocar familia das armas (debug) |
| V | alternar elemento da flecha |
| G | coletar loot |
| I | abrir/fechar bag |
| Seta cima/baixo | selecionar item |
| Enter | equipar/usar item |
| T | selecionar fast travel |
| Y | confirmar teleporte |
| R | novo epoch |

## Scripts

### Linux / BigLinux (principal)

- `scripts/first-test-biglinux.sh` - primeira instalacao e teste;
- `scripts/clone-build-run-linux.sh` - update/build/play;
- `scripts/prepare-worldpartition-linux.sh` - World Partition;
- `scripts/verify-fab-assets-linux.sh` - inventario Fab local;
- `scripts/install-fab-plugin-linux.sh` - instala o ZIP do Fab Linux;
- `scripts/open-editor-linux.sh` - abre o Editor.

### Windows (legado)

- `scripts/first-test-setup.ps1`;
- `scripts/clone-build-run.ps1`;
- `scripts/prepare-worldpartition.ps1`;
- `scripts/verify-fab-assets.ps1`.

## Documentacao

- `docs/BIGLINUX_FIRST_TEST.md`;
- `docs/REALISTIC_ASSETS.md`;
- `docs/CONTENT_EXPANSION.md`;
- `docs/COMBAT_INVENTORY.md`;
- `docs/ARSENAL_PASSIVES_BOSSES_TRAVEL.md`;
- `docs/WORLD_PARTITION_PCG.md`;
- `docs/FAB_EXPANSION_PACKS.md`.

## Validacao

O repositorio nao possui runner com Unreal Engine 5.8. Portanto, GitHub valida estrutura/diffs, mas a compilacao definitiva acontece localmente no BigLinux pelo UnrealBuildTool usando `Build.sh`. O script nao inicia o jogo se o build C++ falhar.

Os modulos C++ do projeto usam dependencias multiplataforma da Unreal e nao dependem de APIs Win32/MSVC.

## Licenca

O codigo proprio segue `LICENSE`. Conteudo de terceiros mantem suas proprias licencas e nao deve ser republicado isoladamente no repositorio.
