#!/usr/bin/env bash
set -u

LOG_FILE="${1:-$HOME/nw2-playable.log}"
GPU_LOG="${2:-$HOME/nw2-premium-v4-gpu.csv}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "ERRO: log nao encontrado: $LOG_FILE" >&2
    exit 2
fi

count() {
    grep -a -cE "$1" "$LOG_FILE" 2>/dev/null || true
}

last() {
    grep -a -E "$1" "$LOG_FILE" 2>/dev/null | tail -n "${2:-80}" || true
}

FATAL_PATTERN='Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|Signal 11|GPU crash|device lost|Out of memory|Assertion failed|Unhandled Exception'
CLOTH_PATTERN='GPUSkinAPEXCloth|Recreating Clothing Actors|CLOTH-SAFETY-V4.*BLOQUEADO'

printf '%s\n' "============================================================"
printf '%s\n' " NEW WORLD 2 - DIAGNOSTICO PREMIUM V4"
printf '%s\n' "============================================================"
printf 'Runtime: %s\n' "$LOG_FILE"
printf 'GPU log: %s\n\n' "$GPU_LOG"

printf '%s\n' "=== STARTUP / PREWARM ==="
printf 'Bootstrap V4            : %s\n' "$(count '\[PREMIUM-V4\]')"
printf 'Loading gate iniciou    : %s\n' "$(count '\[STARTUP-V4\] loading gate ativo')"
printf 'Loading gate liberou    : %s\n' "$(count '\[STARTUP-V4\] jogo liberado')"
last '\[STARTUP-V4\]|PSO PRECACHING|LogPSOHitching' 100

printf '\n%s\n' "=== HUD ==="
printf 'HUD construído          : %s\n' "$(count '\[HUD-V4\] HUD completo construido')"
printf 'HUD no PlayerScreen     : %s\n' "$(count '\[HUD-V4\] HUD fixado no PlayerScreen')"
last '\[HUD-V4\]|\[HUD\]' 80

printf '\n%s\n' "=== SOL / CEU ==="
printf 'Sky premium             : %s\n' "$(count '\[SKY-V4\] sol premium ativo')"
printf 'Cloud material          : %s\n' "$(count '\[SKY-V4\] nuvens volumetricas ativas')"
last '\[SKY-V4\]' 80

printf '\n%s\n' "=== MOBS / CLOTH SAFETY ==="
printf 'Mobs V4 apresentados    : %s\n' "$(count '\[MOB-VISUAL-V4\].*cloth=NAO')"
printf 'Cloth bloqueado         : %s\n' "$(count '\[CLOTH-SAFETY-V4\]')"
printf 'APEX cloth em runtime   : %s\n' "$(count 'GPUSkinAPEXCloth')"
printf 'Recreate clothing       : %s\n' "$(count 'Recreating Clothing Actors')"
last '\[MOB-VISUAL-V4\]|\[CLOTH-SAFETY-V4\]|GPUSkinAPEXCloth|Recreating Clothing Actors' 120

printf '\n%s\n' "=== DDC / TEXTURAS DURANTE JOGO ==="
TEXTURE_BUILDS=$(count 'LogTexture: Display: Building texture')
printf 'Builds de textura       : %s\n' "$TEXTURE_BUILDS"
last 'LogTexture: Display: Building texture|LogShaderCompilers|ShaderCompileWorker' 60

printf '\n%s\n' "=== PSO ==="
PSO_MESSAGES=$(count 'PSO creation hitches')
ZERO_PRECACHE=$(count '0 of them were precached')
printf 'Mensagens de hitch      : %s\n' "$PSO_MESSAGES"
printf 'Marcadores 0 precached  : %s\n' "$ZERO_PRECACHE"
last 'PSO creation hitches|PSOPrecache|ShaderPipelineCache' 80

printf '\n%s\n' "=== COMBATE ==="
printf 'Casts aceitos           : %s\n' "$(count '\[ABILITY-CHECK\].*aceito')"
printf 'Soft aim                : %s\n' "$(count '\[ABILITY-ASSIST\]')"
printf 'VFX V3/V4               : %s\n' "$(count '\[VFX-V3\]')"
printf 'MOB HP                  : %s\n' "$(count '\[MOB-HP\]')"
last '\[ABILITY-CHECK\]|\[ABILITY-ASSIST\]|\[VFX-V3\]|\[MOB-HP\]' 100

printf '\n%s\n' "=== GPU ==="
if [[ -s "$GPU_LOG" ]]; then
    tail -n 30 "$GPU_LOG" || true
else
    echo "nvidia-smi monitor nao ficou disponivel nesta rodada."
fi

printf '\n%s\n' "=== FATAL REAL ==="
FATALS=$(count "$FATAL_PATTERN")
last "$FATAL_PATTERN" 100

STATUS=0
if (( FATALS > 0 )); then
    echo "[FALHA] Runtime registrou fatal/assert/crash."
    STATUS=2
elif (( $(count '\[PREMIUM-V4\]') == 0 || $(count '\[STARTUP-V4\] jogo liberado') == 0 )); then
    echo "[FALHA] Bootstrap/loading gate V4 nao foi concluido."
    STATUS=2
elif (( $(count '\[HUD-V4\] HUD fixado no PlayerScreen') == 0 )); then
    echo "[ATENCAO] HUD V4 nao confirmou fixacao no PlayerScreen."
    STATUS=1
elif (( $(count '\[SKY-V4\] sol premium ativo') == 0 )); then
    echo "[ATENCAO] Sky V4 nao confirmou configuracao."
    STATUS=1
elif (( $(count 'GPUSkinAPEXCloth') > 0 || $(count 'Recreating Clothing Actors') > 0 )); then
    echo "[ATENCAO] Caminho de cloth reapareceu no runtime."
    STATUS=1
else
    echo "[OK] Estrutura Premium V4 confirmada sem fatal real."
fi

if (( TEXTURE_BUILDS > 0 )); then
    echo "[INFO] Ainda houve $TEXTURE_BUILDS build(s) de textura nesta abertura. Rode novamente: o DDC/PSO deve estar mais quente na segunda passagem."
fi
if (( ZERO_PRECACHE > 0 )); then
    echo "[INFO] Ainda existem PSOs de primeira utilizacao; o cache de desenvolvimento continuara aprendendo nesta rodada."
fi

exit "$STATUS"
