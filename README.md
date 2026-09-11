# New World 2

Codename de um action RPG 3D procedural em terceira pessoa, PvPvE, construido em Unreal Engine 5.8.

> `New World 2` e um codename de desenvolvimento. O projeto nao reutiliza codigo, historia, personagens, marcas ou identidade de New World, Throne and Liberty ou qualquer outro jogo. Referencias servem apenas para direcao de genero/gameplay. Antes de publicacao comercial, o produto deve receber nome e identidade proprios.

## Direcao do projeto

- sem level tradicional de personagem;
- progressao horizontal por equipamento, afixos, sinergias e dominio mecanico;
- duas armas equipadas simultaneamente;
- sete familias de arma, cada uma com ataque basico e tres habilidades;
- duas habilidades ofensivas + uma cura por arma;
- combo entre armas por troca de loadout;
- PvE e PvP com servidor autoritativo;
- loot, atributos e propriedades gerados proceduralmente por seed;
- cidades, civis, criaturas e invasoes;
- mundo por epochs;
- World Partition + PCG runtime particionado preparados para streaming por celulas;
- direcao visual realista com conteudo gratuito licenciado instalado localmente;
- fallback completo sem assets de terceiros para o repositorio continuar clonavel e compilavel.

## Combate atual

Familias implementadas:

1. Cajado;
2. Espada Grande de duas maos;
3. Duas Espadas;
4. Espada e Escudo;
5. Adagas;
6. Arco;
7. Arma de Fogo.

Cada arma possui ataque basico, `Q` e `E` ofensivos e `C` de cura. O cooldown base continua proximo de 3 segundos. `F` troca rapidamente entre as duas armas e uma habilidade conectada pela segunda arma dentro da janela de 2,5 segundos recebe o bonus de combo do prototipo.

### Defesa ativa

O personagem agora possui:

- stamina;
- block com botao direito;
- janela curta de parry ao levantar a guarda;
- parry perfeito que anula dano e causa stagger no atacante;
- guard break quando a stamina acaba;
- dodge com `Alt esquerdo`;
- i-frames durante a parte inicial do dodge;
- stagger por ataques fortes/parry;
- hit reaction e montages de combate quando um pacote de animacao compativel esta instalado.

## HUD

O HUD e criado em C++/UMG e nao depende de Blueprint para aparecer. Ele mostra:

- vida atual/maxima;
- stamina;
- armas nos slots 1 e 2;
- arma ativa;
- estado de combate (normal, bloqueando, janela de parry, dodge ou stagger);
- nomes das tres habilidades da arma atual;
- cooldown individual em tempo real;
- prompt de loot proximo;
- inventario/equipamentos.

## Inventario e loot visual

Mobs nao autoequipam mais itens no jogador. Agora o fluxo e:

`mob morre -> item procedural aparece no mundo -> G coleta -> mochila -> I abre inventario -> setas selecionam -> Enter equipa`.

O pickup e replicado, flutua no mundo, possui nome e luz/cor de raridade e desaparece apos o tempo limite se nao for coletado.

## Itens e afixos

Os itens usam seed, item level, slot, raridade, magnitudes procedurais e ate quatro afixos em raridades mais altas.

Afixos implementados:

- Poder;
- Vitalidade;
- Precisao;
- Aceleracao;
- Cura;
- Revestimento Venenoso;
- Roubo de Vida;
- Armadura;
- Marca de Fogo;
- Mordida Gelida;
- Corrente Eletrica;
- Eco de Habilidade;
- Ritmo Critico;
- Guarda Fortificada;
- Parry Restaurador;
- Impulso da Esquiva;
- Sangramento;
- Executor.

Eles nao sao apenas numeros: varios modificam a mecanica. Exemplos atuais incluem veneno/sangramento/fire DoT, slow de gelo, dano que salta para outro alvo, eco de habilidade, reducao de cooldown em critico, bloqueio mais eficiente, cura ao executar parry, bonus apos dodge e dano extra contra alvos com pouca vida.

## Mundo vivo

A geracao atual inclui terreno, vegetacao, rochas, recursos, dois assentamentos, civis, mobs ambientais e invasoes recorrentes. O epoch continua alterando deterministicamente o estado do mundo.

O `ANWProceduralWorldManager` agora possui tambem um `UPCGComponent` configurado para `GenerateAtRuntime` e particionamento. Quando um grafo PCG local e atribuido, ele e gerado com seed derivada do epoch. Sem grafo, o gerador C++ atual permanece como fallback.

## World Partition

`scripts/prepare-worldpartition.ps1` cria localmente o mapa `/Game/GeneratedWorld/NW2_OpenWorld` e executa o `WorldPartitionConvertCommandlet`. O mapa gerado e seus External Actors ficam fora do Git porque sao artefatos locais/binarios.

O bootstrap principal chama esse preparador automaticamente. Se a conversao falhar, o teste abre o mapa fallback em vez de impedir a validacao dos outros sistemas.

## Conteudo realista gratuito

O codigo tenta detectar automaticamente conteudo gratuito instalado localmente, por exemplo:

- Paragon Greystone para o personagem de teste e animacoes;
- Paragon Sparrow para civis;
- Paragon Grux para criaturas;
- Open World Demo Collection / Megascans / Megascans Trees para foliage e rochas;
- packs gratuitos de construcoes medievais compativeis para assentamentos.

Esses arquivos nao sao redistribuidos no repositorio. Veja `docs/REALISTIC_ASSETS.md`.

## Executar no Windows

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& .\scripts\clone-build-run.ps1
```

Para informar manualmente a UE 5.8:

```powershell
& .\scripts\clone-build-run.ps1 -UERoot "D:\Epic Games\UE_5.8"
```

Para testar sem preparar World Partition:

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
| Mouse direito | bloquear / abrir janela de parry |
| Alt esquerdo | dodge |
| Q | habilidade ofensiva 1 |
| E | habilidade ofensiva 2 |
| C | habilidade de cura |
| 1 / 2 | selecionar armas |
| F | troca rapida / combo cross-weapon |
| Z / X | trocar familia das armas (debug) |
| G | coletar loot proximo |
| I | abrir/fechar inventario |
| Seta cima/baixo | selecionar item |
| Enter | equipar item selecionado |
| R | gerar novo epoch imediatamente |

## Arquitetura relevante

```text
Source/NewWorld2/
├─ NWCombatTypes.h
├─ NWCombatLibrary.cpp/.h
├─ NWCombatHUDWidget.cpp/.h
├─ NWLootPickup.cpp/.h
├─ NWCharacter.cpp/.h
├─ NWEnemy.cpp/.h
├─ NWCivilian.cpp/.h
├─ NWSettlementCore.cpp/.h
├─ NWProceduralWorldManager.cpp/.h
├─ PCGGraphInterface.h
└─ NWGameMode.cpp/.h

scripts/
├─ clone-build-run.ps1
└─ prepare-worldpartition.ps1
```

## Proximas prioridades

Depois da primeira compilacao/teste integrado: profiling real em GTX 1650 e RTX 5060, ajuste fino do feeling de combate, animacoes especificas por familia de arma, grafo PCG autorado no Editor, HLOD do World Partition, multiplayer dedicado e persistencia.

## Licenca

O codigo proprio deste repositorio segue a licenca presente em `LICENSE`. Assets de terceiros mantem suas respectivas licencas. Conteudo instalado via Fab/Epic nao passa a ser MIT e nao deve ser republicado isoladamente pelo repositorio.
