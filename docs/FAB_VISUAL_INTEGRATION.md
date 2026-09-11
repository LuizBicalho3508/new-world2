# Integracao visual Fab / Epic

Esta camada liga os packs gratuitos instalados localmente ao jogo sem redistribuir arquivos de terceiros no repositorio.

## Biblioteca x projeto

Adicionar um pack a Biblioteca da Epic/Fab confirma que ele pertence a conta, mas os arquivos ainda precisam estar presentes dentro de `Content/` para que o Unreal consiga usa-los no projeto.

Depois de adicionar os packs ao projeto pelo Fab/Epic, rode:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& .\scripts\verify-fab-assets.ps1
```

O runtime tambem registra linhas `[FAB]` no log indicando o que foi detectado em `/Game`.

## Armas

`ANWContentPresentationManager` cataloga `UStaticMesh` e escolhe o melhor candidato por:

1. familia da arma;
2. `StyleId` procedural do item;
3. palavras-chave como `Weapon`, `Melee` e `Fantasy`;
4. termos especificos como `Bow`, `Dagger`, `Greatsword`, `Shield`, `Musket` etc.

Comportamento:

- Greatsword: mesh de espada longa/greatsword;
- Dual Swords: mesmo conjunto visual nas duas maos;
- Sword + Shield: espada na direita e escudo detectado na esquerda;
- Daggers: adagas nas duas maos;
- Bow: prioriza `Bow`/`Recurve`, incluindo o Ethereal Recurve Bow;
- Staff: `Staff`, `Rod`, `Wand`;
- Firearm: `Musket`, `Rifle`, `Gun`.

Se nenhum mesh compativel existir, a arma continua funcional e apenas o visual e omitido.

## Armaduras modulares

Pecas de armor sao detectadas por slot e `StyleId`.

A camada somente aplica automaticamente uma peca quando o `USkeletalMesh` usa exatamente o mesmo skeleton do personagem atual. Isso evita deformacao, crash ou animacao incorreta.

Quando migrarmos o personagem-base definitivamente para o skeleton UE5 usado pelo pack modular, esta mesma camada passara a montar Head/Chest/Gloves/Legs/Boots via Leader Pose sem mudar a logica de inventario.

## Bosses e criaturas

Para packs Paragon conhecidos, o sistema tenta caminhos diretos e Animation Blueprints correspondentes.

World bosses distribuem visualmente entre:

- Rampage;
- Sevarog;
- Khaimera;
- Countess;
- Revenant.

Zumbis e fantasmas primeiro procuram um mesh instalado com Animation Blueprint compativel. Quando nao encontram, usam Paragon como fallback animado.

## Flechas elementais visiveis

O combate continua sendo calculado pelo servidor como antes.

A camada de apresentacao observa os montages de arco e cria `ANWArrowPresentationProjectile` local para representar:

- ataque basico;
- habilidade Q;
- chuva de flechas E com multiplos projeteis visuais.

O projectile visual procura um mesh contendo `Arrow`/`Projectile` e um Niagara adequado ao elemento:

- Fogo: `Fire`, `Flame`, `Ember`;
- Veneno: `Poison`, `Venom`, `Toxic`, `Acid`;
- Eletrica: `Lightning`, `Electric`, `Shock`, `Thunder`;
- Gelo: `Frost`, `Ice`, `Snow`, `Cold`;
- Fisica: `Arrow`, `Wind`, `Trail`.

Se nenhum Niagara for encontrado, existe iluminacao elemental de fallback, portanto Fogo/Veneno/Eletrica/Gelo continuam visualmente diferenciados.

Nesta etapa o actor de flecha e de apresentacao. O hit/dano segue o sistema servidor-autoritativo existente, evitando dano duplicado enquanto a camada visual e validada.

## Armadura Brutal Lendaria

Durante a metamorfose, a camada visual tenta localizar um Niagara de aura/poder/rage/energy/buff e adiciona uma iluminacao forte de fallback. O personagem tambem recebe uma pequena ampliacao visual enquanto os bonus mecanicos continuam sendo controlados por `ANWCharacter`.

## Logs importantes

```text
[FAB]
[VISUAL]
[ARMOR-VISUAL]
[MONSTRO-VISUAL]
```

Eles permitem descobrir rapidamente se o problema esta na instalacao do pack, no skeleton, na deteccao de nomes ou na logica do jogo.
