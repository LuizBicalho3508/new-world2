#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
FPS_LIMIT=45
RESOLUTION=""
MAX_PARALLEL=3
FORCE_DDC=0
SKIP_DDC=0
PROFILE=0
BUILD_LOG="$HOME/nw2-premium-v4-build.log"
DDC_LOG="$HOME/nw2-premium-v4-ddc.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
GPU_LOG="$HOME/nw2-premium-v4-gpu.csv"

usage() {
    cat <<'EOF'
Uso: premium-v4-test-biglinux.sh [opcoes]
  --ue-root PATH       Unreal Engine 5.8 Linux
  --fps N              FPS alvo (padrao 45)
  --resolution LxA     ex. 1600x900; se omitido detecta tela
  --max-parallel N     acoes paralelas UBT (padrao 3)
  --force-ddc          refaz o DDC prefill mesmo se o fingerprint nao mudou
  --skip-ddc           pula DDC prefill (somente diagnostico rapido)
  --profile            habilita stat unit/game/gpu/fps no runtime
EOF
}

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; shift 2 ;;
        --fps) FPS_LIMIT="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        --force-ddc) FORCE_DDC=1; shift ;;
        --skip-ddc) SKIP_DDC=1; shift ;;
        --profile) PROFILE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
    esac
done

fail() {
    echo
    echo "[FALHA] $*" >&2
    echo "Build log  : $BUILD_LOG" >&2
    echo "DDC log    : $DDC_LOG" >&2
    echo "Runtime log: $RUNTIME_LOG" >&2
    exit 1
}

[[ -f "$PROJECT_FILE" ]] || fail "uproject ausente: $PROJECT_FILE"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
[[ -x "$EDITOR" ]] || fail "UnrealEditor ausente em $EDITOR"
[[ -x "$BUILD_SH" ]] || fail "Build.sh ausente em $BUILD_SH"

if [[ -z "$RESOLUTION" ]]; then
    SCREEN_MODE=""
    if command -v xrandr >/dev/null 2>&1; then
        SCREEN_MODE="$(xrandr --current 2>/dev/null | awk '/\*/ {print $1; exit}')"
    fi
    if [[ "$SCREEN_MODE" =~ ^([0-9]+)x([0-9]+)$ ]] && (( BASH_REMATCH[1] >= 1700 && BASH_REMATCH[2] >= 950 )); then
        RESOLUTION="1600x900"
    else
        RESOLUTION="1280x720"
    fi
fi
[[ "$RESOLUTION" =~ ^[0-9]+x[0-9]+$ ]] || fail "resolucao invalida: $RESOLUTION"

cd "$PROJECT_DIR"
git config core.fileMode false || true
BRANCH="$(git branch --show-current 2>/dev/null || true)"
COMMIT="$(git rev-parse --short=12 HEAD 2>/dev/null || echo sem-git)"

cat <<EOF
============================================================
 NEW WORLD 2 - PREMIUM V4 / BIGLINUX
============================================================
Projeto    : $PROJECT_DIR
Branch     : $BRANCH
Commit     : $COMMIT
UE         : $UE_ROOT
Resolucao  : $RESOLUTION
FPS        : $FPS_LIMIT
UBT jobs   : $MAX_PARALLEL
DDC prefill: $([[ $SKIP_DDC -eq 1 ]] && echo PULADO || echo ATIVO)
Profiler   : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)
Build log  : $BUILD_LOG
DDC log    : $DDC_LOG
Runtime log: $RUNTIME_LOG
GPU log    : $GPU_LOG
============================================================
EOF

# ---------------------------------------------------------------------------
# 1. PREFLIGHT V4
# ---------------------------------------------------------------------------
echo
echo "[1/8] Validando contratos Premium V4..."
required_files=(
    "Source/NewWorld2/NWStartupLoadingWidget.h"
    "Source/NewWorld2/NWStartupLoadingWidget.cpp"
    "Source/NewWorld2/NWStartupWarmupDirector.h"
    "Source/NewWorld2/NWStartupWarmupDirector.cpp"
    "Source/NewWorld2/NWPremiumSkyDirector.h"
    "Source/NewWorld2/NWPremiumSkyDirector.cpp"
    "Source/NewWorld2/NWPremiumHUDDirector.h"
    "Source/NewWorld2/NWPremiumHUDDirector.cpp"
    "Source/NewWorld2/NWEnemyHealthBarWidget.h"
    "Source/NewWorld2/NWEnemyHealthBarWidget.cpp"
    "Source/NewWorld2/NWEnemyAnimationDirector.cpp"
    "Source/NewWorld2/NWEnemyVisualDirector.cpp"
    "Source/NewWorld2/NWPremiumVFXDirector.cpp"
    "scripts/play-biglinux.sh"
    "scripts/check-premium-v4-log.sh"
)
for file in "${required_files[@]}"; do
    [[ -f "$file" ]] || fail "arquivo V4 ausente: $file"
done

