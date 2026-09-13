#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
FPS_LIMIT=45
RESOLUTION=""
MAX_PARALLEL=3
FULL_DDC=0
FORCE_DDC=0
DDC_BUDGET_MINUTES=12
PROFILE=0

BUILD_LOG="$HOME/nw2-premium-v5-build.log"
DDC_LOG="$HOME/nw2-premium-v5-ddc.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
GPU_LOG="$HOME/nw2-premium-v5-gpu.csv"

usage() {
    cat <<'EOF'
Uso: premium-v5-test-biglinux.sh [opcoes]
  --ue-root PATH          Unreal Engine 5.8 Linux
  --fps N                 FPS interno alvo (padrao 45)
  --resolution LxA        ex. 1600x900; se omitido detecta tela
  --max-parallel N        acoes paralelas UBT (padrao 3)
  --full-ddc              executa DDC -fill opcional antes do jogo
  --force-ddc             ignora fingerprint do ultimo full DDC concluido
  --ddc-budget-minutes N  teto do full DDC (padrao 12 min)
  --profile               habilita estatisticas do runtime

IMPORTANTE:
  O Premium V5 NAO executa full DDC por padrao. O cache ja existente e
  reutilizado e o jogo abre apos build + loading curto de PSO.
EOF
}

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; shift 2 ;;
        --fps) FPS_LIMIT="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        --full-ddc) FULL_DDC=1; shift ;;
        --force-ddc) FORCE_DDC=1; FULL_DDC=1; shift ;;
        --ddc-budget-minutes) DDC_BUDGET_MINUTES="$2"; shift 2 ;;
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

[[ "$FPS_LIMIT" =~ ^[0-9]+$ ]] || fail "FPS invalido: $FPS_LIMIT"
[[ "$MAX_PARALLEL" =~ ^[0-9]+$ ]] || fail "max-parallel invalido: $MAX_PARALLEL"
[[ "$DDC_BUDGET_MINUTES" =~ ^[0-9]+$ ]] || fail "ddc-budget-minutes invalido: $DDC_BUDGET_MINUTES"

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
 NEW WORLD 2 - PREMIUM V5 / FAST BOOT / BIGLINUX
============================================================
Projeto        : $PROJECT_DIR
Branch         : $BRANCH
Commit         : $COMMIT
UE             : $UE_ROOT
Resolucao      : $RESOLUTION
FPS interno    : $FPS_LIMIT
UBT jobs       : $MAX_PARALLEL
Full DDC       : $([[ $FULL_DDC -eq 1 ]] && echo SIM || echo NAO)
DDC budget     : ${DDC_BUDGET_MINUTES} min
Profiler       : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)
Build log      : $BUILD_LOG
DDC log        : $DDC_LOG
Runtime log    : $RUNTIME_LOG
GPU log        : $GPU_LOG
============================================================
EOF

# ---------------------------------------------------------------------------
# 1. PREFLIGHT V5
# ---------------------------------------------------------------------------
echo
echo "[1/8] Validando contratos Premium V5..."
required_files=(
    "Source/NewWorld2/NWNvidiaPerformanceDirector.h"
    "Source/NewWorld2/NWNvidiaPerformanceDirector.cpp"
    "Source/NewWorld2/NWStartupWarmupDirector.h"
    "Source/NewWorld2/NWStartupWarmupDirector.cpp"
    "Source/NewWorld2/NWPremiumSkyDirector.cpp"
    "Source/NewWorld2/NWPremiumHUDDirector.cpp"
    "Source/NewWorld2/NWEnemyVisualDirector.cpp"
    "Source/NewWorld2/NWEnemyAnimationDirector.cpp"
    "Source/NewWorld2/NWPremiumVFXDirector.cpp"
    "scripts/play-biglinux.sh"
)
for file in "${required_files[@]}"; do
    [[ -f "$file" ]] || fail "arquivo V5 ausente: $file"
done

grep -q 'ANWNvidiaPerformanceDirector' Source/NewWorld2/NWGameMode.cpp || fail "NvidiaPerformanceDirector nao ligado ao GameMode"
grep -q 'r.Streamline.DLSSG.Enable' Source/NewWorld2/NWNvidiaPerformanceDirector.cpp || fail "Frame Generation runtime ausente"
grep -q 'r.NGX.DLSS.Enable' Source/NewWorld2/NWNvidiaPerformanceDirector.cpp || fail "DLSS SR runtime ausente"
grep -q 't.Streamline.Reflex.Enable' Source/NewWorld2/NWNvidiaPerformanceDirector.cpp || fail "Reflex runtime ausente"
grep -q 'MaximumLoadingSeconds = 25.0f' Source/NewWorld2/NWStartupWarmupDirector.h || fail "loading gate V5 nao limitado"
grep -q 'r.PSOPrecaching.WaitForHighPriorityRequestsOnly=1' Config/DefaultEngine.ini || fail "PSO prioritario V5 ausente"
grep -q 'HasActiveClothingAssets' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "cloth safety ausente"

