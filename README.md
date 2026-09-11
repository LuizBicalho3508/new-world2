# New World 2

Codename de um action RPG 3D procedural em terceira pessoa, PvPvE, construido em Unreal Engine 5.8.

> `New World 2` e um codename de desenvolvimento. O projeto nao reutiliza codigo, assets, historia, personagens, marcas ou conteudo de New World, Throne and Liberty ou qualquer outro jogo. Referencias servem apenas para direcao de genero/gameplay. Antes de publicacao comercial, o produto deve receber nome e identidade proprios.

## Objetivo

Construir um action RPG PvPvE com combate por armas, troca de loadout, itens procedurais e um mundo que se transforma ao longo do tempo, mantendo uma base tecnicamente viavel para hardware desde GTX 1650 ate GPUs modernas.

Principios atuais:

- sem level tradicional de personagem;
- progressao horizontal por equipamento, sinergias e dominio do loadout;
- 2 armas equipadas simultaneamente;
- 3 habilidades diferentes para cada familia de arma;
- 2 habilidades ofensivas + 1 habilidade de cura por arma;
- combo entre as duas armas quando habilidades sao encadeadas dentro da janela de combo;
- equipamentos com raridade e afixos procedurais;
- efeitos condicionais de equipamento capazes de modificar o combate;
- PvE e PvP previstos desde o nucleo;
- terreno, recursos, inimigos, assentamentos e populacao derivados de seed/epoch;
- invasoes de mobs contra cidades/NPCs;
- servidor autoritativo para gameplay;
- escalabilidade grafica e uso de instancing desde o prototipo.

## Vertical slice atual

### Combate

Familias implementadas:

1. Cajado;
2. Espada Grande de duas maos;
3. Duas Espadas;
4. Espada e Escudo;
5. Adagas;
6. Arco;
7. Arma de Fogo.

Cada arma possui:

- ataque basico proprio;
- habilidade ofensiva 1 (`Q`);
- habilidade ofensiva 2 (`E`);
- habilidade de cura (`C`);
- cooldown base proximo de 3 segundos;
- alcance/raio/potencia especificos;
- compatibilidade propria com efeitos especiais.

O personagem inicia com Espada Grande + Cajado. Para o prototipo, `Z` percorre as armas no slot 1 e `X` percorre as armas no slot 2, permitindo testar todas as familias antes de existir uma tela de inventario completa.

### Combo entre armas

Usar uma habilidade de uma arma, trocar para a outra e conectar outra habilidade dentro da janela de 2,5 segundos ativa multiplicador de combo. O prototipo usa +18% de dano no segundo golpe da sequencia.

### Equipamentos e drops procedurais

Mobs derrotados geram itens com:

- seed propria;
- slot de equipamento;
- raridade;
- de 1 a 3 afixos;
- magnitudes procedurais.

Afixos iniciais:

- Poder;
- Vitalidade;
- Precisao;
- Aceleracao;
- Cura;
- Revestimento Venenoso;
- Roubo de Vida.

O prototipo autoequipa um drop apenas quando sua pontuacao supera o item atual do mesmo slot. Inventario, pickup visual, comparador e descarte entram na proxima etapa.

### Exemplo de sinergia: luvas venenosas

O personagem inicia com `Luvas do Alquimista - Prototipo`, contendo `Revestimento Venenoso`.

Quando a arma ativa e compativel, ataques e habilidades aplicam dano adicional por veneno durante 3 ticks. Compatibilidade inicial:

- Espada Grande;
- Duas Espadas;
- Espada e Escudo;
- Adagas;
- Arco.

Cajado e arma de fogo nao recebem esse efeito, deixando a regra preparada para sinergias especificas de build em vez de bonus universais.

### Mundo vivo

O mundo atual gera por seed/epoch:

- terreno procedural;
- 260 arvores;
- 190 arbustos;
- 110 rochas;
- 40 recursos/cristais;
- 26 mobs ambientais;
- 2 assentamentos;
- casas, muralhas e torres instanciadas;
- 8 civis por assentamento.

A cada epoch o layout e regenerado deterministicamente para servidor/clientes.

### Invasoes

- primeira onda: ~20 segundos apos iniciar;
- recorrencia: ~55 segundos;
- 10 invasores por assentamento;
- inimigos priorizam jogador dentro do raio de aggro;
- fora disso continuam atacando civis e o nucleo da cidade;
- se o nucleo for rompido, ele e restaurado no prototipo para o teste continuar.

## Visual

A arquitetura esta sendo preparada para uma direcao realista, com:

- vegetacao densa;
- cidades e estruturas;
- NPCs e populacao;
- criaturas em grande quantidade;
- materiais/iluminacao escalaveis;
- LOD/HISM/streaming como requisitos de performance.

A arte do vertical slice ainda usa primitivas internas da Unreal propositalmente. O passo seguinte e substituir os placeholders por assets gratuitos licenciados, personagens animados, foliage realista, materiais de terreno e VFX sem comprometer a meta de hardware.

## Requisitos Windows

- Windows 10/11 64-bit;
- Unreal Engine 5.8 instalada pelo Epic Games Launcher;
- Visual Studio com toolchain C++ compativel;
- Git;
- GPU DirectX 12 recomendada.

## Executar

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& .\scripts\clone-build-run.ps1
```

Para informar a UE 5.8 manualmente:

```powershell
& .\scripts\clone-build-run.ps1 -UERoot "D:\Epic Games\UE_5.8"
```

## Controles

| Controle | Acao |
|---|---|
| WASD | mover |
| Mouse | camera |
| Espaco | pular |
| Shift | correr |
| Mouse esquerdo | ataque basico da arma atual |
| Q | habilidade ofensiva 1 |
| E | habilidade ofensiva 2 |
| C | habilidade de cura |
| 1 | selecionar arma do slot 1 |
| 2 | selecionar arma do slot 2 |
| F | alternar rapidamente entre as duas armas |
| Z | trocar a familia da arma do slot 1 (debug) |
| X | trocar a familia da arma do slot 2 (debug) |
| R | gerar um novo epoch imediatamente |

## Arquitetura relevante

```text
Source/NewWorld2/
├─ NWCombatTypes.h
├─ NWCombatLibrary.cpp/.h
├─ NWCharacter.cpp/.h
├─ NWEnemy.cpp/.h
├─ NWCivilian.cpp/.h
├─ NWSettlementCore.cpp/.h
├─ NWProceduralWorldManager.cpp/.h
└─ NWGameMode.cpp/.h
```

## Proximas etapas

1. HUD de habilidades/cooldowns/vida/arma ativa;
2. animacoes reais, dodge, block, parry, stagger e hit reactions;
3. inventario e loot visual no mundo;
4. mais afixos e efeitos que alterem habilidades;
5. personagens, foliage, construcoes e criaturas realistas usando conteudo gratuito licenciado;
6. streaming/World Partition/PCG por celulas;
7. perfis graficos e profiling em GTX 1650;
8. multiplayer dedicado e persistencia.

## Licenca

O codigo proprio deste repositorio segue a licenca presente em `LICENSE`. Assets de terceiros mantem suas respectivas licencas e nao passam automaticamente a ser MIT por estarem usados no projeto.
