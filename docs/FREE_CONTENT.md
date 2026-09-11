# New World 2 - Conteudo sem custo

## Regra do projeto

A meta e chegar a uma versao totalmente jogavel sem comprar engine, plugins, assets, animacoes ou servicos obrigatorios.

Isso nao significa que todo asset gratuito possa ser redistribuido livremente no GitHub. Antes de adicionar qualquer conteudo de terceiros, conferir a licenca do item e registrar a origem.

## Fontes prioritarias

### Unreal Engine

A engine e a base principal do projeto. Recursos nativos como PCG, World Partition, Niagara, Chaos, Gameplay Ability System e ferramentas de profiling devem ser preferidos antes de plugins externos.

### Fab

Usar filtros `Price: Free` e, quando fizer sentido, `Publisher: Epic Games`.

Possiveis categorias:

- ambientes;
- vegetacao;
- materiais;
- efeitos;
- animacoes;
- personagens de amostra;
- projetos de exemplo.

### Conteudo Epic

Projetos/amostras gratuitos podem ser usados para estudo de arquitetura e, quando a licenca permitir, como base de recursos incorporados ao jogo. Nunca copiar nomes, marcas, personagens ou conteudo de outro jogo apenas porque existe um sample tecnico.

## Politica de repositorio publico

- Nao redistribuir asset de terceiros isoladamente.
- Nao subir pacotes baixados inteiros so para facilitar clone.
- Preferir um manifesto/documentacao do que precisa ser adquirido gratuitamente na biblioteca do desenvolvedor.
- Assets criados pelo projeto podem ser versionados normalmente.
- Guardar creditos/atribuicoes quando a licenca exigir, especialmente CC-BY.

## Arte do prototipo 0.1

O primeiro teste usa somente primitivas da propria Unreal Engine:

- capsule para jogador/mob;
- cylinder/cone para arvores;
- cube para rochas;
- sphere para recursos;
- terreno gerado por codigo.

Assim o clone inicial nao depende de nenhum download de asset alem da propria Unreal Engine 5.8.
