# Arsenal, passivas, mortos-vivos, bag e fast travel

## Armas e loot

O loot procedural agora pode gerar:

- armaduras;
- armas;
- consumiveis.

Armas sao organizadas por familia na bag: Cajado, Espada Grande, Duas Espadas, Espada e Escudo, Adagas, Arco e Arma de Fogo.

Cada familia possui diversos `StyleId` para permitir varias aparencias. O mesh final e ligado aos assets locais instalados via Fab/Epic sem redistribuir conteudo licenciado no GitHub.

## Tres passivas por arma

As passivas das duas armas equipadas ficam ativas simultaneamente.

### Cajado

1. Reservatorio Arcano: cooldown mais rapido.
2. Tecelagem Vital: cura superior.
3. Mestre Elemental: efeitos elementais mais fortes.

### Espada Grande

1. Forca do Colosso: dano aumentado.
2. Folego de Guerra: stamina maxima aumentada.
3. Executor Implacavel: maior dano de execucao.

### Duas Espadas

1. Ritmo Gemeo: critico aumentado.
2. Danca de Laminas: bonus apos esquiva aumentado.
3. Fome de Combate: roubo de vida.

### Espada e Escudo

1. Baluarte: armadura extra.
2. Guarda Inabalavel: guarda/bloqueio superior.
3. Contra-Ataque: parry oferece sustentacao adicional.

### Adagas

1. Assassino Nato: critico aumentado.
2. Veneno Concentrado: veneno e sangramento mais fortes.
3. Golpe Final: execucao superior em alvos feridos.

### Arco

1. Olho de Aguia: alcance maior.
2. Aljava Elemental: alterna tipo elemental de flecha.
3. Cacador Preciso: critico aumentado.

### Arma de Fogo

1. Polvora Refinada: dano aumentado.
2. Olho Morto: critico aumentado.
3. Recarga Tatica: cooldown mais rapido.

## Flechas elementais

Com arco equipado, `V` alterna:

1. Fisica;
2. Fogo: dano continuo;
3. Veneno: dano continuo mais longo;
4. Eletrica: corrente para outro inimigo proximo;
5. Gelo: reduz movimentacao do alvo.

O diretor de VFX adiciona o elemento da flecha as palavras-chave usadas para localizar Niagara/SFX instalados localmente.

## Bag sem limite logico

O prototipo nao possui limite numerico de slots. Todo item coletado entra na bag e ela e ordenada automaticamente:

1. armas, agrupadas por familia;
2. armaduras, agrupadas por peso e slot;
3. consumiveis.

Dentro de cada grupo, itens mais raros e de maior score aparecem antes.

Em producao, uma bag muito grande devera usar persistencia e UI virtualizada/paginada para nao replicar milhares de entradas de uma vez, mas isso nao exige limitar a quantidade possuida pelo jogador.

## Zumbis, fantasmas e bosses

O diretor do mundo cria adicionalmente:

- 18 zumbis;
- 12 fantasmas;
- 3 world bosses simultaneos.

World bosses reaparecem quando o total fica abaixo de tres. Eles podem ser Bruto, Zumbi ou Fantasma e surgem em regioes aleatorias distantes do centro.

O balanceamento inicial foi pensado para permitir solo com build lendaria bem montada. Sets inferiores continuam podendo vencer, mas exigem mais tempo, defesa ativa, cura, dodge e boa execucao mecanica.

Cada world boss derruba:

- 3 itens lendarios garantidos;
- 1 Pocao Lendaria da Metamorfose da Armadura Brutal garantida.

Guardioes de dungeon continuam derrubando 2-3 lendarios e agora possuem 35% de chance da mesma pocao.

## Armadura Brutal Lendaria

Ao selecionar a pocao na bag e pressionar `Enter`, o consumivel desaparece e ativa por 300 segundos:

- +120 de vida maxima;
- +35 de stamina maxima;
- +28% de dano;
- +15% de cura;
- +65 de armadura;
- +5% de roubo de vida;
- +12 de guarda.

A ativacao tambem restaura vida e stamina para o novo maximo. A apresentacao visual final pode usar uma armadura/mesh lendario instalado localmente e Niagara de metamorfose.

## Fast travel

Controles:

- `T`: proximo destino;
- `Y`: confirmar teleporte.

Destinos iniciais:

- Refugio Central;
- Portao do Castelo Sombrio;
- Entrada da Caverna Ancestral;
- Fronteira Norte;
- Fronteira Sul.

O servidor valida o destino, exige estado normal de combate, aplica cooldown de 15 segundos e ajusta a altura final usando o terreno procedural.

## Conteudo visual recomendado para pesquisar no Fab

Gratuitos priorizados:

- `Medieval weapon axe and shield Set`;
- `GanzSe FREE Weapons - Fantasy Low Poly Pack`;
- `Stylized Medieval Weapons Packet`;
- `Ethereal Recurve Bow`;
- `Free Prototype Stylized Weapons V1`;
- `Lowpoly Modular Armors - Free - MEDIEVAL FANTASY SERIES`;
- `Crimson Knight Warrior - Free Version`;
- `Free Ghost`.

Opcoes maiores/pagas para avaliar futuramente, sem dependencia do codigo:

- `Modular Weapons Pack`;
- `Upgradable Weapons Bundle`;
- `Medieval Weapon`;
- `Knight` / `Fantasy Knight` packs modulares;
- `Undead Pack`;
- `Skeleton Character Pack`;
- `Demon Boss | Demon Boss Remake`;
- `Monster Boss`.

A arquitetura faz busca de meshes por palavras-chave e continua com placeholders/fallbacks se um asset nao estiver instalado. Nunca commitar `.uasset` de terceiros no repositorio publico sem confirmar permissao de redistribuicao.
