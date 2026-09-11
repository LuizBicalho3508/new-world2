# New World 2 - expansao visual, audio, equipamentos e dungeons

Este documento descreve o catalogo de conteudo externo que o projeto sabe aproveitar sem tornar o codigo dependente de um fornecedor especifico. Os assets continuam licenciados pelos respectivos autores e devem ser instalados localmente pelo fluxo oficial da Epic/Fab.

## Regra principal

- O repositorio versiona codigo, configuracao e documentacao proprios.
- Packs Fab/Epic devem ser adicionados a biblioteca da conta e instalados no projeto local.
- Nao publicar os `.uasset`, WAV, FBX, texturas ou outros arquivos de terceiros isoladamente no GitHub publico.
- O codigo usa Asset Registry e palavras-chave para descobrir VFX, sons, estruturas, cavernas e armas disponiveis localmente.
- Se um pack nao existir, o jogo continua com fallback funcional.

## VFX gratuitos recomendados

### Niagara Examples Pack - Epic Games

Fab:
https://www.fab.com/listings/0e188eca-4e54-4fb2-a9ed-d8b8a565e600

Prioridade maxima. Inclui exemplos de:

- explosoes;
- impactos;
- trails;
- sparks;
- fogo e fumaca;
- lightning;
- buffs/debuffs;
- efeitos de arma;
- dissolves;
- footprints;
- teleport/impact examples.

O `ANWWorldEventDirector` procura Niagara Systems por palavras-chave e usa o melhor resultado encontrado para cada arma/habilidade.

### Free Magic Niagara

Fab:
https://www.fab.com/listings/d0fe50c4-6ebe-40d5-b78a-56960832f49e

Uso: magic circles, slash, wave, fire, smoke e ataques magicos.

### MagicCircle VFX

Fab:
https://www.fab.com/listings/78873187-c121-4b6d-b776-f275b941bae2

Uso: conjuracao, cura, buff e habilidades de cajado.

### Free Spline VFX

Fab:
https://www.fab.com/listings/2b923e61-b02d-4cc9-bd0b-b067c9e6056e

Inclui efeitos de fogo, holy, electric/lightning, poison/miasma, wind, water, frost e energy.

### Free Niagara Particles - CC BY 4.0

Fab:
https://www.fab.com/listings/183732bc-c2fb-465c-9453-f70a1ce7ba2c

Atencao: este pack e CC BY 4.0. Manter a atribuicao exigida pela licenca nos creditos do produto.

### FREE Realistic Niagara Explosions Pack

Fab:
https://www.fab.com/listings/a48b3fa2-2ebf-42c2-8892-fa20a1eff289

Uso: arma de fogo, explosoes, impacto pesado, ambiente de guerra e chefes.

## Audio

### Gratuito: Free Realistic Sword Sound Effects Pack

Fab:
https://www.fab.com/listings/041c5773-f40e-4ae6-bb8b-8a3f36b20c27

Inclui 30 sons de espada: swings, slashes, impactos e sharpening.

### Opcional: biblioteca fantasy completa

Ha bibliotecas Fab com centenas de efeitos para:

- melee;
- arco;
- magia elemental;
- criaturas;
- footsteps;
- neve;
- cavernas;
- dungeon;
- castelos;
- loot;
- crafting;
- UI;
- tempestades e ambientes.

Esses packs podem ser pagos. O projeto nao depende deles. Qualquer `USoundBase` instalado dentro de `/Game` pode ser descoberto automaticamente pelo diretor de apresentacao conforme palavras-chave.

## Armaduras e roupas

### Gratuito: Lowpoly Modular Armors - Free - MEDIEVAL FANTASY SERIES

Fab:
https://www.fab.com/listings/d32023d6-cc7c-4a6b-bbc6-b0821c3d3391

Uso de prototipo e variedade modular. O jogo diferencia mecanicamente:

- **leve**: aceleracao, cura e especializacao de esquiva;
- **media**: precisao, poder e perfil equilibrado;
- **pesada**: armadura, vitalidade e especializacao de guarda.

O item procedural recebe tambem `AppearanceSeed`, `StyleId` e `SetId`, permitindo separar atributo de aparencia.

Estilos logicos atuais:

### Leves

- Arcanist;
- Shadowweave;
- Ranger;
- Duelist;
- Moonveil;
- Wanderer.

### Medias

- Warden;
- Mercenary;
- Hunter;
- Battlemage;
- Corsair;
- Pathfinder.

