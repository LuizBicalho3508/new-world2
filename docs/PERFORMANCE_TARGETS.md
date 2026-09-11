# New World 2 - Metas de Performance

## Hardware-alvo inicial

O projeto deve continuar jogavel em configuracoes como:

- Intel Core i5-10300H + GTX 1650 4 GB + 16 GB RAM;
- Intel Core i7-3770 + RTX 5060, considerando que nesse caso o processador sera o principal gargalo.

Meta inicial de produto: 1080p com experiencia estavel em preset Baixo/Medio nesses perfis. A meta desejada e 45-60 FPS, mas ela so pode ser considerada compromisso depois de termos arte final suficiente para profiling real.

## Presets planejados

### Baixo
- sem Lumen;
- sem hardware ray tracing;
- sem Nanite obrigatorio;
- sombras convencionais reduzidas;
- densidade de vegetacao menor;
- distancia de visao menor;
- materiais simplificados;
- limite menor de efeitos simultaneos.

### Medio
- mesma base do Baixo com maior distancia, sombras e densidade;
- alvo principal para GTX 1650.

### Alto
- opcao de recursos modernos quando a GPU suportar;
- vegetacao e materiais mais densos;
- melhor iluminacao e reflexos.

### Ultra
- qualidade visual maxima, sem ser requisito de gameplay.

## Budgets que vamos medir

- Game Thread;
- Render Thread;
- GPU frame time;
- draw calls;
- quantidade de actors/ticks ativos;
- memoria de textura;
- memoria de malha;
- custo de AI;
- banda de rede por cliente;
- tempo de geracao de celula procedural;
- hitch durante streaming/epoch.

## Regras

1. Nao adicionar feature visual sem preset de desligamento/reducao.
2. Nao usar Tick por actor se timer/evento resolver.
3. Objetos repetidos usam instancing/HISM.
4. Mundo grande usa streaming e relevancia espacial.
5. AI distante reduz frequencia ou entra em simulacao simplificada.
6. Mudanca de epoch final sera incremental por celula, nao rebuild global.
7. Performance deve ser testada no hardware fraco durante todo o desenvolvimento, nao so no fim.
