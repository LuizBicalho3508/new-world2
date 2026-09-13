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

echo "=== BOOT / INPUT / PLAYTEST ==="
last '\[BOOT\]|\[INPUT\]|\[PLAYTEST\]|\[TERRAIN\]' 40

echo
echo "=== CATALOGO / VISUAIS ==="
last '\[FAB\] Catalogo|\[REALISMO\] Catalogo|\[FAB-EXPANSAO\] catalogo|\[VISUAL\]|\[REALISMO-ARMA\]|\[VISUAL-SAFETY\]|\[MONSTRO-VISUAL\]' 80

echo
echo "=== GAMEPLAY ==="
echo "Playtest READY           : $(count '\[PLAYTEST\] READY')"
echo "Terreno criado           : $(count '\[TERRAIN\] malha visivel criada')"
echo "Normalizacoes visuais    : $(count '\[VISUAL-SAFETY\]')"
echo "Habilidades registradas : $(count '\[ABILITY\]')"
echo "Trocas/loadout          : $(count '\[LOADOUT\]')"
echo "Bag/HUD                 : $(count '\[HUD\] Bag')"
echo "Epochs regenerados      : $(count '\[WORLD\] Novo epoch')"
echo "Recuperacoes de terreno : $(count '\[SAFETY\] queda atraves')"
echo "Acoes de budget de AI   : $(count '\[BUDGET\]')"
last '\[PLAYTEST\]|\[TERRAIN\]|\[VISUAL-SAFETY\]|\[ABILITY\]|\[LOADOUT\]|\[HUD\] Bag|\[WORLD\] Novo epoch|\[SAFETY\] queda atraves|\[BUDGET\]' 100

echo
echo "=== ERROS IMPORTANTES ==="
last 'Fatal error|Log.*Error:|Segmentation fault|GPU crash|device lost|Out of memory|Assertion failed|ensure condition failed' 80

echo
echo "=== AVISOS DE FISICA / INPUT ==="
last 'FindTeleportSpot|EnhancedInput|Unknown Axis|unknown Eixo|unknown.*InputAction|collision|Collision' 60

echo
echo "============================================================"
echo " INTERPRETACAO RAPIDA"
echo "============================================================"

READY=$(count '\[PLAYTEST\] READY')
TERRAIN=$(count '\[TERRAIN\] malha visivel criada')
EPOCHS=$(count '\[WORLD\] Novo epoch')
RECOVERIES=$(count '\[SAFETY\] queda atraves')
FATALS=$(count 'Fatal error|Segmentation fault|GPU crash|device lost|Out of memory|Assertion failed')

if (( FATALS > 0 )); then
    echo "[FALHA] Houve erro fatal/crash no playtest."
elif (( TERRAIN == 0 )); then
    echo "[ATENCAO] O marcador do terreno nao apareceu. Revise o boot/WorldManager acima."
elif (( READY == 0 )); then
    echo "[ATENCAO] O marcador [PLAYTEST] READY nao apareceu. Revise o boot acima."
elif (( EPOCHS > 0 )); then
    echo "[ATENCAO] O mundo foi regenerado $EPOCHS vez(es). Se F10 nao foi pressionado, isso ainda e bug."
elif (( RECOVERIES > 3 )); then
    echo "[ATENCAO] Colisao ainda instavel: $RECOVERIES recuperacoes de emergencia."
else
    echo "[OK] Boot, terreno e loop jogavel confirmados; nenhum crash/reload automatico detectado."
fi
