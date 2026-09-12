# New World 2 - primeiro teste no BigLinux

O fluxo principal de desenvolvimento local agora e Linux nativo. Os scripts PowerShell permanecem apenas como legado para Windows.

## O que a automacao faz

`scripts/first-test-biglinux.sh`:

1. confirma BigLinux/Manjaro/Arch e `pacman`;
2. atualiza o sistema e instala `base-devel`, Git, curl/wget, unzip/p7zip, CMake/Ninja, Python, clang/lld, rsync, Vulkan Tools e xdg-utils;
3. clona ou atualiza `~/Projetos/new-world2`;
4. procura uma Unreal Engine 5.8 Linux instalada;
5. se a Engine nao existir, procura o ZIP oficial em `~/Downloads` ou `~/Transferências`;
6. se o ZIP tambem nao existir, abre `https://www.unrealengine.com/en-US/linux`, que exige login Epic, e encerra de forma intencional para o usuario baixar o build Linux 5.8;
7. na execucao seguinte, extrai a UE em `~/Aplicativos/UnrealEngine-5.8`, grava `UE_ROOT` e valida Vulkan;
8. procura `Linux_Fab_5.8*.zip` e instala o plugin Fab no Engine quando o arquivo estiver disponivel;
9. valida os `.uasset` ja instalados no projeto;
10. executa o `SetupToolchain.sh` oficial da Engine quando necessario;
11. compila `NewWorld2Editor Linux Development` com `Build.sh`;
12. tenta preparar World Partition com o commandlet Linux;
13. abre o jogo em Vulkan, 1280x720, com FPS e log no terminal.

## Primeira execucao

No Konsole:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/LuizBicalho3508/new-world2/main/scripts/first-test-biglinux.sh)
```

Se a UE ainda nao estiver instalada, o navegador sera aberto. Entre na conta Epic e baixe o ZIP da Unreal Engine 5.8 para Linux. Deixe o ZIP em `~/Downloads` e execute o mesmo comando outra vez.

Na pagina Linux, se estiver disponivel para a versao 5.8, baixe tambem o ZIP `Linux_Fab_5.8.x...zip`. O bootstrap detecta esse ZIP e copia o plugin para a Engine.

## Fab no Linux

O `NewWorld2.uproject` referencia `Fab` como plugin `Optional`. Portanto:

- com o Fab instalado, ele pode ser carregado pelo projeto;
- sem o Fab, o projeto nao falha;
- assets ausentes continuam usando fallbacks do prototipo.

Depois do primeiro boot da Engine, para instalar os packs da Biblioteca Fab:

```bash
cd ~/Projetos/new-world2
bash scripts/open-editor-linux.sh
```

No Editor, abra Fab, entre na conta Epic e use `Add to Project` nos packs desejados. Depois feche o Editor e rode novamente:

```bash
cd ~/Projetos/new-world2
bash scripts/verify-fab-assets-linux.sh
bash scripts/clone-build-run-linux.sh
```

## Drivers / Vulkan

O projeto usa Vulkan no Linux. O bootstrap executa `vulkaninfo --summary` antes do build/play. Se esse teste falhar, corrija o driver de video primeiro.

Para NVIDIA, a UE 5.8 recomenda drivers modernos; a documentacao da Epic lista 570+ como referencia recomendada. Para AMD, a Epic recomenda RADV/Mesa recentes.

O script nao troca automaticamente o driver de video porque isso depende da GPU e do kernel instalados no BigLinux. Use a Central de Controle/Drivers do BigLinux para instalar a opcao recomendada para o hardware.

## Scripts Linux

- `scripts/first-test-biglinux.sh`: instala/prepara tudo e inicia o primeiro teste;
- `scripts/clone-build-run-linux.sh`: atualiza, compila e executa o jogo em Linux;
- `scripts/prepare-worldpartition-linux.sh`: prepara o mapa World Partition via commandlet Linux;
- `scripts/verify-fab-assets-linux.sh`: verifica os packs Fab dentro de `Content/`;
- `scripts/install-fab-plugin-linux.sh`: instala o ZIP do plugin Fab Linux 5.8;
- `scripts/open-editor-linux.sh`: abre o Editor para gerenciamento/importacao de assets.

## Onde ficam os arquivos

Padroes usados pelo bootstrap:

- projeto: `~/Projetos/new-world2`;
- Engine: `~/Aplicativos/UnrealEngine-5.8`;
- configuracao persistente: `~/.config/new-world2/env.sh`;
- log do teste: `~/Projetos/new-world2/Saved/Logs/first-test-linux-console.log`;
- log do Editor: `~/Projetos/new-world2/Saved/Logs/editor-linux-console.log`.

## Teste rapido sem World Partition

Se o commandlet de World Partition for a primeira fonte de erro, isole o build/gameplay:

```bash
cd ~/Projetos/new-world2
bash scripts/clone-build-run-linux.sh --skip-world-partition
```

## Observacao sobre a Engine

A Epic suporta Linux com build pre-compilado instalado ou build a partir do codigo-fonte. O fluxo deste projeto usa preferencialmente o build Linux pre-compilado oficial porque e muito mais simples para o primeiro teste. O download exige autenticacao Epic e, por isso, essa etapa permanece manual.