grep -q 'ANWStartupWarmupDirector' Source/NewWorld2/NWGameMode.cpp || fail "startup warmup nao ligado ao GameMode"
grep -q 'ANWPremiumSkyDirector' Source/NewWorld2/NWGameMode.cpp || fail "premium sky nao ligado ao GameMode"
grep -q 'HasActiveClothingAssets' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "cloth safety ausente"
grep -q '/paragonminions/' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "blacklist ParagonMinions ausente"
grep -q '/skins/' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "blacklist de skins cloth ausente"
grep -q 'AddToPlayerScreen(60)' Source/NewWorld2/NWPremiumHUDDirector.cpp || fail "HUD nao esta garantido no PlayerScreen"
grep -q 'SetEnableLightShaftBloom(true)' Source/NewWorld2/NWPremiumSkyDirector.cpp || fail "light shaft bloom ausente"
grep -q 'SetVolumetricFog(true)' Source/NewWorld2/NWPremiumSkyDirector.cpp || fail "volumetric fog ausente"
grep -q 'PremiumVolumetricCloud' Source/NewWorld2/NWPremiumSkyDirector.cpp || fail "volumetric clouds ausentes"
grep -q 'r.ShaderPipelineCache.StartupMode=1' Config/DefaultEngine.ini || fail "PSO StartupMode Fast ausente"
grep -q 'r.AntiAliasingMethod=4' Config/DefaultEngine.ini || fail "TSR V4 ausente"
grep -q 'bShareMaterialShaderCode=True' Config/DefaultGame.ini || fail "shared shader code ausente"

echo "OK: contratos V4 presentes."

# ---------------------------------------------------------------------------
# 2. UE / VULKAN / HARDWARE
# ---------------------------------------------------------------------------
echo
echo "[2/8] Validando Unreal Engine 5.8 e hardware..."
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json, sys
p = sys.argv[1]
with open(p, encoding="utf-8") as f:
    v = json.load(f)
major = int(v.get("MajorVersion", 0))
minor = int(v.get("MinorVersion", 0))
patch = int(v.get("PatchVersion", 0))
print(f"UE detectada: {major}.{minor}.{patch}")
if major != 5 or minor != 8:
    raise SystemExit("ERRO: Premium V4 foi validada para Unreal Engine 5.8")
PY

if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v4-vulkan.txt 2>&1 || {
        cat /tmp/nw2-v4-vulkan.txt >&2 || true
        fail "Vulkan indisponivel"
    }
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v4-vulkan.txt | head -20 || true
fi

if command -v nvidia-smi >/dev/null 2>&1; then
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true
fi

# ---------------------------------------------------------------------------
# 3. ENCERRAR INSTANCIA E PRESERVAR LOGS
# ---------------------------------------------------------------------------
echo
echo "[3/8] Preparando rodada limpa sem apagar caches..."
if pgrep -af 'UnrealEditor.*NewWorld2' >/dev/null 2>&1; then
    pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
    sleep 3
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v4-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v4-build-before-$STAMP.log" || true
[[ -s "$DDC_LOG" ]] && cp -a "$DDC_LOG" "$HOME/nw2-premium-v4-ddc-before-$STAMP.log" || true
: > "$BUILD_LOG"
: > "$GPU_LOG"

# ---------------------------------------------------------------------------
# 4. BUILD C++
# ---------------------------------------------------------------------------
echo
echo "[4/8] Compilando NewWorld2Editor..."
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

if (( BUILD_RC != 0 )); then
    echo
    echo "=== ERROS DE BUILD ==="
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 220 || true
    exit "$BUILD_RC"
fi

echo "OK: build C++ concluido."

# ---------------------------------------------------------------------------
# 5. DDC PREFILL ANTES DO JOGO
# ---------------------------------------------------------------------------
echo
echo "[5/8] Verificando Derived Data Cache..."
mkdir -p "$PROJECT_DIR/Saved/NW2V4"
FINGERPRINT_FILE="$PROJECT_DIR/Saved/NW2V4/ddc-content.fingerprint"

CONTENT_FINGERPRINT="$({
    git rev-parse HEAD 2>/dev/null || true
    sha256sum Config/DefaultEngine.ini Config/DefaultGame.ini 2>/dev/null || true
    find Content -type f \( -name '*.uasset' -o -name '*.umap' \) -printf '%P|%s|%T@\n' 2>/dev/null | LC_ALL=C sort
} | sha256sum | awk '{print $1}')"

OLD_FINGERPRINT=""
[[ -f "$FINGERPRINT_FILE" ]] && OLD_FINGERPRINT="$(cat "$FINGERPRINT_FILE" 2>/dev/null || true)"

NEED_DDC=1
if (( SKIP_DDC )); then
    NEED_DDC=0
    echo "DDC prefill pulado por --skip-ddc."
elif (( ! FORCE_DDC )) && [[ "$CONTENT_FINGERPRINT" == "$OLD_FINGERPRINT" ]]; then
    NEED_DDC=0
    echo "DDC ja aquecido para este codigo/conteudo: $CONTENT_FINGERPRINT"
