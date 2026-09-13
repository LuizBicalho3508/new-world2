#!/usr/bin/env bash
set -u

LOG_FILE="${1:-$HOME/nw2-playable.log}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "ERRO: log nao encontrado: $LOG_FILE" >&2
    exit 1
fi

echo "============================================================"
echo " NEW WORLD 2 - DIAGNOSTICO PREMIUM V2"
echo "============================================================"
echo "Log: $LOG_FILE"
echo

count() {
    grep -a -cE "$1" "$LOG_FILE" 2>/dev/null || true
}

last() {
    grep -a -E "$1" "$LOG_FILE" 2>/dev/null | tail -n "${2:-10}" || true
}

echo "=== BOOT / INPUT / PREMIUM ==="
last '\[BOOT\]|\[INPUT\]|\[PREMIUM|\[PLAYTEST\]|\[TERRAIN\]|\[HUD\]|\[RETICLE\]' 100

echo
echo "=== AMBIENTE REALISTA ==="
echo "Diretor de ambiente      : $(count '\[ENV-PREMIUM\] catalogo local')"
echo "Mundo natural pronto     : $(count '\[ENV-PREMIUM\] mundo natural pronto')"
echo "Solo realista aplicado   : $(count '\[ENV-PREMIUM\] solo realista')"
last '\[ENV-PREMIUM\]' 120

echo
echo "=== MOBS / ENCONTRO ==="
echo "Visuais de mob aplicados : $(count '\[MOB-VISUAL\].*->')"
echo "Arena garantida          : $(count '\[ENCOUNTER\].*pront')"
last '\[MOB-VISUAL\]|\[ENCOUNTER\]' 120

echo
echo "=== HABILIDADES / FREE AIM ==="
echo "Casts detectados         : $(count '\[ABILITY-CHECK\].*aceito')"
echo "Dano nativo confirmado  : $(count '\[ABILITY-CHECK\].*confirmou dano')"
echo "Soft-aim aplicado        : $(count '\[ABILITY-ASSIST\]')"
echo "Free-casts registrados   : $(count '\[ABILITY-FREECAST\]')"
echo "Curas/efeitos nativos    : $(count '\[ABILITY\]')"
last '\[ABILITY-CHECK\]|\[ABILITY-ASSIST\]|\[ABILITY-FREECAST\]|\[ABILITY\]' 160

echo
echo "=== LOOP DE GAMEPLAY ==="
echo "Playtest READY           : $(count '\[PLAYTEST\] READY')"
echo "Terreno criado           : $(count '\[TERRAIN\] malha visivel criada')"
echo "HUD construido           : $(count '\[HUD\] CombatHUD')"
echo "Reticulo                 : $(count '\[RETICLE\]')"
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
echo "=== ERROS FATAIS REAIS ==="
# Nao usamos mais o padrao generico Log.*Error: porque plugins/editor podem emitir
# mensagens Error recuperaveis. Ctrl+C/exit 130 tambem nao e crash do jogo.
FATAL_PATTERN='Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|Signal 11|GPU crash|device lost|Out of memory|Assertion failed|ensure condition failed|Unhandled Exception'
last "$FATAL_PATTERN" 100

echo
echo "=== WARNINGS DE REGRESSAO ==="
last 'SetAutoActivate called.*after construction|FindTeleportSpot|Unknown Axis|unknown.*InputAction|VISUAL-SAFETY' 80

echo
echo "============================================================"
echo " INTERPRETACAO AUTOMATICA"
echo "============================================================"

READY=$(count '\[PLAYTEST\] READY')
TERRAIN=$(count '\[TERRAIN\] malha visivel criada')
PREMIUM=$(count '\[PREMIUM-V2\]')
HUD=$(count '\[HUD\] CombatHUD')
RETICLE=$(count '\[RETICLE\]')
ENCOUNTER=$(count '\[ENCOUNTER\].*pront')
MOBS=$(count '\[MOB-VISUAL\].*->')
ENV=$(count '\[ENV-PREMIUM\] mundo natural pronto')
EPOCHS=$(count '\[WORLD\] Novo epoch')
RECOVERIES=$(count '\[SAFETY\] queda atraves')
FATALS=$(count "$FATAL_PATTERN")
AUTO_ACTIVATE=$(count 'SetAutoActivate called.*after construction')
VISUAL_RACE=$(count '\[VISUAL-SAFETY\]')

STATUS=0
if (( FATALS > 0 )); then
    echo "[FALHA] Houve crash/assert/fatal real no runtime."
    STATUS=2
elif (( TERRAIN == 0 || READY == 0 || PREMIUM == 0 )); then
    echo "[FALHA] Boot premium V2 incompleto: terrain=$TERRAIN ready=$READY premium=$PREMIUM."
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
    echo "[OK] Boot premium V2 confirmado; nenhum fatal real encontrado."
fi

if (( HUD == 0 )); then
    echo "[INFO] HUD nao confirmou no log; verifique barras e cooldowns visualmente."
fi
if (( RETICLE == 0 )); then
    echo "[ATENCAO] Reticulo action-RPG nao apareceu no log."
    STATUS=1
fi
if (( ENCOUNTER == 0 )); then
    echo "[ATENCAO] Arena de mobs proxima ao jogador nao foi confirmada."
    STATUS=1
fi
if (( MOBS == 0 )); then
    echo "[ATENCAO] Nenhum skeletal mesh real foi aplicado aos mobs."
    STATUS=1
fi
if (( ENV == 0 )); then
    echo "[ATENCAO] Diretor de ambiente nao confirmou a populacao natural."
    STATUS=1
fi
if (( PSO > 0 )); then
    echo "[INFO] Houve $PSO mensagem(ns) agregada(s) de PSO hitch. Compare a segunda abertura com cache aquecido."
fi
if (( LIVE_NIAGARA > 0 || LIVE_AUDIO > 0 )); then
    echo "[ATENCAO] Houve geracao de apresentacao em runtime: Niagara=$LIVE_NIAGARA Audio=$LIVE_AUDIO."
    STATUS=1
fi

exit "$STATUS"
