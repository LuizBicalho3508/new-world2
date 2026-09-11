# New World 2 - Game Design Base

## Visao do produto

Action RPG 3D em terceira pessoa com foco em combate responsivo, exploracao, PvE e PvP, mundo vivo e progressao horizontal por equipamento. O projeto nao copia historia, personagens, nomes, assets ou propriedade intelectual de outros jogos; referencias externas servem apenas para direcao de genero, leitura visual, ritmo e ergonomia de combate.

## Pilares

1. **Sem grind de level**: nao existe nivel de personagem como barreira principal de poder.
2. **Progressao horizontal**: o jogador evolui por equipamento, combinacoes, conhecimento, especializacoes e dominio mecanico.
3. **Combate definido pelas armas**: cada familia possui identidade, ataque basico e 3 habilidades proprias.
4. **Duas armas por loadout**: trocar entre duas armas permite combos cross-weapon.
5. **Defesa ativa**: posicionamento, stamina, block, parry e dodge importam tanto quanto dano.
6. **Equipamento altera gameplay**: pecas podem mudar status, cooldown, defesa, mobilidade e comportamento das habilidades.
7. **PvPvE desde a arquitetura**: jogadores, criaturas, cidades, NPCs e eventos compartilham o mesmo mundo e regras de autoridade.
8. **Mundo mutavel**: terreno, recursos, criaturas, assentamentos e itens podem mudar por epochs controlados pelo servidor.
9. **Geracao com regras, nao caos puro**: procedural respeita navegacao, coerencia espacial, raridade, balanceamento e identidade visual.
10. **Sem historia obrigatoria no MVP**: narrativa fica fora do caminho critico ate combate, mundo, performance e multiplayer estarem comprovados.

## Loadout de combate

Familias iniciais:

- Cajado;
- Espada Grande de duas maos;
- Duas Espadas;
- Espada e Escudo;
- Adagas;
- Arco;
- Arma de Fogo.

Cada familia recebe ataque basico, duas habilidades ofensivas, uma cura e valores proprios de alcance/raio/potencia. Cooldown base atual: aproximadamente 3 segundos.

## Combo entre duas armas

Fluxo de referencia:

1. usar Q/E da arma A;
2. trocar para arma B;
3. usar Q/E da arma B em ate 2,5 segundos;
4. receber o bonus de combo no segundo golpe.

Valor atual de teste: +18% de dano. A evolucao desejada e transformar parte desse bonus em interacoes como detonacao de status, stagger, extensao de debuff e conversao elemental.

## Defesa ativa

### Block

Mouse direito inicia guarda. Golpes frontais drenam stamina e tem dano reduzido. `Guarda Fortificada` melhora a eficiencia. Stamina zerada causa guard break/stagger.

### Parry

A abertura da guarda possui janela de aproximadamente 0,22 segundo. Um parry perfeito:

- anula o golpe;
- recupera stamina;
- pode curar via `Parry Restaurador`;
- staggera atacantes compativeis.

### Dodge

Alt esquerdo executa uma esquiva direcional. No prototipo:

- custo de 22 stamina;
- ~0,28 s de invulnerabilidade inicial;
- ~0,48 s de estado total;
- `Impulso da Esquiva` fortalece o proximo ataque.

### Stagger e hit reactions

Parry, guard break e golpes de alto impacto podem causar stagger. Quando pacotes de animacao compativeis estao instalados, esses estados disparam montages/hit reactions; a regra de gameplay nao depende da presenca do asset visual.

## Controles do vertical slice

- WASD: movimento;
- Mouse: camera;
- Espaco: pulo;
- Shift: corrida;
- Botao esquerdo: ataque basico;
- Botao direito: block/parry;
- Alt esquerdo: dodge;
- Q/E: ofensivas;
- C: cura;
- 1/2: selecionar slots;
- F: troca rapida;
- Z/X: percorrer familias no prototipo;
- G: coletar loot;
- I: inventario;
- setas: selecionar item;
- Enter: equipar;
- R: novo epoch.

## Equipamentos, inventario e afixos

Slots atuais:

- Cabeca;
- Peitoral;
- Luvas;
- Pernas;
- Botas.

O drop agora aparece fisicamente no mundo e precisa ser coletado. Itens carregam seed, item level, raridade e afixos. O inventario inicial suporta 30 itens e a troca de equipamento ocorre manualmente.

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

A filosofia permanece: **mudar a build, nao apenas somar atributo**. Alguns efeitos atuais aplicam DoT, slow, chain damage, repeticao parcial de habilidade, cooldown em critico, melhoria de guarda/parry/dodge ou bonus de execucao.

## Sets

A infraestrutura de afixos ja permite construir sets depois. Direcao planejada:

- bonus por 2/3/5 pecas;
- alteracao de geometria/range de habilidade;
- conversao elemental;
- cargas adicionais;
- ricochete;
- area persistente;
- detonacao de status;
- trade-offs para evitar uma unica build dominante.

## Geracao procedural de itens

Cada item deve ser reproduzivel por seed e inclui:

- archetype/slot;
- item level ligado lentamente ao estado do mundo, nao a level do personagem;
- raridade;
- afixos compativeis por slot;
- magnitudes controladas;
- score de comparacao;
- seed para auditoria.

O servidor e a autoridade. O cliente nunca decide atributos, drop, dano ou resultado de crafting.

## Mundo vivo

O vertical slice gera dois assentamentos com estruturas e civis, mobs ambientais e ondas de invasao. Inimigos priorizam jogadores no raio de aggro; fora dele atacam civis e nucleo da cidade.

Regras atuais:

- primeira invasao: ~20 s;
- recorrencia: ~55 s;
- 10 invasores por assentamento;
- breach do nucleo e restaurado no prototipo para manter o ciclo de teste.

No produto final, ataques devem variar por faccao, clima, bioma, recursos, horario e estado economico/regional.

## Mundo por epochs

O prototipo usa novo epoch a cada 180 segundos para acelerar validacao. A direcao final separa:

- **micro**: recursos, clima, patrulhas e eventos;
- **meso**: criaturas, economia local, invasoes e POIs;
- **macro**: terreno/biomas/regioes.

## Streaming e PCG

A base agora prepara World Partition localmente e um PCG Component particionado em runtime. O grafo PCG binario sera autorado no Editor; ate la, o gerador C++ continua como fallback para manter o prototipo executavel.

O objetivo de escala e usar grids diferentes para ground cover, vegetacao, recursos e POIs, evitando gerar tudo na mesma granularidade.

## Direcao visual

Objetivo: realismo estilizado de alta qualidade com leitura clara de combate.

Prioridades:

- personagens humanoides realistas;
- animacoes de locomocao/combate;
- foliage denso com PCG/HISM/LOD;
- estruturas modulares;
- criaturas variadas;
- materiais de terreno;
- iluminacao atmosferica escalavel;
- presets desde GTX 1650 ate GPUs modernas.

O codigo usa conteudo gratuito licenciado apenas quando instalado localmente; nenhum asset de terceiro e redistribuido isoladamente no GitHub.

## O que nao entra ainda

- campanha/historia principal;
- monetizacao;
- marketplace real;
- guildas completas;
- housing;
- centenas de jogadores por shard;
- anti-cheat comercial;
- backend definitivo.

Esses itens entram apenas depois que combate, geracao, performance e rede forem comprovados.