echo "OK: contratos V5 presentes."

# ---------------------------------------------------------------------------
# 2. UE / GPU / PLATAFORMA
# ---------------------------------------------------------------------------
echo
echo "[2/8] Validando Unreal Engine 5.8 e GPU..."
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    v = json.load(f)
major = int(v.get("MajorVersion", 0))
minor = int(v.get("MinorVersion", 0))
patch = int(v.get("PatchVersion", 0))
print(f"UE detectada: {major}.{minor}.{patch}")
if major != 5 or minor != 8:
    raise SystemExit("ERRO: Premium V5 foi preparada para Unreal Engine 5.8")
PY

if command -v nvidia-smi >/dev/null 2>&1; then
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true
else
    echo "AVISO: nvidia-smi nao encontrado; monitor de GPU sera pulado."
fi

if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v5-vulkan.txt 2>&1 || fail "Vulkan indisponivel"
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v5-vulkan.txt | head -20 || true
fi

echo
echo "DLSS 4.5 / Frame Generation no teste atual:"
echo "  - o codigo V5 esta pronto para ativar os CVars NVIDIA quando o plugin existir"
echo "  - no Linux nativo, a distribuicao oficial UE do DLSS/Streamline pode nao carregar"
echo "  - nesse caso o runtime registra [RTX-V5] e usa TSR/Vulkan sem quebrar o jogo"

# ---------------------------------------------------------------------------
# 3. PARAR EXECUCOES ANTIGAS E PRESERVAR LOGS
# ---------------------------------------------------------------------------
echo
echo "[3/8] Encerrando execucoes antigas e preservando caches..."
if pgrep -af 'UnrealEditor.*NewWorld2' >/dev/null 2>&1; then
    pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
    sleep 3
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v5-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v5-build-before-$STAMP.log" || true
[[ -s "$DDC_LOG" ]] && cp -a "$DDC_LOG" "$HOME/nw2-premium-v5-ddc-before-$STAMP.log" || true
: > "$BUILD_LOG"
: > "$DDC_LOG"
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
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 220 || true
    exit "$BUILD_RC"
fi

echo "OK: build C++ concluido."

# ---------------------------------------------------------------------------
# 5. DDC OPCIONAL E LIMITADO
# ---------------------------------------------------------------------------
echo
echo "[5/8] Politica de Derived Data Cache V5..."
mkdir -p "$PROJECT_DIR/Saved/NW2V5"
FINGERPRINT_FILE="$PROJECT_DIR/Saved/NW2V5/full-ddc.fingerprint"
CONTENT_FINGERPRINT="$({
    git rev-parse HEAD 2>/dev/null || true
    sha256sum Config/DefaultEngine.ini Config/DefaultGame.ini 2>/dev/null || true
    find Content -type f \( -name '*.uasset' -o -name '*.umap' \) -printf '%P|%s|%T@\n' 2>/dev/null | LC_ALL=C sort
} | sha256sum | awk '{print $1}')"
OLD_FINGERPRINT=""
[[ -f "$FINGERPRINT_FILE" ]] && OLD_FINGERPRINT="$(cat "$FINGERPRINT_FILE" 2>/dev/null || true)"

if (( ! FULL_DDC )); then
    echo "FAST BOOT: full DDC NAO sera executado."
    echo "Reutilizando tudo que ja foi produzido pelo teste anterior e pelo cache local."
else
    if (( ! FORCE_DDC )) && [[ "$CONTENT_FINGERPRINT" == "$OLD_FINGERPRINT" ]]; then
        echo "Full DDC ja foi concluido para este conteudo; pulando."
    else
        command -v timeout >/dev/null 2>&1 || fail "comando timeout nao encontrado"
        echo "Executando full DDC opcional com teto de ${DDC_BUDGET_MINUTES} minuto(s)."
        echo "Se atingir o teto, o teste CONTINUA e aproveita o cache parcial ja gerado."

        set +e
        timeout --foreground --signal=INT --kill-after=30s "${DDC_BUDGET_MINUTES}m" \
            nice -n 5 "$EDITOR" "$PROJECT_FILE" \
            -run=DerivedDataCache -fill \
            -unattended -nop4 -NoSplash -stdout -FullStdOutLogOutput \
            -vulkan -sm6 2>&1 | tee "$DDC_LOG"
        DDC_RC=${PIPESTATUS[0]}
        set -e

        if (( DDC_RC == 0 )); then
            printf '%s\n' "$CONTENT_FINGERPRINT" > "$FINGERPRINT_FILE"
            echo "OK: full DDC concluido dentro do budget."
        elif (( DDC_RC == 124 || DDC_RC == 130 || DDC_RC == 137 )); then
            echo "AVISO: budget do DDC atingido (rc=$DDC_RC). Seguindo para o jogo com cache parcial valido."
        else
            echo "AVISO: DDC retornou rc=$DDC_RC. O V5 nao bloqueia o playtest por falha de prefill."
            grep -a -nE 'Fatal error|LowLevelFatalError|Assertion failed|Error:|Failed' "$DDC_LOG" | tail -n 120 || true
        fi
    fi
