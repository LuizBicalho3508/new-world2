# New World 2 - Game Design Base

## Visao do produto

Action RPG 3D em terceira pessoa com foco em combate responsivo, exploracao, PvE e PvP, mundo vivo e progressao horizontal por equipamento. O projeto nao copia historia, personagens, nomes, assets ou propriedade intelectual de outros jogos; referencias externas servem apenas para direcao de genero, leitura visual, ritmo e ergonomia de combate.

## Pilares

1. **Sem grind de level**: nao existe nivel de personagem como barreira principal de poder.
2. **Progressao horizontal**: o jogador evolui por equipamento, combinacoes, conhecimento, especializacoes e dominio mecanico.
3. **Combate definido pelas armas**: cada familia de arma possui identidade, ataque basico e 3 habilidades proprias.
4. **Duas armas por loadout**: o jogador alterna entre duas armas e recebe bonus ao encadear habilidades de armas diferentes dentro da janela de combo.
5. **Equipamento altera gameplay**: pecas nao servem apenas para subir numeros; afixos podem mudar comportamento do combate. Exemplo inicial: luvas com revestimento venenoso adicionam dano periodico a laminas e flechas.
6. **PvPvE desde a arquitetura**: jogadores, criaturas, cidades, NPCs e eventos compartilham o mesmo mundo e regras de autoridade.
7. **Mundo mutavel**: terreno, recursos, criaturas, assentamentos e propriedades de itens podem mudar por epochs controlados pelo servidor.
8. **Geracao com regras, nao caos puro**: procedural deve respeitar navegacao, coerencia espacial, raridade, balanceamento e identidade visual.
9. **Sem historia obrigatoria no MVP**: lore e narrativa ficam fora do caminho critico ate combate, mundo, performance e multiplayer estarem comprovados.

## Loadout de combate

O personagem equipa dois slots de arma. Familias iniciais:

- Cajado;
- Espada Grande de duas maos;
- Duas Espadas;
- Espada e Escudo;
- Adagas;
- Arco;
- Arma de Fogo.

Cada familia recebe:

- ataque basico;
- habilidade ofensiva 1;
- habilidade ofensiva 2;
- habilidade de cura;
- alcance, raio, potencia e ritmo proprios;
- compatibilidades diferentes com afixos de equipamento.

Cooldown base atual das habilidades: aproximadamente 3 segundos. Esse valor e propositalmente curto no prototipo para acelerar testes de combo e balanceamento.

## Combo entre duas armas

Fluxo de referencia:

1. usar Q/E da arma A;
2. trocar rapidamente para arma B;
3. usar Q/E da arma B dentro de 2,5 segundos;
4. aplicar multiplicador de combo no segundo golpe.

Valor inicial de teste: +18% de dano. O objetivo final e substituir parte desse bonus numerico por interacoes mecanicas, como detonacao de status, stagger, extensao de debuff e conversao elemental.

## Controles do vertical slice

- WASD: movimento;
- Mouse: camera;
- Espaco: pulo;
- Shift: corrida;
- Botao esquerdo: ataque basico da arma ativa;
- Q: habilidade ofensiva 1;
- E: habilidade ofensiva 2;
- C: habilidade de cura;
- 1/2: selecionar slots de arma;
- F: troca rapida;
- Z/X: percorrer familias de arma nos slots durante o prototipo;
- R: forcar novo epoch.

## Equipamentos, sets e afixos

Slots iniciais:

- Cabeca;
- Peitoral;
- Luvas;
- Pernas;
- Botas.

Cada drop possui seed, raridade e afixos. O sistema atual suporta:

- Poder;
- Vitalidade;
- Precisao;
- Aceleracao;
- Cura;
- Revestimento Venenoso;
- Roubo de Vida.

A filosofia de sets e **mudar a build, nao apenas somar atributo**. O prototipo ja implementa o primeiro exemplo funcional: luvas com `Revestimento Venenoso` fazem ataques e habilidades de Espada Grande, Duas Espadas, Espada e Escudo, Adagas e Arco aplicarem tres ticks de veneno. Cajado e arma de fogo nao recebem esse efeito.

Evolucao planejada de sets:

- bonus por 2/3/5 pecas;
- afixos que alteram geometria/range de habilidades;
- conversao de dano fisico em elemental;
- proc condicionado a block/parry/dodge;
- habilidades ganhando cargas, ricochete, area persistente ou detonacao;
- trade-offs para evitar uma unica build dominante.

## Geracao procedural de itens

Cada item deve ser reproduzivel por seed e composto por:

- archetype/slot;
- raridade;
- propriedades primarias;
- afixos compativeis;
- magnitudes dentro de faixas controladas;
- modificadores situacionais;
- assinatura de seed para auditoria do servidor.

O servidor e a autoridade. O cliente nunca decide atributos de item, drop, dano ou resultado de crafting.

## Mundo vivo

O vertical slice atual gera dois assentamentos com estruturas e civis. Mobs ambientais coexistem com ondas de invasao.

Regras iniciais:

- inimigos priorizam jogadores dentro do raio de aggro;
- sem jogador proximo, avancam contra NPCs e nucleo da cidade;
- primeira invasao ocorre aproximadamente 20 segundos apos o inicio;
- novas ondas surgem aproximadamente a cada 55 segundos;
- cada assentamento recebe 10 invasores por onda;
- breach do nucleo e restaurado no prototipo para manter o ciclo de teste continuo.

No produto final, ataques devem variar por faccao, clima, biome, recursos locais, horario e estado economico/regional.

## Mundo por epochs

O prototipo usa um novo epoch a cada 180 segundos. No produto final, o epoch sera dividido em camadas:

- **micro**: recursos, clima, patrulhas e eventos, mudando em minutos/horas;
- **meso**: distribuicao de criaturas, economia local, invasoes e pontos de interesse, mudando em dias;
- **macro**: terreno/biomas/regioes, mudando em janelas maiores e com transicao segura.

Essa separacao evita que uma mudanca de terreno destrua uma luta, cidade ou atividade no meio de uma sessao.

## Direcao visual

Objetivo: realismo estilizado de alta qualidade, com leitura clara de combate.

Prioridades:

- terreno com materiais por camada, umidade, rocha, lama, vegetacao e trilhas;
- foliage denso usando HISM/PCG/LOD;
- estruturas modulares com variacao procedural;
- NPCs com silhuetas e ocupacoes distintas;
- criaturas em grupos e eventos de massa;
- iluminacao atmosferica escalavel;
- presets graficos desde GTX 1650 ate GPUs modernas.

Enquanto o sistema-base estiver sendo validado, primitivas da Unreal permanecem como placeholders para nao prender o codigo a assets especificos/licencas externas.

## O que nao entra ainda

- campanha/historia principal;
- monetizacao;
- marketplace real;
- guildas completas;
- housing;
- centenas de jogadores por shard;
- arte final completa;
- anti-cheat comercial;
- backend de conta definitivo.

Esses itens entram apenas depois que combate, geracao, performance e rede forem comprovados.
