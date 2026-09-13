#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
FPS_LIMIT=45
RESOLUTION=""
MAX_PARALLEL=3
PROFILE=0

BUILD_LOG="$HOME/nw2-premium-v6-build.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
GPU_LOG="$HOME/nw2-premium-v6-gpu.csv"

usage() {
cat <<'EOF'
Uso: premium-v6-test-biglinux.sh [opcoes]
  --ue-root PATH
  --fps N
  --resolution LxA
  --max-parallel N
  --profile

V6 usa FAST BOOT por padrao e nunca executa DerivedDataCache -fill completo.
EOF
}

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; shift 2 ;;
        --fps) FPS_LIMIT="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        --profile) PROFILE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
    esac
done

fail() {
    echo "[FALHA] $*" >&2
    echo "Build  : $BUILD_LOG" >&2
    echo "Runtime: $RUNTIME_LOG" >&2
    exit 1
}

[[ -f "$PROJECT_FILE" ]] || fail "uproject ausente"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
[[ -x "$EDITOR" ]] || fail "UnrealEditor ausente: $EDITOR"
[[ -x "$BUILD_SH" ]] || fail "Build.sh ausente: $BUILD_SH"
[[ "$FPS_LIMIT" =~ ^[0-9]+$ ]] || fail "fps invalido"
[[ "$MAX_PARALLEL" =~ ^[0-9]+$ ]] || fail "max-parallel invalido"

if [[ -z "$RESOLUTION" ]]; then
    SCREEN_MODE=""
    command -v xrandr >/dev/null 2>&1 && SCREEN_MODE="$(xrandr --current 2>/dev/null | awk '/\*/ {print $1; exit}')"
    if [[ "$SCREEN_MODE" =~ ^([0-9]+)x([0-9]+)$ ]] && (( BASH_REMATCH[1] >= 1700 )); then RESOLUTION="1600x900"; else RESOLUTION="1280x720"; fi
fi
[[ "$RESOLUTION" =~ ^[0-9]+x[0-9]+$ ]] || fail "resolucao invalida"

cd "$PROJECT_DIR"
git config core.fileMode false || true
BRANCH="$(git branch --show-current 2>/dev/null || true)"
COMMIT="$(git rev-parse --short=12 HEAD 2>/dev/null || echo sem-git)"

cat <<EOF
============================================================
 NEW WORLD 2 - PREMIUM V6 COMPLETE / BIGLINUX
============================================================
Projeto     : $PROJECT_DIR
Branch      : $BRANCH
Commit      : $COMMIT
UE          : $UE_ROOT
Resolucao   : $RESOLUTION
FPS alvo    : $FPS_LIMIT
UBT jobs    : $MAX_PARALLEL
Fast boot   : SIM (sem full DDC)
Build log   : $BUILD_LOG
Runtime log : $RUNTIME_LOG
GPU log     : $GPU_LOG
============================================================
EOF

echo
echo "[1/7] Preflight Premium V6..."
required=(
  Source/NewWorld2/NWCharacterPremiumV6.cpp
  Source/NewWorld2/NWPremiumV6PlayerDirector.cpp
  Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp
  Source/NewWorld2/NWPremiumV6AbilityLibrary.h
  Source/NewWorld2/NWPremiumV6CombatDirector.cpp
  Source/NewWorld2/NWPremiumV6EnvironmentBooster.cpp
  Source/NewWorld2/NWPremiumVFXDirector.cpp
  Source/NewWorld2/NWCombatHUDWidget.cpp
  Source/NewWorld2/NWNvidiaPerformanceDirector.cpp
  scripts/play-biglinux.sh
)
for f in "${required[@]}"; do [[ -f "$f" ]] || fail "arquivo V6 ausente: $f"; done

grep -q '\[PREMIUM-V6\]' Source/NewWorld2/NWGameMode.cpp || fail "bootstrap V6 ausente"
grep -q 'ReplaceAxisMapping' Source/NewWorld2/NWGameMode.cpp || fail "normalizacao de WASD ausente"
grep -q 'IgnoreRootMotion' Source/NewWorld2/NWCharacterPremiumV6.cpp || fail "root-motion safety ausente"
grep -q 'MELHOR' Source/NewWorld2/NWCharacterPremiumV6.cpp || fail "comparacao de itens ausente"
grep -q '\[PLAYER-GEAR-V6\]' Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp || fail "gear visual V6 ausente"
grep -q '\[ABILITY-V6\]' Source/NewWorld2/NWPremiumV6CombatDirector.cpp || fail "skills V6 ausentes"
grep -q '_splinevfx' Source/NewWorld2/NWPremiumVFXDirector.cpp || fail "blocklist VFX V6 ausente"
grep -q '\[ENV-V6\]' Source/NewWorld2/NWPremiumV6EnvironmentBooster.cpp || fail "lush world V6 ausente"
grep -q '\[HUD-V6\]' Source/NewWorld2/NWCombatHUDWidget.cpp || fail "HUD V6 ausente"
grep -q 'r.PSOPrecache.ProxyCreationStrategy=1' Config/DefaultEngine.ini || fail "PSO API UE5.8 ausente"
if grep -q 'ProxyCreationWhenPSOReady\|ProxyCreationDelayStrategy' Config/DefaultEngine.ini; then fail "CVar PSO deprecated voltou"; fi

echo "OK: contratos V6 presentes."

