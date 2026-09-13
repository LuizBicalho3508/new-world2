#!/usr/bin/env bash
set -u

LOG_FILE="${1:-$HOME/nw2-playable.log}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "ERRO: log nao encontrado: $LOG_FILE" >&2
    exit 1
fi

echo "============================================================"
echo " NEW WORLD 2 - RESUMO DO PLAYTEST"
echo "============================================================"
echo "Log: $LOG_FILE"
echo

count() {
    grep -cE "$1" "$LOG_FILE" 2>/dev/null || true
}

last() {
    grep -E "$1" "$LOG_FILE" 2>/dev/null | tail -n "${2:-10}" || true
}

echo "=== BOOT / INPUT ==="
last '\[BOOT\]|\[INPUT\]' 20

echo
echo "=== CATALOGO FAB ==="
last '\[FAB\] Catalogo|\[REALISMO\] Catalogo|\[FAB-EXPANSAO\] catalogo' 20

echo
echo "=== VISUAIS DETECTADOS ==="
last '\[VISUAL\]|\[REALISMO-ARMA\]|\[FAB-ARMA\]|\[FAB-INIMIGO\]|\[MONSTRO-VISUAL\]' 50

echo
echo "=== GAMEPLAY ==="
echo "Habilidades registradas : $(count '\[ABILITY\]')"
echo "Trocas/loadout          : $(count '\[LOADOUT\]')"
echo "Bag/HUD                 : $(count '\[HUD\] Bag')"
echo "Epochs regenerados      : $(count '\[WORLD\] Novo epoch')"
echo "Recuperacoes de terreno : $(count '\[SAFETY\] queda atraves')"
last '\[ABILITY\]|\[LOADOUT\]|\[HUD\] Bag|\[WORLD\] Novo epoch|\[SAFETY\] queda atraves' 40

echo
echo "=== ERROS IMPORTANTES ==="
last 'Fatal error|Log.*Error:|Segmentation fault|GPU crash|device lost|Out of memory|Assertion failed' 60

echo
echo "=== AVISOS DE FISICA / INPUT ==="
last 'FindTeleportSpot|EnhancedInput|Unknown Axis|unknown Eixo|unknown.*InputAction' 50

echo
echo "============================================================"
echo " INTERPRETACAO RAPIDA"
echo "============================================================"

EPOCHS=$(count '\[WORLD\] Novo epoch')
RECOVERIES=$(count '\[SAFETY\] queda atraves')
FATALS=$(count 'Fatal error|Segmentation fault|GPU crash|device lost|Out of memory|Assertion failed')

if (( FATALS > 0 )); then
    echo "[FALHA] Houve erro fatal/crash no playtest."
elif (( EPOCHS > 0 )); then
    echo "[ATENCAO] O mundo foi regenerado $EPOCHS vez(es). Se F10 nao foi pressionado, isso ainda e bug."
elif (( RECOVERIES > 3 )); then
    echo "[ATENCAO] Colisao ainda instavel: $RECOVERIES recuperacoes de emergencia."
else
    echo "[OK] Nenhum crash/reload automatico detectado; revisar controles/visuais acima."
fi
