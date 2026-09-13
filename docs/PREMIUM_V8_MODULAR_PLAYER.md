# Premium V8 - Modular Player

## Motivo

O playtest V7 mostrou que o Greystone ainda era um corpo-base inadequado para um RPG de equipamento visivel: ele ja possui identidade visual/armadura propria, armas StaticMesh podiam aparentar sobreposicao e nenhuma das armaduras do teste foi realmente vestida.

A V8 separa corpo-base, arma e armadura.

## Corpo-base

A ordem de preferencia e:

1. Manny / SKM_Manny_Simple do Third Person da UE;
2. UEFN Mannequin / Game Animation Sample quando instalado;
3. basebody/underwear/underlayer compatível;
4. visual anterior apenas como fallback.

O script `scripts/prepare-neutral-player-linux.sh` tenta localizar Manny nos templates locais da mesma Unreal Engine antes do build. Ele nunca baixa conteudo licenciado e nunca apaga Content/.

## Armadura

Peitoral, luvas, pernas e botas so sao mostrados como SkeletalMesh quando a hierarquia de bones e compatível com a do corpo-base. A verificacao aceita o mesmo USkeleton ou uma Reference Skeleton identica (nomes e parents).

O fallback StaticMesh para tronco/pernas/luvas/botas foi removido porque uma peca rigida presa a um bone nao acompanha deformacao corporal e gera interpenetracao/flutuacao. Capacete permanece como fallback estatico seguro.

## Armas

O diretor amplia a descoberta de staff/scepter/wand/quarterstaff e calcula escala/offset pelo bounds da mesh. Para pivots centralizados, aproxima o punho de uma extremidade em vez de colocar o centro da arma na mao. Dual weapons usam componentes independentes.

## Startup e diagnostico

A V8 corrige o gate Niagara: `PollForCompilationComplete()` atualiza/consome compilacoes, enquanto `HasOutstandingCompilationRequests(true)` determina se ainda existe trabalho pendente. O conjunto de warm-up cai de 16 para 10 sistemas prioritarios.

O `DefaultEngine.ini` volta a usar `r.PSOPrecache.ProxyCreationStrategy=1`, conforme o warning produzido pela UE 5.8.2 do playtest.

## Overlay de profiling

`premium-v8-test-biglinux.sh` abre o jogo sem `stat game`, `stat unit`, `stat gpu` ou `stat fps`. Esses overlays so aparecem quando `--profile` e passado explicitamente.

## Animacao

A V8 remove a repeticao de busca/log de mob para skeletons que nao possuem sequencias compativeis. A ausencia de Idle/Run/Attack continua sendo reportada uma unica vez por familia de skeleton.

O personagem modular ainda exige uma etapa posterior de consolidacao/retarget das animacoes de combate para o skeleton definitivo. Nao se deve misturar montages Greystone com Manny/UEFN sem retarget.
