# New World 2 - expansao Fab/Epic (2026-09-11)

Esta etapa integra ou prepara os packs encontrados na Fab sem redistribuir `.uasset` de terceiros no GitHub. Os assets precisam ser instalados localmente em `Content/` para serem detectados.

## Integrados no runtime

### Armas
- Thornblade Sword | Fantasy Game Ready Weapon
  - https://www.fab.com/listings/c2d48a7a-7006-42c7-b82c-a9fe70f35ff9
  - prioridade alta para espadas dark fantasy; PBR/realistic recebem boost mesmo que o anuncio tambem use a tag lowpoly.
- Dark Knight Longsword | Game Ready Weapon
  - https://www.fab.com/listings/cc7c30c4-c2a8-42cd-88d3-92727f3a15ca
  - prioridade alta para espada rara/lendaria.
- Free Fantasy Weapon Sample Pack
  - https://www.fab.com/listings/d5be0dc9-1a41-4be2-a63a-5ed436f3445d
  - entra como fonte adicional de visuais de armas.

### Personagens / inimigos
- Paragon: Minions
  - https://www.fab.com/listings/039ea035-9360-4e76-ad06-5d3a92da6f65
  - preferido para variar mobs Brute quando houver mesh + AnimBP compativeis.
- Paragon: Terra
  - https://www.fab.com/listings/5ea6bcb6-e43e-4bbe-813f-c19d8c907565
  - pode aparecer como variante visual de world boss humano/tank.
- Orc warrior axe and shield
  - https://www.fab.com/listings/b02e780e-1078-4766-ad1f-cbd1c556800c
  - somente fallback para mob menor. O anuncio e low-poly/stylized, entao nao entra na linha visual do personagem do jogador.

### VFX
- Free Torch Fire
  - https://www.fab.com/listings/7437ae0c-d67c-4f0a-af81-d03774cfdb82
  - usado para decorar Castelo Sombrio quando instalado.
- Free Arrow Trail
  - https://www.fab.com/listings/b8ff3ab4-0e81-4335-bbf0-fea15f6fcdfc
  - o sistema de flecha/Niagara ja procura Arrow/Trail e pode usar o pack automaticamente.

### Mundo / ambiente
- Free Sample - Fantasy Desert Ruins (Lost Desert Temple)
  - https://www.fab.com/listings/e1224238-fca3-42fd-b6e8-8b88ec47bcaa
  - usado como landmark visual automatico em uma celula de bioma Deserto quando um mesh compativel e encontrado.
- Free Dark Fantasy Stone Statue, Pedestal, and Sword
  - https://www.fab.com/listings/9aceb3ef-8c74-432b-b83a-4edfa957926f
  - usado como decoracao do Castelo Sombrio quando instalado.

### Audio
- Atmoshpheric Worlds - FREE Game Music Pack - No AI
  - https://www.fab.com/listings/c9e03a63-9359-4929-9c8a-a680309dbd3f
  - trilha ambiente muda por bioma usando busca por termos como Nordic, Dark, Fantasy, Western e Exploration.
- Procedural Sea Waves
  - https://www.fab.com/listings/2b11f781-970a-4c37-9ab2-90d16137555f
  - detectado e reservado para uma futura costa/oceano real. Nao toca automaticamente enquanto nao houver agua visual no mundo.

## Opcionais / nao obrigatorios

### Advanced Landscape Auto Material
- https://www.fab.com/listings/707fc1df-bbb8-4edc-8c40-9b3f9be356bd
- atualmente nao e dependencia do projeto.
- o New World 2 continua usando PCG + geracao procedural C++ mesmo sem este asset.

### Ultimate Bridge Creator
- https://www.fab.com/listings/34b07a69-69ec-4413-9301-7451869f73a7
- excelente para authoring de pontes, mas requer Houdini Engine e uma instalacao/licenca Houdini compativel.
- por isso nao foi transformado em dependencia de runtime.

## Politica de qualidade visual

Para o personagem do jogador, armas e armaduras, o runtime favorece `Realistic`, `PBR`, `4K`, `Medieval`, `Steel`, `Iron`, `Chainmail`, `Historical`, `GameReady` e assets premium conhecidos. `LowPoly`, `Stylized`, `Cartoon`, `Toon`, `Chibi` e similares permanecem fallback, exceto quando um asset explicitamente realista/PBR usa `lowpoly` apenas para indicar topologia otimizada.

O script `scripts/verify-fab-assets.ps1` lista todos esses grupos e informa quais realmente existem na pasta `Content/` do projeto.
