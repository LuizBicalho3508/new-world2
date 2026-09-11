# New World 2 - Curadoria de conteudo realista para o jogador

Esta camada define a prioridade visual do runtime depois da adicao dos novos packs gratuitos da Fab/Epic.

## Objetivo

Evitar que armas, armaduras ou personagens com aparencia lowpoly/stylized sejam escolhidos como primeira opcao quando houver assets PBR/realistas instalados.

## Prioridade de personagens / armaduras

O runtime passa a considerar como palavras-chave de alta prioridade:

- Classic Medieval Knight Warrior;
- Medieval King;
- Wasteland Warrior;
- Paragon Serath;
- Knight / Medieval / Plate / Chainmail / Steel;
- Realistic / PBR / 4K / Cinematic;
- modular armor quando o skeleton for compativel.

Assets contendo `lowpoly`, `low_poly`, `stylized`, `cartoon`, `toon` ou `chibi` recebem penalidade forte e so devem ser escolhidos se nao existir alternativa melhor.

## Prioridade de armas

As novas pesquisas da Fab adicionadas a biblioteca sao priorizadas por termos como:

- realistic;
- pbr;
- 4k;
- medieval;
- steel;
- iron;
- historical;
- game ready;
- long sword / short sword / dagger / shield / recurve.

Packs explicitamente lowpoly/stylized continuam suportados apenas como fallback.

## Packs gratuitos adicionados recentemente

O verificador local procura referencias para:

- Classic Medieval Knight Warrior;
- Medieval King;
- Wasteland Warrior;
- Paragon Serath;
- Free Sword Pack;
- Short Sword;
- Medieval Dagger;
- Daggers pack;
- Atris Swords;
- Medieval Iron and Wood Shield;
- Realistic Viking Shield;
- Ethereal Recurve Bow.

## Regra de skeleton

Armadura modular continua sendo aplicada automaticamente apenas quando o `USkeleton` e compativel com o personagem atual. Isso evita deformacao e pose invalida.

## Proxima fase

Depois do primeiro teste local com os `.uasset` efetivamente dentro de `Content/`, os caminhos e sockets reais podem ser fixados por pack para melhorar:

- escala;
- rotacao na mao;
- offsets;
- meshes diferentes por `StyleId`;
- combinacoes visuais de set;
- retarget/migracao definitiva para skeleton UE5/MetaHuman.