fi

# ---------------------------------------------------------------------------
# 6. GPU MONITOR + PLAYTEST
# ---------------------------------------------------------------------------
echo
echo "[6/8] Abrindo Premium V5..."
echo "O loading interno possui teto de aproximadamente 25 segundos."

GPU_MONITOR_PID=""
cleanup_gpu() {
    if [[ -n "$GPU_MONITOR_PID" ]]; then
        kill "$GPU_MONITOR_PID" 2>/dev/null || true
        wait "$GPU_MONITOR_PID" 2>/dev/null || true
        GPU_MONITOR_PID=""
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
trap - EXIT INT TERM

# ---------------------------------------------------------------------------
# 7. DIAGNOSTICO V5
# ---------------------------------------------------------------------------
echo
echo "[7/8] Diagnosticando runtime..."
CHECK_RC=0

if [[ ! -s "$RUNTIME_LOG" ]]; then
    echo "ERRO: runtime log ausente ou vazio."
    CHECK_RC=2
else
    echo "--- RTX / DLSS ---"
    grep -a -E '\[RTX-V5\]' "$RUNTIME_LOG" | tail -n 20 || true

    echo "--- STARTUP ---"
    grep -a -E '\[STARTUP-V4\]|\[PREMIUM-V5\]' "$RUNTIME_LOG" | tail -n 30 || true

    echo "--- HUD / SKY ---"
    grep -a -E '\[HUD-V4\]|\[HUD\]|\[SKY-V4\]' "$RUNTIME_LOG" | tail -n 40 || true

    echo "--- PSO ---"
    grep -a -E 'PSO creation hitches|Precache|PSOPrecache' "$RUNTIME_LOG" | tail -n 40 || true

    if grep -a -qE 'Assertion failed: bPrevious|GPUSkinVertexFactory\.cpp|GetGPUSkinAPEXClothVertexFactory|Signal 11 caught|Fatal error|LowLevelFatalError' "$RUNTIME_LOG"; then
        echo "ERRO: assinatura de crash/fatal detectada."
        grep -a -nE 'Assertion failed: bPrevious|GPUSkinVertexFactory\.cpp|GetGPUSkinAPEXClothVertexFactory|Signal 11 caught|Fatal error|LowLevelFatalError' "$RUNTIME_LOG" | tail -n 120 || true
        CHECK_RC=3
    fi
fi

# ---------------------------------------------------------------------------
# 8. RESUMO
# ---------------------------------------------------------------------------
echo
echo "[8/8] Resumo final..."
GPU_AVG="n/d"
GPU_MAX="n/d"
GPU_MEM_MAX="n/d"
if [[ -s "$GPU_LOG" ]]; then
    read -r GPU_AVG GPU_MAX GPU_MEM_MAX < <(awk -F',' '
        {
            gpu=$2; mem=$4;
            gsub(/[^0-9.]/,"",gpu);
            gsub(/[^0-9.]/,"",mem);
            if (gpu != "") { sum+=gpu; n++; if (gpu>max) max=gpu; }
            if (mem != "" && mem>memmax) memmax=mem;
        }
        END {
            if (n>0) printf "%.1f %.1f %.0f\n", sum/n, max, memmax;
            else print "n/d n/d n/d";
        }' "$GPU_LOG")
fi

cat <<EOF
============================================================
 RESULTADO PREMIUM V5
============================================================
Build exit code : $BUILD_RC
Game exit code  : $GAME_RC
Diagnostico     : $CHECK_RC
GPU media       : ${GPU_AVG}%
GPU pico        : ${GPU_MAX}%
VRAM pico       : ${GPU_MEM_MAX} MiB
Commit          : $COMMIT
Build log       : $BUILD_LOG
DDC log         : $DDC_LOG
Runtime log     : $RUNTIME_LOG
GPU log         : $GPU_LOG
============================================================
EOF

if (( GAME_RC != 0 && GAME_RC != 130 )); then
    tail -n 220 "$RUNTIME_LOG" 2>/dev/null || true
    exit "$GAME_RC"
fi
if (( CHECK_RC >= 2 )); then
    exit "$CHECK_RC"
fi
exit 0
