# New World 2 - Roadmap

## Estado atual - vertical slice integrado

Status: aguardando primeira compilacao/teste local em uma maquina com Unreal Engine 5.8.

Ja implementado em codigo:

- [x] projeto Unreal Engine 5.8 em C++;
- [x] personagem terceira pessoa, movimento, pulo e sprint;
- [x] 7 familias de arma;
- [x] 3 habilidades por arma;
- [x] dois slots e combo cross-weapon;
- [x] vida/dano/armas/equipamentos replicados;
- [x] HUD nativo de vida, stamina, arma e cooldowns;
- [x] block, parry, dodge, i-frames, guard break e stagger;
- [x] hit reactions/montages quando pack visual compativel esta instalado;
- [x] inventario visual;
- [x] loot fisico/replicado no mundo;
- [x] raridade, item level e 18 tipos de afixo procedural;
- [x] afixos que alteram mecanica de combate;
- [x] mobs ambientais;
- [x] dois assentamentos, civis e invasoes;
- [x] terreno/vegetacao/recursos por seed/epoch;
- [x] auto-deteccao de personagem/criatura/foliage/construcao gratuitos instalados localmente;
- [x] componente PCG em GenerateAtRuntime/partitioned;
- [x] preparador local de mapa World Partition;
- [x] bootstrap Windows clone/build/World Partition/run;
- [ ] primeira compilacao real no Windows com UE 5.8;
- [ ] primeiro playtest completo;

## Proxima fase - estabilizacao do combate

Depois do primeiro build integrado:

- corrigir qualquer warning/erro real de toolchain;
- ajustar tempos de ataque, block, parry e dodge por sensacao de jogo;
- ataques leve/pesado;
- animation notifies para sincronizar hit frames com os montages;
- animacoes especificas por cada familia de arma;
- targeting/lock-on opcional;
- VFX/SFX por tipo de dano;
- primeiro boss com fases e ataques telegrafados;
- avaliar migracao das habilidades para Gameplay Ability System sem quebrar o modelo atual.

Criterio para avancar: o combate precisa sustentar uma sessao de 20-30 minutos mesmo sem level tradicional.

## Mundo procedural e streaming

Infraestrutura pronta:

- World Partition convertido localmente;
- PCG runtime particionado;
- gerador C++ fallback;
- seed por WorldEpoch;
- foliage/estruturas instanciadas.

Proximos passos:

- autorar o primeiro grafo PCG binario no Editor;
- hierarchical generation com grids por categoria;
- biomas por altura/umidade/temperatura;
- rios e caminhos;
- POIs/ruinas/cavernas;
- recursos com migracao/respawn;
- separar epochs em micro/meso/macro;
- Data Layers para estados de cidades/eventos;
- HLOD por categoria;
- streaming sources para eventos longe dos jogadores.

## Itens, crafting e economia

Ja existe:

- inventario;
- slots equipaveis;
- drops procedurais;
- raridade;
- afixos;
- item level derivado do estado do mundo;
- efeitos mecanicos.

Ainda falta:

- armas como itens reais de inventario;
- crafting por materiais/propriedades;
- sockets/runes;
- sets com bonus 2/3/5 pecas;
- comparador visual completo;
- descarte/desmontagem;
- economia local ligada aos assentamentos;
- persistencia/auditoria de seeds no servidor.

## Multiplayer PvPvE

- listen server 2-8 jogadores;
- regras de PvP;
- grupos;
- objetivos de mapa;
- Dedicated Server Linux;
- Replication Graph/Iris conforme profiling;
- testes de latencia/perda de pacote;
- escalar 16, 32 e depois 64 jogadores por instancia somente apos medicao.

## Arte de alta qualidade

O pipeline agora aceita conteudo gratuito licenciado localmente sem versionar assets de terceiros.

Proximos passos visuais:

- retarget/normalizacao de skeleton final;
- personagens finais proprios ou licenciados adequadamente;
- animations por arma;
- material de terreno realista;
- foliage com variacao/LOD/Nanite onde fizer sentido;
- construcoes modulares completas;
- mais especies de criaturas;
- Niagara;
- audio;
- clima/dia-noite;
- perfis graficos completos.

Meta de performance permanece: o jogo deve continuar ajustavel para GTX 1650 sem impedir qualidade superior em GPUs modernas.

## Persistencia e produto

- contas;
- personagem;
- inventario persistente;
- economia;
- shards/sessoes;
- logs/telemetria;
- seguranca/anti-cheat;
- build distribuivel para playtest externo.

## Regra de escopo

Nao construir historia completa ou infraestrutura de MMO massiva antes de provar quatro coisas:

1. combate divertido;
2. mundo procedural interessante;
3. multiplayer estavel;
4. performance no hardware-alvo.