echo
echo "[2/7] Validando UE 5.8 / Vulkan / GPU..."
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json,sys
with open(sys.argv[1], encoding='utf-8') as f: v=json.load(f)
major,minor,patch=int(v.get('MajorVersion',0)),int(v.get('MinorVersion',0)),int(v.get('PatchVersion',0))
print(f'UE detectada: {major}.{minor}.{patch}')
if (major,minor)!=(5,8): raise SystemExit('ERRO: Premium V6 requer UE 5.8')
PY
if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v6-vulkan.txt 2>&1 || fail "Vulkan indisponivel"
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v6-vulkan.txt | head -20 || true
fi
command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true

echo
echo "[3/7] Encerrando instancia antiga e preservando logs/cache..."
pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
sleep 2
STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v6-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v6-build-before-$STAMP.log" || true
: > "$BUILD_LOG"
: > "$GPU_LOG"

echo
echo "[4/7] Build incremental NewWorld2Editor..."
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e
if (( BUILD_RC != 0 )); then
    echo "--- ERROS DE BUILD ---"
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 240 || true
    exit "$BUILD_RC"
fi
echo "OK: build V6 concluido."

echo
echo "[5/7] Abrindo jogo sem DDC full..."
GPU_PID=""
cleanup_gpu() { [[ -n "$GPU_PID" ]] && kill "$GPU_PID" 2>/dev/null || true; }
trap cleanup_gpu EXIT INT TERM
if command -v nvidia-smi >/dev/null 2>&1; then
    (
      while true; do
        nvidia-smi --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used,memory.total,power.draw,clocks.current.graphics --format=csv,noheader,nounits 2>/dev/null || break
        sleep 2
      done
    ) > "$GPU_LOG" & GPU_PID=$!
fi

PLAY_ARGS=(--ue-root "$UE_ROOT" --fps "$FPS_LIMIT" --resolution "$RESOLUTION" --max-parallel "$MAX_PARALLEL" --skip-build)
(( PROFILE )) && PLAY_ARGS+=(--profile)
set +e
bash "$PROJECT_DIR/scripts/play-biglinux.sh" "${PLAY_ARGS[@]}"
GAME_RC=$?
set -e
cleanup_gpu; GPU_PID=""; trap - EXIT INT TERM

echo
echo "[6/7] Diagnostico Premium V6..."
if [[ -s "$RUNTIME_LOG" ]]; then
  echo "--- BOOT / MOVIMENTO ---"
  grep -a -E '\[PREMIUM-V6\]|\[INPUT-V6\]|\[MOVE-V6\]|\[STARTER-V6\]|\[STARTUP-V5\]' "$RUNTIME_LOG" | tail -n 80 || true
  echo "--- HUD / INVENTARIO / GEAR ---"
  grep -a -E '\[HUD-V6\]|\[PLAYER-GEAR-V6\]|\[EQUIP\]|\[ARMA\]' "$RUNTIME_LOG" | tail -n 160 || true
  echo "--- SKILLS / VFX ---"
  grep -a -E '\[ABILITY-V6\]|\[VFX-V6\]|\[ABILITY-CHECK\]|\[ABILITY-ASSIST\]' "$RUNTIME_LOG" | tail -n 220 || true
  echo "--- AMBIENTE / CEU ---"
  grep -a -E '\[ENV-V6\]|\[ENV-PREMIUM\]|\[SKY-V6\]' "$RUNTIME_LOG" | tail -n 120 || true
  echo "--- RTX ---"
  grep -a -E '\[RTX-V5\]' "$RUNTIME_LOG" | tail -n 30 || true
  echo "--- REGRESSOES ---"
  grep -a -E 'Query Mesh Distance Field GPU|_SplineVFX|GPUSkinVertexFactory|APEXCloth|Divide by zero|ProxyCreationWhenPSOReady|ProxyCreationDelayStrategy' "$RUNTIME_LOG" | tail -n 100 || true
  echo "--- PSO ---"
  grep -a -E 'PSO creation hitches|PSOPrecach|ShaderPipelineCache' "$RUNTIME_LOG" | tail -n 100 || true
  echo "--- FATAL ---"
  grep -a -Ei 'Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|GPU crash|device lost|Out of memory|Assertion failed|Unhandled Exception' "$RUNTIME_LOG" | tail -n 100 || true
else
  echo "AVISO: runtime log vazio."
fi

echo
echo "[7/7] Resumo de GPU..."
if [[ -s "$GPU_LOG" ]]; then
  awk -F',' '
  {g=$2+0; m=$4+0; n++; sg+=g; if(g>mg)mg=g; if(m>mm)mm=m}
  END{if(n) printf("GPU media %.1f%% | pico %.0f%% | VRAM pico %.0f MiB | amostras %d\n",sg/n,mg,mm,n); else print "sem amostras"}
  ' "$GPU_LOG"
else
  echo "Monitor GPU indisponivel."
fi

echo
echo "============================================================"
echo " PREMIUM V6 ENCERRADA - RC=$GAME_RC"
echo " Build  : $BUILD_LOG"
echo " Runtime: $RUNTIME_LOG"
echo " GPU    : $GPU_LOG"
echo "============================================================"

# Ctrl+C no editor Linux e encerramento solicitado, nao crash do gameplay.
if (( GAME_RC == 130 )); then exit 0; fi
exit "$GAME_RC"
