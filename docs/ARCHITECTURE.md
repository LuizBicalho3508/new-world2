# New World 2 - Arquitetura

## Principio central

O servidor e autoritativo para mundo, combate, inventario, drops, seeds e mudancas de epoch. O cliente renderiza, prediz movimento quando aplicavel e solicita acoes, mas nao cria resultados de gameplay confiaveis.

## Vertical slice 0.1

### Runtime

- Unreal Engine 5.8.
- C++ como nucleo de gameplay.
- `ANWGameMode` cria o mundo e controla o spawn inicial.
- `ANWProceduralWorldManager` gera terreno, vegetacao de debug, recursos e inimigos.
- `ANWCharacter` implementa locomocao, dano, ataque e habilidade universal.
- `ANWEnemy` representa o primeiro mob PvE replicado.

### Geracao deterministica

O estado minimo replicado e `WorldEpoch`. A seed e derivada desse valor. Clientes reconstroem localmente terreno e decoracao a partir da mesma seed, reduzindo trafego de rede. Entidades de gameplay relevantes, como inimigos e futuramente recursos coletaveis, continuam sob autoridade do servidor.

## Evolucao planejada

### Mundo

1. trocar a malha procedural simples por World Partition + PCG em grids;
2. separar terrain seed, biome seed, resource seed e event seed;
3. streaming por celulas proximas ao jogador;
4. gerar/cookar somente dados necessarios por celula;
5. cache de celulas procedurais por hash de seed;
6. transicoes de epoch por regiao, evitando reconstruir o mapa inteiro de uma vez.

### Multiplayer

1. listen server para teste rapido;
2. Dedicated Server Linux;
3. Replication Graph para relevancia espacial;
4. backend de sessao/conta separado do servidor de jogo;
5. persistencia de jogador e economia, nao da geometria completa do mundo;
6. shards/instancias quando os testes de concorrencia mostrarem necessidade.

### Combate

Quando o vertical slice estiver aprovado, migrar habilidades para Gameplay Ability System (GAS), mantendo a regra de habilidades independentes de armas. GAS sera usado para cooldowns, efeitos, tags, previsao e replicacao; nao sera usado para criar progressao por level.

### Itens

Itens serao DataAssets/archetypes + dados compactos gerados por seed. O banco persiste somente identificadores, seed, ownership e estado mutavel. A descricao completa do item e reconstruida de maneira deterministica no servidor.

## Performance

- HISM para objetos repetidos.
- PCG hierarquico/runtime generation no mundo final.
- pooling para mobs/VFX quando necessario.
- LOD/HLOD e culling agressivo.
- sem Lumen/Nanite obrigatorios nos presets baixos.
- limite de actors replicados por celula.
- ticks desabilitados quando nao necessarios.
- frequencia de rede e AI escaladas por distancia/relevancia.

## Seguranca

- dano calculado no servidor;
- cooldown validado no servidor;
- drops e crafting decididos no servidor;
- cliente nunca envia atributos finais de item;
- seeds de loot nao devem permitir previsao exploravel pelo cliente;
- telemetria de comportamento anomalo antes de adicionar anti-cheat externo.

## Repositorio

Nao subir assets do Fab de forma independente para repositorio publico quando a licenca nao permitir redistribuicao standalone. Cada colaborador deve adquirir os assets na propria biblioteca quando necessario. Codigo, configuracoes, DataAssets proprios e conteudo criado especificamente para o projeto podem ser versionados normalmente.
