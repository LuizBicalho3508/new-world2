# New World 2 - Roadmap

## Fase 0 - Vertical slice tecnico

Status: em andamento.

- [x] projeto Unreal Engine 5.8 em C++;
- [x] personagem terceira pessoa;
- [x] movimento, pulo e sprint;
- [x] ataque basico;
- [x] habilidade universal independente de arma;
- [x] vida/dano replicados;
- [x] mob PvE basico;
- [x] terreno procedural deterministico;
- [x] arvores, rochas e recursos por seed;
- [x] world epoch automatico;
- [x] regeneracao manual para teste;
- [x] configuracao inicial de performance;
- [x] bootstrap Windows clone/build/run;
- [ ] teste compilado em maquina com UE 5.8 instalada;
- [ ] HUD minimo de vida/cooldown/epoch;
- [ ] animacao de placeholder gratuita;

## Fase 1 - Combate realmente divertido

- locomocao com animacao;
- dodge/esquiva;
- stamina somente se melhorar o combate;
- hit reactions;
- ataques leve/pesado;
- bloqueio/parry;
- targeting opcional;
- 4-6 habilidades de loadout sem dependencia de arma;
- migracao para Gameplay Ability System;
- primeiro boss procedural simples.

Criterio para avancar: jogar 20-30 minutos ainda precisa ser divertido sem progressao por level.

## Fase 2 - Mundo procedural por celulas

- World Partition;
- PCG runtime/hierarquico;
- biomas por regras;
- rios/caminhos/pontos de interesse;
- recursos com respawn e migracao;
- eventos dinamicos;
- epochs separados em micro/meso/macro;
- streaming assincromo;
- seeds persistidas no servidor.

## Fase 3 - Itens e crafting generativos

- archetypes;
- materiais;
- afixos validos por regra;
- crafting;
- economia inicial;
- inventario;
- equipamentos;
- geracao deterministica auditavel;
- nenhuma progressao numerica infinita.

## Fase 4 - Multiplayer PvPvE

- listen server 2-8 jogadores;
- PvP com regras basicas;
- grupos;
- respawn;
- objetivos de mapa;
- Dedicated Server Linux;
- Replication Graph;
- testes de latencia/perda de pacote;
- 16, 32 e depois 64 jogadores por instancia conforme profiling.

## Fase 5 - Arte de alta qualidade

Somente depois do gameplay estar validado:

- pipeline Fab/Quixel/Epic gratuito;
- personagem final;
- animacoes;
- materiais de terreno;
- vegetacao;
- VFX Niagara;
- audio;
- clima;
- presets graficos completos;
- otimizar continuamente para GTX 1650.

## Fase 6 - Persistencia e produto

- conta;
- personagem;
- inventario;
- economia;
- shards/sessoes;
- logs/telemetria;
- seguranca/anti-cheat;
- launcher/update somente se realmente necessario;
- build distribuivel para playtest externo.

## Regra de escopo

Nao construir MMO, historia completa ou centenas de assets antes de provar quatro coisas:

1. combate divertido;
2. mundo procedural interessante;
3. multiplayer estavel;
4. performance no hardware-alvo.
