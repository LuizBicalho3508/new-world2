# New World 2 - Game Design Base

## Visao do produto

Action RPG 3D em terceira pessoa com foco em combate responsivo, exploracao, PvE e PvP. O projeto nao tenta copiar historia, personagens, nomes, assets ou propriedade intelectual de New World; a referencia e apenas de sensacao de escala, leitura visual e combate de um RPG de acao moderno.

## Pilares

1. **Sem grind de level**: nao existe nivel de personagem como barreira de poder.
2. **Progressao horizontal**: o jogador evolui por conhecimento, itens, combinacoes, especializacoes, reputacao e dominio mecanico, sem transformar tempo jogado em vantagem numerica ilimitada.
3. **Habilidades independentes de arma**: trocar espada, machado, arco ou outra arma nao substitui automaticamente o conjunto de habilidades do personagem. Armas alteram alcance, cadencia, dano base, postura e propriedades fisicas; habilidades pertencem ao loadout do jogador.
4. **PvPvE desde a arquitetura**: o mesmo mundo suporta jogadores, criaturas, eventos e disputa por recursos/objetivos.
5. **Mundo mutavel**: terreno, distribuicao de recursos, criaturas, eventos e propriedades de itens podem mudar por epochs controlados pelo servidor.
6. **Geracao com regras, nao caos puro**: procedural nao significa aleatorio sem sentido. Biomas, rotas, seguranca, pontos de interesse e dificuldade devem obedecer restricoes de navegacao e balanceamento.
7. **Sem historia obrigatoria no MVP**: lore e narrativa ficam fora do caminho critico ate o combate, mundo e multiplayer estarem divertidos.

## Loop inicial

- Entrar no mundo.
- Explorar terreno e recursos gerados pela seed atual.
- Encontrar criaturas e outros jogadores.
- Combater, coletar materiais e testar builds.
- O mundo entra em um novo epoch e altera terreno/distribuicao/conteudo.
- Adaptar estrategia ao novo estado do mundo.

## Combate do vertical slice 0.1

- WASD: movimento.
- Mouse: camera.
- Espaco: pulo.
- Shift: corrida.
- Botao esquerdo: ataque corpo a corpo.
- Q: habilidade universal em area, independente da arma.
- R: forca novo epoch para teste.

O prototipo usa formas geometricas de debug de proposito. Arte, animacoes e VFX entram depois que locomocao, combate e geracao estiverem validados.

## Progressao futura

Em vez de level tradicional:

- slots de especializacao limitados;
- perks com vantagens e trade-offs;
- crafting por propriedades e materiais;
- itens com identidade gerada por seed e regras de raridade;
- reputacao/faccoes sem bonus bruto infinito;
- conquistas que liberam opcoes, nao multiplicadores permanentes de poder;
- builds com limite de pontos/modulos para manter PvP competitivo.

## Geracao de itens

Cada item deve ser reproduzivel por seed e composto por:

- archetype;
- material principal;
- qualidade de fabricacao;
- propriedades primarias;
- afixos compatíveis;
- modificadores situacionais;
- aparencia/variacoes permitidas;
- assinatura de seed para auditoria do servidor.

O servidor e a autoridade. O cliente nunca decide atributos de item, drop, dano ou resultado de crafting.

## Mundo por epochs

O prototipo 0.1 usa um novo epoch a cada 180 segundos. No produto final, o epoch sera dividido em camadas:

- **micro**: recursos, clima, patrulhas e eventos, mudando em minutos/horas;
- **meso**: distribuicao de criaturas, economia local e pontos de interesse, mudando em dias;
- **macro**: terreno/biomas/regioes, mudando em janelas maiores e com transicao segura.

Essa separacao evita que uma mudanca de terreno destrua uma luta ou base no meio de uma sessao.

## O que nao entra agora

- campanha/historia principal;
- monetizacao;
- marketplace real;
- guildas completas;
- housing;
- centenas de jogadores por shard;
- arte final;
- anti-cheat comercial;
- backend de conta definitivo.

Esses itens so entram depois do vertical slice comprovar combate, geracao, performance e rede.