fi

if (( NEED_DDC )); then
    echo "Primeira rodada V4/conteudo alterado: preenchendo DDC ANTES de abrir o jogo."
    echo "Isso pode usar bastante CPU e levar varios minutos no i7-2600S; e proposital."
    echo "Durante esta etapa o jogo ainda nao esta aberto."
    : > "$DDC_LOG"

    set +e
    nice -n 5 "$EDITOR" "$PROJECT_FILE" \
        -run=DerivedDataCache -fill -DDC=CreatePak \
        -unattended -nop4 -NoSplash -stdout -FullStdOutLogOutput \
        -vulkan -sm6 2>&1 | tee "$DDC_LOG"
    DDC_RC=${PIPESTATUS[0]}
    set -e

    if (( DDC_RC != 0 )); then
        echo
        echo "=== DDC PREFILL FALHOU ==="
        grep -a -nE 'Fatal error|LowLevelFatalError|Assertion failed|Error:|Failed' "$DDC_LOG" | tail -n 240 || true
        fail "DDC prefill retornou $DDC_RC; nao abriremos o jogo com cache incompleto nesta rodada"
    fi

    printf '%s\n' "$CONTENT_FINGERPRINT" > "$FINGERPRINT_FILE"
    echo "OK: DDC preenchido e fingerprint salvo."
else
    : > "$DDC_LOG"
    echo "OK: reutilizando DDC existente."
fi

# ---------------------------------------------------------------------------
# 6. MONITOR GPU + PLAYTEST
# ---------------------------------------------------------------------------
echo
echo "[6/8] Abrindo Premium V4..."
echo "A tela NEW WORLD 2 segura o controle enquanto o lote inicial de PSOs termina."
echo "Depois dela: confira HUD, raios de sol/nuvens e Q/E/R."

GPU_MONITOR_PID=""
cleanup_gpu() {
    if [[ -n "$GPU_MONITOR_PID" ]]; then
        kill "$GPU_MONITOR_PID" 2>/dev/null || true
        wait "$GPU_MONITOR_PID" 2>/dev/null || true
    fi
}
trap cleanup_gpu EXIT INT TERM

if command -v nvidia-smi >/dev/null 2>&1; then
    (
        while true; do
            nvidia-smi \
                --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used,memory.total,power.draw,clocks.current.graphics \
                --format=csv,noheader,nounits 2>/dev/null || break
            sleep 2
        done
    ) > "$GPU_LOG" &
    GPU_MONITOR_PID=$!
fi

PLAY_ARGS=(
    --ue-root "$UE_ROOT"
    --fps "$FPS_LIMIT"
    --resolution "$RESOLUTION"
    --max-parallel "$MAX_PARALLEL"
    --skip-build
)
(( PROFILE )) && PLAY_ARGS+=(--profile)

set +e
bash "$PROJECT_DIR/scripts/play-biglinux.sh" "${PLAY_ARGS[@]}"
GAME_RC=$?
set -e

cleanup_gpu
GPU_MONITOR_PID=""
trap - EXIT INT TERM

# ---------------------------------------------------------------------------
# 7. DIAGNOSTICO
# ---------------------------------------------------------------------------
echo
echo "[7/8] Diagnosticando Premium V4..."
CHECK_RC=0
bash "$PROJECT_DIR/scripts/check-premium-v4-log.sh" "$RUNTIME_LOG" "$GPU_LOG" || CHECK_RC=$?

# ---------------------------------------------------------------------------
# 8. RESUMO DE GPU E RESULTADO
# ---------------------------------------------------------------------------
echo
echo "[8/8] Resumo final..."
GPU_AVG="n/d"
GPU_MAX="n/d"
if [[ -s "$GPU_LOG" ]]; then
    read -r GPU_AVG GPU_MAX < <(awk -F',' '
        {
            v=$2; gsub(/[^0-9.]/,"",v);
            if (v != "") { sum+=v; n++; if (v>max) max=v; }
        }
        END {
            if (n>0) printf "%.1f %.1f\n", sum/n, max;
            else print "n/d n/d";
        }' "$GPU_LOG")
fi

cat <<EOF
============================================================
 RESULTADO PREMIUM V4
============================================================
Build exit code : $BUILD_RC
Game exit code  : $GAME_RC
Diagnostico     : $CHECK_RC
GPU media       : ${GPU_AVG}%
GPU pico        : ${GPU_MAX}%
Commit          : $COMMIT
Build log       : $BUILD_LOG
DDC log         : $DDC_LOG
Runtime log     : $RUNTIME_LOG
GPU log         : $GPU_LOG
============================================================
EOF

if (( GAME_RC != 0 && GAME_RC != 130 )); then
    echo "Runtime retornou erro real: $GAME_RC"
    tail -n 240 "$RUNTIME_LOG" 2>/dev/null || true
    exit "$GAME_RC"
fi
if (( CHECK_RC >= 2 )); then
    exit "$CHECK_RC"
fi
exit 0
