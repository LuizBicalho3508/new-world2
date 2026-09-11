# New World 2

Codename de um action RPG 3D procedural em terceira pessoa, PvPvE, construido em Unreal Engine 5.8.

> `New World 2` e um codename de desenvolvimento. O projeto nao reutiliza codigo, assets, historia, personagens, marcas ou conteudo do jogo New World. Antes de publicacao comercial, o produto deve receber nome e identidade proprios.

## Objetivo

Construir primeiro um vertical slice realmente jogavel e otimizado, sem custo obrigatorio de engine/assets/plugins, antes de aumentar o escopo para um mundo online maior.

Principios:

- sem level de personagem;
- progressao horizontal;
- habilidades independentes da arma equipada;
- PvE e PvP previstos desde o nucleo;
- terreno, recursos, inimigos e futuramente itens gerados por regras/seeds;
- mundo dividido em epochs, permitindo mudancas ao longo do tempo;
- servidor autoritativo para gameplay;
- escalabilidade grafica desde GTX 1650 ate GPUs modernas.

## Vertical slice 0.1

O repositorio ja contem:

- personagem terceira pessoa;
- WASD + camera por mouse;
- pulo e sprint;
- ataque corpo a corpo;
- habilidade universal em area (`Q`);
- vida/dano replicados;
- mob PvE simples;
- terreno procedural por Perlin noise;
- arvores, rochas e cristais instanciados;
- seed deterministica derivada do `WorldEpoch`;
- mudanca automatica de mundo a cada 180 segundos;
- tecla `R` para forcar novo epoch;
- configuracao grafica inicial focada em escalabilidade;
- script PowerShell para clone, build e execucao.

A arte deste primeiro teste usa primitivas internas da Unreal de proposito. O objetivo agora e validar sistema, nao aparencia final.

## Requisitos Windows

- Windows 10/11 64-bit;
- Unreal Engine 5.8 instalada pelo Epic Games Launcher;
- Visual Studio 2022 17.14+ ou Visual Studio 2026 com toolchain C++;
- Git;
- GPU DirectX 12 recomendada.

O script tenta instalar Git e Visual Studio Build Tools quando necessario. A Unreal Engine em si precisa ser instalada pelo fluxo suportado do Epic Games Launcher.

## Executar

Abra PowerShell e execute o script completo disponibilizado em `scripts/clone-build-run.ps1`, ou salve/copiei esse arquivo para qualquer pasta e rode:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& .\clone-build-run.ps1
```

Destino padrao:

```text
%USERPROFILE%\Documents\new-world2
```

Para informar manualmente onde a UE 5.8 esta instalada:

```powershell
& .\clone-build-run.ps1 -UERoot "D:\Epic Games\UE_5.8"
```

## Controles

| Controle | Acao |
|---|---|
| WASD | mover |
| Mouse | camera |
| Espaco | pular |
| Shift | correr |
| Mouse esquerdo | ataque |
| Q | habilidade universal em area |
| R | gerar novo epoch imediatamente |

## Estrutura

```text
new-world2/
├─ Config/
│  ├─ DefaultEngine.ini
│  ├─ DefaultGame.ini
│  └─ DefaultInput.ini
├─ Content/
├─ Source/
│  ├─ NewWorld2.Target.cs
│  ├─ NewWorld2Editor.Target.cs
│  └─ NewWorld2/
│     ├─ NewWorld2.Build.cs
│     ├─ NewWorld2.cpp/.h
│     ├─ NWGameMode.cpp/.h
│     ├─ NWCharacter.cpp/.h
│     ├─ NWEnemy.cpp/.h
│     └─ NWProceduralWorldManager.cpp/.h
├─ docs/
│  ├─ GAME_DESIGN.md
│  ├─ ARCHITECTURE.md
│  ├─ PERFORMANCE_TARGETS.md
│  ├─ FREE_CONTENT.md
│  └─ ROADMAP.md
├─ scripts/
│  └─ clone-build-run.ps1
└─ NewWorld2.uproject
```

## Proximos passos

Consulte `docs/ROADMAP.md`. A ordem e intencional: combate -> mundo procedural por celulas -> itens/crafting -> multiplayer -> arte final -> persistencia/produto.

## Licenca

O codigo proprio deste repositorio segue a licenca presente em `LICENSE`. Assets de terceiros mantem suas respectivas licencas e nao passam automaticamente a ser MIT por estarem usados no projeto.