### Pesadas

- DreadKnight;
- RoyalGuard;
- IronVanguard;
- Dragonplate;
- Crusader;
- Obsidian.

Esses nomes sao identidades internas. O visual definitivo pode ser preenchido por muitos packs diferentes sem alterar o sistema de stats.

## Armas

### Gratuito: Medieval weapon axe and shield Set

Fab:
https://www.fab.com/listings/0842eb1b-c49a-4d3c-9a57-a081944427b6

### Gratuito: GanzSe FREE Weapons - Fantasy Low Poly Pack

Fab:
https://www.fab.com/listings/8d570eec-44d7-40eb-b02d-c77600146600

### Gratuito: Fantasy Evolution Arsenal - Stylized Low Poly Weapons

Fab:
https://www.fab.com/listings/8dddbd9b-50fb-47da-9d6b-bf3f53c570f0

A arquitetura mantem familia funcional separada do cosmetic/mesh. Futuramente varias skins podem representar a mesma Greatsword, Staff, Bow etc. sem alterar o balanceamento.

## Castelos sombrios e dungeons

### Gratuito: Free Sample Dark Fantasy Gothic Environment Kitbash

Fab:
https://www.fab.com/listings/338af1ae-80da-4d94-b3ef-38368e287678

Uso prioritario para `DarkCastle`.

### Gratuito: Dungeon Environment - 135+ Assets

Fab:
https://www.fab.com/listings/bb39bae4-7f7a-4127-b07e-151cf52db0f6

Uso: corredores, interiores, ruinas e dungeon medieval escura.

O ator `ANWDungeonSite` procura meshes contendo termos como `Gothic`, `Castle`, `Fortress`, `Wall`, `Arch`, `Pillar`, `Statue`, `Gargoyle`, `Ruins` e `Gate`.

O Castelo Sombrio atual possui:

- muralha procedural;
- torres;
- keep central;
- mobs;
- guardioes elite;
- drops lendarios tematicos `DarkCastle`.

## Cavernas

### Gratuito: Soul: Cave

Fab:
https://www.fab.com/listings/75f42402-40bb-4a1b-b557-18e2c9604273

### Gratuito: Destructible Cave Generator Lite

Fab:
https://www.fab.com/listings/7c8ec064-1270-4edd-844e-082f5ba9201d

O `ANWDungeonSite` procura `Cave`, `Rock`, `Boulder`, `Cliff`, `Crystal`, `Stalag`, `Mushroom` e termos equivalentes. A Caverna Ancestral atual gera varias camaras, monstros e guardioes com drops lendarios `AncientCave`.

## Neve

### Gratuito: DeformableSnowSystem

Fab:
https://www.fab.com/listings/e5fbb0e8-d234-414d-9599-b8d65e6d0517

Pode ser usado mais tarde para pegadas/deformacao. O sistema de mundo ja possui `Snow` como bioma e `Snow` como clima.

## Biomas e clima atuais

Biomas:

- Floresta Temperada;
- Deserto;
- Neve;
- Pantano;
- Vulcanico;
- Amaldicoado.

Climas:

- Ceu Limpo;
- Chuva;
- Tempestade;
- Nevasca;
- Tempestade de Areia;
- Neblina Densa.

O ciclo completo de 24 horas dura inicialmente 12 minutos para acelerar o playtest. O sol, skylight e fog sao atualizados dinamicamente. Efeitos Niagara instalados sao escolhidos para apresentar chuva, neve, tempestade de areia e neblina quando encontrados.

## Dungeons lendarias

Existem dois archetypes iniciais:

- `DarkCastle`;
- `AncientCave`.

Cada site tem mobs normais e guardioes de tier. Guardioes derrotados soltam 2 a 3 itens lendarios garantidos, alem do loot procedural normal.

Sets lendarios atuais:

- `DarkCastle`: foco em roubo de vida, execucao e eco de habilidade;
- `AncientCave`: foco em gelo, corrente eletrica e vitalidade, respeitando compatibilidade de slot.

Os atributos continuam determinados pelo servidor e reproduziveis por seed.

## Conteudo pago opcional

O projeto nao exige compras. Existem packs comerciais excelentes de VFX/audio, por exemplo bibliotecas com 90+ Niagara fantasy systems e centenas de sons de RPG. Eles devem ser vistos apenas como upgrade futuro; o vertical slice deve continuar funcionando com os packs gratuitos e fallbacks internos.
