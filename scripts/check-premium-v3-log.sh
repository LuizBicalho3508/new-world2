#!/usr/bin/env bash
set -u

LOG_FILE="${1:-$HOME/nw2-playable.log}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "ERRO: log nao encontrado: $LOG_FILE" >&2
    exit 2
fi

count() {
    grep -a -cE "$1" "$LOG_FILE" 2>/dev/null || true
}

last() {
    grep -a -E "$1" "$LOG_FILE" 2>/dev/null | tail -n "${2:-20}" || true
}

FATAL_PATTERN='Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|Signal 11|GPU crash|device lost|Out of memory|Assertion failed|Unhandled Exception'

echo "============================================================"
echo " NEW WORLD 2 - PREMIUM V3 / DIAGNOSTICO"
echo "============================================================"
echo "Log: $LOG_FILE"
echo

echo "=== BOOT V3 ==="
last '\[PREMIUM-V3\]|\[BOOT\]|\[INPUT\]|\[RETICLE\]|\[PLAYTEST\]' 100

echo
echo "=== AMBIENTE ==="
echo "Mundo natural pronto : $(count '\[ENV-PREMIUM\] mundo natural pronto')"
echo "Solo realista        : $(count '\[ENV-PREMIUM\] solo realista')"
last '\[ENV-PREMIUM\]' 80

echo
echo "=== MOBS / HP ==="
echo "Visuais V3 aplicados : $(count '\[MOB-VISUAL-V3\].*->')"
echo "Hits com HP logado   : $(count '\[MOB-HP\]')"
echo "Arena de 5 mobs      : $(count '\[PLAYTEST\] READY.*V3|\[ENCOUNTER\].*pront')"
last '\[MOB-VISUAL-V3\]|\[MOB-HP\]|\[PLAYTEST-MOB\]|\[ENCOUNTER\]' 180

echo
echo "=== HABILIDADES / VFX ==="
echo "Casts detectados     : $(count '\[ABILITY-CHECK\].*aceito')"
echo "Soft aim aplicado    : $(count '\[ABILITY-ASSIST\]')"
echo "Free casts           : $(count '\[ABILITY-FREECAST\]')"
echo "VFX V3 casts         : $(count '\[VFX-V3\].*slot=')"
echo "VFX warmup           : $(count '\[VFX-V3\] warm-up')"
last '\[ABILITY-CHECK\]|\[ABILITY-ASSIST\]|\[ABILITY-FREECAST\]|\[VFX-V3\]' 220

echo
echo "=== PIPELINE / STUTTER ==="
PSO=$(count 'PSO creation hitches')
LIVE_NIAGARA=$(count 'LogNiagara: Compiling System')
LIVE_AUDIO=$(count 'Building compressed audio')
echo "Mensagens PSO hitch  : $PSO"
echo "Niagara live compile : $LIVE_NIAGARA"
echo "Audio live build     : $LIVE_AUDIO"
last 'PSO creation hitches|LogNiagara: Compiling System|Building compressed audio|PSO.*precache' 100

echo
echo "=== ASSETS/ANIMACOES QUE DEVEM TER SUMIDO ==="
DIVZERO=$(count 'Divide by zero')
FREE_MAGIC_MISSING=$(count 'Free_Magic/Demo.*dependent package|Free_Magic/Demo.*was not available')
ASP_WARN=$(count 'AnimStarterPack.*missing NodeGuid')
OLD_MOB_OWNER=$(count '\[MONSTRO-VISUAL\]')
echo "Divide by zero       : $DIVZERO"
echo "Free_Magic quebrado  : $FREE_MAGIC_MISSING"
echo "AnimStarter upgrade  : $ASP_WARN"
echo "Visual mob legado    : $OLD_MOB_OWNER"
last 'Divide by zero|Free_Magic/Demo.*(dependent package|was not available)|AnimStarterPack.*missing NodeGuid|\[MONSTRO-VISUAL\]' 80

echo
echo "=== FATAIS ==="
FATALS=$(count "$FATAL_PATTERN")
last "$FATAL_PATTERN" 100

echo
echo "============================================================"
echo " INTERPRETACAO PREMIUM V3"
echo "============================================================"

PREMIUM=$(count '\[PREMIUM-V3\]')
READY=$(count '\[PLAYTEST\] READY')
ENV=$(count '\[ENV-PREMIUM\] mundo natural pronto')
RETICLE=$(count '\[RETICLE\]')
VFX_BOOT=$(count '\[VFX-V3\] diretor premium ativo')
MOB_VISUAL=$(count '\[MOB-VISUAL-V3\].*->')

STATUS=0
if (( FATALS > 0 )); then
    echo "[FALHA] Fatal real encontrado no runtime."
    STATUS=2
elif (( PREMIUM == 0 || READY == 0 || ENV == 0 || RETICLE == 0 )); then
    echo "[FALHA] Boot V3 incompleto: premium=$PREMIUM ready=$READY env=$ENV reticle=$RETICLE"
    STATUS=2
else
    echo "[OK] Boot Premium V3 completo e sem fatal real."
fi

if (( VFX_BOOT == 0 )); then
    echo "[ATENCAO] PremiumVFXDirector nao confirmou inicializacao."
    STATUS=$(( STATUS < 1 ? 1 : STATUS ))
fi
if (( MOB_VISUAL == 0 )); then
    echo "[ATENCAO] Nenhum visual V3 de mob foi aplicado."
    STATUS=$(( STATUS < 1 ? 1 : STATUS ))
fi
if (( DIVZERO > 0 )); then
    echo "[ATENCAO] Ainda houve $DIVZERO divide-by-zero em animacao."
    STATUS=$(( STATUS < 1 ? 1 : STATUS ))
fi
if (( FREE_MAGIC_MISSING > 0 )); then
    echo "[ATENCAO] Pack Free_Magic Demo ainda puxou dependencias ausentes."
    STATUS=$(( STATUS < 1 ? 1 : STATUS ))
fi
if (( OLD_MOB_OWNER > 0 )); then
    echo "[ATENCAO] Sistema legado de visual de mob ainda executou $OLD_MOB_OWNER vez(es)."
    STATUS=$(( STATUS < 1 ? 1 : STATUS ))
fi
if (( LIVE_NIAGARA > 0 )); then
    echo "[INFO] Niagara compilou $LIVE_NIAGARA sistema(s) nesta abertura; compare a segunda execucao com cache aquecido."
fi
if (( PSO > 0 )); then
    echo "[INFO] O renderer reportou $PSO marcador(es) agregado(s) de PSO hitch. O V3 registra/aquece efeitos antes do combate; compare o segundo boot."
fi

exit "$STATUS"
