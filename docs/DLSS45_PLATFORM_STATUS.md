# DLSS 4.5 / Frame Generation - Premium V5

## Estado da integracao

O projeto possui uma camada runtime (`NWNvidiaPerformanceDirector`) que nao cria dependencia obrigatoria dos binarios NVIDIA. Ela procura os CVars publicados pelo plugin oficial DLSS/Streamline e, quando presentes, ativa:

- NVIDIA NGX
- DLSS Super Resolution
- NVIDIA Reflex Low Latency + Boost
- DLSS Frame Generation / Multi Frame Generation pelo Streamline
- VSync desligado para o caminho de Frame Generation

Quando o plugin nao esta presente, o projeto continua usando TSR e nao falha no boot.

## Unreal Engine 5.8

A NVIDIA disponibiliza DLSS 4.5 para Unreal Engine 5.8. O pacote atual inclui Streamline e NGX e oferece Super Resolution, Dynamic Multi Frame Generation, Ray Reconstruction, DLAA e Reflex.

## BigLinux / Linux nativo

O teste atual do projeto usa Unreal Engine 5.8 nativa para Linux com Vulkan. A distribuicao oficial do plugin Unreal DLSS/Streamline da NVIDIA ainda nao oferece o mesmo caminho de plugin nativo Linux/UE que o Win64. Por isso a Premium V5 NAO torna DLSS ou Frame Generation uma dependencia obrigatoria no BigLinux.

No Linux nativo:

- TSR permanece como fallback
- Vulkan/SM6 continua sendo usado
- o log `[RTX-V5]` informa se algum CVar NVIDIA foi realmente registrado
- nenhum CVar inexistente e forcado via `.ini`

## Win64 / Windows / Proton

Quando um build Win64 for preparado com o plugin oficial DLSS 4.5 para UE 5.8 instalado, a mesma camada runtime detectara os CVars e habilitara automaticamente DLSS SR, Reflex e Frame Generation.

A quantidade de frames gerados por Multi Frame Generation depende do suporte da GPU, do plugin/driver e da configuracao NVIDIA. Em RTX 50 Series o caminho de MFG fica disponivel quando suportado pelo runtime.

## Politica de performance V5

O `premium-v5-test-biglinux.sh` nao executa `DerivedDataCache -fill` completo por padrao. O cache existente e reutilizado e o jogo abre apos build + um gate curto de PSO. O full DDC continua disponivel com `--full-ddc`, mas possui budget configuravel e nunca bloqueia indefinidamente o playtest.
