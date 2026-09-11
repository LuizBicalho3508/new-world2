# Combate, HUD, inventario e afixos

## Fluxo de combate

O combate e server-authoritative. O cliente solicita ataques/acoes, mas dano, cooldown, stamina, loot e equipamento sao decididos pelo servidor.

### Ataque e armas

- Mouse esquerdo: ataque basico da arma ativa.
- Q: habilidade ofensiva 1.
- E: habilidade ofensiva 2.
- C: habilidade de cura.
- 1/2: selecionar slot.
- F: troca rapida entre armas.
- Janela de combo cross-weapon: 2,5 s.

### Block

Mouse direito inicia guarda. Enquanto bloqueando:

- dano frontal e reduzido;
- stamina e drenada conforme a potencia do golpe;
- `Guarda Fortificada` melhora reducao e eficiencia de stamina;
- stamina zerada causa guard break/stagger.

### Parry

Nos primeiros ~0,22 s da guarda existe janela de parry.

Parry perfeito:

- anula o dano;
- recupera pequena quantidade de stamina;
- aplica stagger no atacante quando compativel;
- `Parry Restaurador` tambem cura o defensor.

### Dodge

Alt esquerdo executa dodge na direcao atual.

- custo: 22 stamina no prototipo;
- i-frames iniciais: ~0,28 s;
- duracao do estado: ~0,48 s;
- `Impulso da Esquiva` fortalece o proximo golpe por alguns segundos.

### Stagger / hit reaction

Golpes muito altos em relacao a vida maxima podem gerar stagger. Parry tambem pode staggerar o atacante. Quando os assets de animacao compativeis estao presentes, o estado dispara hit reactions/montages; sem assets, a mecanica continua funcional.

## HUD

O widget `UNWCombatHUDWidget` e construido integralmente em C++.

Exibe:

- vida;
- stamina;
- arma 1 / arma 2;
- arma ativa;
- estado de combate;
- Q/E/C da arma atual;
- cooldown de cada habilidade;
- loot proximo;
- inventario e equipamento.

## Loot

`ANWLootPickup` representa itens fisicos no mundo.

Cada pickup:

- replica seu item;
- possui nome em world-space;
- usa cor/luz de raridade;
- flutua/rotaciona;
- pode ser coletado com G;
- expira apos 180 s no prototipo.

## Inventario

Capacidade inicial: 30 itens.

Controles:

- I: abrir/fechar;
- seta cima/baixo: selecionar;
- Enter: equipar.

Equipar uma peca troca apenas o mesmo slot. A peca anterior volta para a mochila se houver espaco.

## Afixos mecanicos

### Ofensivos

- `Poder`: multiplicador geral de dano.
- `Precisao`: chance de critico.
- `Revestimento Venenoso`: DoT em armas compativeis.
- `Marca de Fogo`: DoT de fogo.
- `Sangramento`: DoT para armas fisicas compativeis.
- `Mordida Gelida`: slow temporario em alvo atingido por habilidade.
- `Corrente Eletrica`: habilidade pode propagar parte do dano para outro alvo proximo.
- `Eco de Habilidade`: chance de repetir uma fracao do dano pouco depois do impacto.
- `Ritmo Critico`: criticos reduzem cooldowns ainda ativos.
- `Executor`: dano adicional contra alvo abaixo de 30% de vida.

### Defensivos / sustain

- `Vitalidade`: aumenta vida e um pouco de stamina.
- `Armadura`: reduz dano recebido.
- `Roubo de Vida`: cura proporcional ao dano causado.
- `Cura`: aumenta habilidades de cura.
- `Aceleracao`: reduz cooldown base.
- `Guarda Fortificada`: block mais eficiente.
- `Parry Restaurador`: parry cura.
- `Impulso da Esquiva`: proximo dano apos dodge e fortalecido.

## Geracao procedural

`NWCombat::GenerateProceduralItem(seed, epoch)` e deterministico para a mesma combinacao de seed/epoch.

O item inclui:

- item level;
- slot;
- raridade;
- quantidade de afixos ligada a raridade;
- regras de compatibilidade por slot;
- magnitudes escaladas;
- score para comparacao no HUD.

O item level acompanha lentamente a evolucao do mundo, nao um level tradicional do personagem.
