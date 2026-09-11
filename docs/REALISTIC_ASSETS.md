# Conteudo realista gratuito - fluxo local

O codigo do New World 2 nao exige assets de terceiros para compilar. Os placeholders internos continuam como fallback, enquanto os packs abaixo podem ser instalados localmente para elevar a qualidade visual.

## Regra de licenca

Conteudo da Fab/Epic mantem sua propria licenca. A estrategia deste repositorio e:

1. versionar apenas codigo/configuracao produzidos para o projeto;
2. instalar packs licenciados pela conta do desenvolvedor diretamente pelo fluxo oficial da Epic/Fab;
3. usar os assets incorporados no jogo;
4. nunca republicar os `.uasset` de terceiros isoladamente no GitHub publico.

As pastas conhecidas desses fornecedores ficam no `.gitignore` justamente para impedir commit acidental.

## Packs priorizados

### Personagem jogador / combate

**Paragon: Greystone**

Uso no prototipo:

- personagem humanoide do jogador;
- Animation Blueprint;
- cadeia de ataques primarios A/B/C;
- habilidades Q/E/R como base visual temporaria;
- hit reactions.

O codigo tenta carregar automaticamente:

```text
/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone.Greystone
/Game/ParagonGreystone/Characters/Heroes/Greystone/Greystone_AnimBlueprint.Greystone_AnimBlueprint_C
```

Se o pack nao estiver presente, o placeholder continua ativo.

### Civis / NPCs

**Paragon: Sparrow**

Uso atual:

- visual humanoide para os civis dos assentamentos;
- locomocao via AnimBP do pack quando instalada.

Caminho esperado:

```text
/Game/ParagonSparrow/Characters/Heroes/Sparrow/Meshes/Sparrow.Sparrow
```

### Criaturas hostis

**Paragon: Grux**

Uso atual:

- substituicao opcional do corpo placeholder de inimigos;
- AnimBP do pack quando disponivel.

Caminho esperado:

```text
/Game/ParagonGrux/Characters/Heroes/Grux/Meshes/Grux.Grux
```

Outros packs gratuitos de criaturas/minions podem ser adicionados depois por archetype sem mudar a IA.

### Animacao adicional

Prioridades para a fase de polimento:

- **Game Animation Sample**: locomocao moderna e Motion Matching;
- **Animation Starter Pack**: biblioteca adicional de movimentos humanoides.

No estado atual, dodge/block/parry/stagger sao mecanicas reais mesmo sem esses packs. Quando uma animacao compativel de Greystone e encontrada, o personagem tenta usa-la. A fase seguinte pode retargetear Game Animation Sample/Starter Pack para o esqueleto final de cada personagem.

### Vegetacao / rochas / ambiente

Prioridades:

- **Open World Demo Collection**;
- **Megascans / Quixel**;
- **Megascans Trees**, incluindo arvores europeias gratuitas quando disponiveis na biblioteca.

O `ANWProceduralWorldManager` consulta o Asset Registry em pastas conhecidas e procura meshes com palavras como `Tree`, `Beech`, `Hornbeam`, `Oak`, `Pine`, `Bush`, `Shrub`, `Fern`, `Rock`, `Boulder` e `Cliff`.

Quando encontra uma arvore realista:

- substitui o tronco placeholder pelo mesh completo;
- desativa a copa geometrica separada;
- usa escala uniforme para evitar deformacao visual.

### Construcoes

O codigo esta preparado para procurar packs locais em pastas como:

```text
/Game/ElderBoom
/Game/MedievalVillage
/Game/Medieval
/Game/Village
```

E procurar meshes com nomes contendo `House`, `Building`, `Cottage` ou `Hut`.

Se nenhum pack compativel estiver instalado, as casas continuam usando geometria placeholder. Muralhas/torres permanecem separadas para uma substituicao modular posterior.

## Como instalar

Use Epic Games Launcher/Fab com a mesma conta que possui acesso ao projeto Unreal. Adicione os packs gratuitos a biblioteca e instale-os no projeto local `new-world2`.

Nao e necessario alterar o codigo depois: na inicializacao, o jogo tenta detectar automaticamente os assets suportados.

## Marcas

Os packs Paragon podem ser usados como conteudo licenciado dentro de projetos Unreal, mas o jogo nao deve usar `Paragon` como nome, marca ou publicidade do produto. Eles sao apenas conteudo temporario/de prototipagem visual para este projeto.
