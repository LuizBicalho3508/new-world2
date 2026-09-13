#!/usr/bin/env bash
set -u

LOG_FILE="${1:-$HOME/nw2-playable.log}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "ERRO: log nao encontrado: $LOG_FILE" >&2
    exit 1
fi

echo "============================================================"
echo " NEW WORLD 2 - DIAGNOSTICO PREMIUM DO PLAYTEST"
echo "============================================================"
echo "Log: $LOG_FILE"
echo

count() {
    grep -cE "$1" "$LOG_FILE" 2>/dev/null || true
}

last() {
    grep -E "$1" "$LOG_FILE" 2>/dev/null | tail -n "${2:-10}" || true
}

echo "=== BOOT / INPUT / PREMIUM ==="
last '\[BOOT\]|\[INPUT\]|\[PREMIUM\]|\[PLAYTEST\]|\[TERRAIN\]|\[HUD\]' 80

echo
echo "=== VISUAIS / ASSETS ==="
last '\[FAB\] Catalogo|\[PRESENTATION\]|\[VISUAL\]|\[ARMOR-VISUAL\]|\[MONSTRO-VISUAL\]' 100

echo
echo "=== HABILIDADES ==="
echo "Casts detectados         : $(count '\[ABILITY-CHECK\].*aceito')"
echo "Dano nativo confirmado  : $(count '\[ABILITY-CHECK\].*confirmou dano')"
echo "Aim-assist aplicado      : $(count '\[ABILITY-ASSIST\]')"
echo "Miss real                : $(count '\[ABILITY-MISS\]')"
echo "Curas/efeitos nativos    : $(count '\[ABILITY\]')"
last '\[ABILITY-CHECK\]|\[ABILITY-ASSIST\]|\[ABILITY-MISS\]|\[ABILITY\]' 120

echo
echo "=== LOOP DE GAMEPLAY ==="
echo "Playtest READY           : $(count '\[PLAYTEST\] READY')"
echo "Terreno criado           : $(count '\[TERRAIN\] malha visivel criada')"
echo "HUD construido           : $(count '\[HUD\] CombatHUD')"
echo "Trocas/loadout           : $(count '\[LOADOUT\]')"
echo "Bag                      : $(count '\[HUD\] Bag')"
echo "Epochs regenerados       : $(count '\[WORLD\] Novo epoch')"
echo "Recuperacoes de terreno  : $(count '\[SAFETY\] queda atraves')"
echo "Culls por budget de AI   : $(count '\[BUDGET\]')"
echo "Invasoes                 : $(count '\[INVASAO\]')"
last '\[PLAYTEST\]|\[TERRAIN\]|\[LOADOUT\]|\[HUD\] Bag|\[WORLD\] Novo epoch|\[SAFETY\] queda atraves|\[BUDGET\]|\[INVASAO\]' 100

echo
echo "=== STUTTER / PIPELINE ==="
PSO=$(count 'PSO creation hitches')
LIVE_NIAGARA=$(count 'LogNiagara: Compiling System')
LIVE_AUDIO=$(count 'LogAudioDerivedData: Display: Building compressed audio')
echo "Mensagens PSO hitch      : $PSO"
echo "Niagara compilado live   : $LIVE_NIAGARA"
echo "Audio comprimido live    : $LIVE_AUDIO"
last 'PSO creation hitches|LogNiagara: Compiling System|Building compressed audio' 60

echo
echo "=== ERROS IMPORTANTES ==="
FATAL_PATTERN='Fatal error|Log.*Error:|Segmentation fault|GPU crash|device lost|Out of memory|Assertion failed|ensure condition failed'
last "$FATAL_PATTERN" 100

echo
echo "=== WARNINGS DE REGRESSAO ==="
last 'SetAutoActivate called.*after construction|Particle Notify: Particle system is null|FindTeleportSpot|Unknown Axis|unknown.*InputAction|VISUAL-SAFETY' 80

echo
echo "============================================================"
echo " INTERPRETACAO AUTOMATICA"
echo "============================================================"

READY=$(count '\[PLAYTEST\] READY')
TERRAIN=$(count '\[TERRAIN\] malha visivel criada')
PREMIUM=$(count '\[PREMIUM\] gameplay watchdog ativo')
HUD=$(count '\[HUD\] CombatHUD')
EPOCHS=$(count '\[WORLD\] Novo epoch')
RECOVERIES=$(count '\[SAFETY\] queda atraves')
FATALS=$(count "$FATAL_PATTERN")
AUTO_ACTIVATE=$(count 'SetAutoActivate called.*after construction')
VISUAL_RACE=$(count '\[VISUAL-SAFETY\]')

STATUS=0
if (( FATALS > 0 )); then
    echo "[FALHA] Houve erro fatal/crash/assert importante."
    STATUS=2
elif (( TERRAIN == 0 || READY == 0 || PREMIUM == 0 )); then
    echo "[FALHA] Boot premium incompleto: terrain=$TERRAIN ready=$READY premium=$PREMIUM."
    STATUS=2
elif (( EPOCHS > 0 )); then
    echo "[ATENCAO] O mundo regenerou $EPOCHS vez(es). Se F10 nao foi usado, envie o log."
    STATUS=1
elif (( RECOVERIES > 3 )); then
    echo "[ATENCAO] Colisao ainda exigiu $RECOVERIES recuperacoes de emergencia."
    STATUS=1
elif (( AUTO_ACTIVATE > 0 || VISUAL_RACE > 0 )); then
    echo "[ATENCAO] Foi detectado marcador de uma regressao visual antiga."
    STATUS=1
else
    echo "[OK] Boot premium confirmado, sem crash/reload automatico ou corrida visual antiga."
fi

if (( HUD == 0 )); then
    echo "[INFO] O marcador de HUD nao apareceu; confirme visualmente as barras Q/E/R/Vida/Stamina."
fi
if (( PSO > 0 )); then
    echo "[INFO] Ainda houve mensagem de PSO hitch ($PSO). A primeira execucao pode aquecer o cache; compare uma segunda abertura."
fi
if (( LIVE_NIAGARA > 0 || LIVE_AUDIO > 0 )); then
    echo "[ATENCAO] Ainda houve compilacao/geracao de apresentacao em runtime: Niagara=$LIVE_NIAGARA Audio=$LIVE_AUDIO."
    STATUS=1
fi

exit "$STATUS"
