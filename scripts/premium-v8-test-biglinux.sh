#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
FPS_LIMIT=60
RESOLUTION=""
MAX_PARALLEL=3
PROFILE=0

BUILD_LOG="$HOME/nw2-premium-v8-build.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
GPU_LOG="$HOME/nw2-premium-v8-gpu.csv"
ASSET_REPORT="$HOME/nw2-premium-v8-assets.txt"

usage() {
cat <<'EOF'
Uso: premium-v8-test-biglinux.sh [opcoes]
  --ue-root PATH          Raiz da Unreal Engine 5.8
  --fps N                 Limite de FPS (padrao: 60)
  --resolution LxA        Resolucao do jogo; auto se omitida
  --max-parallel N        Acoes paralelas UBT (padrao: 3)
  --profile               MOSTRA stat unit/game/gpu/fps sobre a tela
  -h, --help              Ajuda

Por padrao o jogo abre LIMPO, sem o overlay de profiling.
Antes do build a V8 tenta preparar Manny/UEFN Mannequin a partir dos templates
locais da Unreal. Se nao existir, Greystone permanece apenas como fallback.
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
    echo "Build   : $BUILD_LOG" >&2
    echo "Runtime : $RUNTIME_LOG" >&2
    echo "Assets  : $ASSET_REPORT" >&2
    exit 1
}

[[ -f "$PROJECT_FILE" ]] || fail "NewWorld2.uproject ausente em $PROJECT_DIR"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
[[ -x "$EDITOR" ]] || fail "UnrealEditor ausente: $EDITOR"
[[ -x "$BUILD_SH" ]] || fail "Build.sh ausente: $BUILD_SH"
[[ "$FPS_LIMIT" =~ ^[0-9]+$ ]] || fail "fps invalido: $FPS_LIMIT"
[[ "$MAX_PARALLEL" =~ ^[0-9]+$ ]] || fail "max-parallel invalido: $MAX_PARALLEL"

if [[ -z "$RESOLUTION" ]]; then
    SCREEN_MODE=""
    command -v xrandr >/dev/null 2>&1 && SCREEN_MODE="$(xrandr --current 2>/dev/null | awk '/\*/ {print $1; exit}')"
    if [[ "$SCREEN_MODE" =~ ^([0-9]+)x([0-9]+)$ ]] && (( BASH_REMATCH[1] >= 1700 )); then
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
 NEW WORLD 2 - PREMIUM MODULAR PLAYER V8 / BIGLINUX
============================================================
Projeto       : $PROJECT_DIR
Branch        : $BRANCH
Commit        : $COMMIT
UE            : $UE_ROOT
Resolucao     : $RESOLUTION
FPS alvo      : $FPS_LIMIT
UBT jobs      : $MAX_PARALLEL
Overlay stat  : $([[ $PROFILE -eq 1 ]] && echo LIGADO || echo DESLIGADO)
Build log     : $BUILD_LOG
Runtime log   : $RUNTIME_LOG
GPU log       : $GPU_LOG
Assets report : $ASSET_REPORT
============================================================
EOF

printf '\n[1/9] Preflight de codigo V8...\n'
required=(
  Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp
  Source/NewWorld2/NWEnemyAnimationDirector.cpp
  Source/NewWorld2/NWStartupWarmupDirector.cpp
  Source/NewWorld2/NWCombatHUDWidget.cpp
  scripts/play-biglinux.sh
  scripts/prepare-neutral-player-linux.sh
  scripts/verify-fab-assets-linux.sh
)
for f in "${required[@]}"; do [[ -f "$f" ]] || fail "arquivo obrigatorio ausente: $f"; done

grep -q '\[PLAYER-GEAR-V8\]' Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp || fail "gear V8 ausente"
grep -q '\[PLAYER-BASE-V8\]' Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp || fail "corpo neutro V8 ausente"
grep -q 'AreLeaderPoseMeshesCompatible' Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp || fail "compatibilidade modular V8 ausente"
grep -q '\[MOB-ANIM-V8\]' Source/NewWorld2/NWEnemyAnimationDirector.cpp || fail "mob animation V8 ausente"
grep -q '\[STARTUP-V8\]' Source/NewWorld2/NWStartupWarmupDirector.cpp || fail "startup V8 ausente"
grep -q 'HasOutstandingCompilationRequests' Source/NewWorld2/NWStartupWarmupDirector.cpp || fail "controle Niagara V8 ausente"
grep -q '^r.PSOPrecache.ProxyCreationStrategy=1$' Config/DefaultEngine.ini || fail "PSO Strategy atual ausente"
if grep -qE '^r\.PSOPrecache\.(ProxyCreationWhenPSOReady|ProxyCreationDelayStrategy)=|^r\.PSOPrecaching\.WaitForHighPriorityRequestsOnly=' Config/DefaultEngine.ini; then
    fail "CVar PSO deprecated/dummy voltou ao DefaultEngine.ini"
fi

echo "OK: contratos V8 encontrados."

printf '\n[2/9] Validando Unreal Engine / Vulkan / GPU...\n'
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    v = json.load(f)
version = (int(v.get("MajorVersion", 0)), int(v.get("MinorVersion", 0)), int(v.get("PatchVersion", 0)))
print(f"UE detectada: {version[0]}.{version[1]}.{version[2]}")
if version[:2] != (5, 8):
    raise SystemExit("ERRO: Premium V8 foi preparado para UE 5.8")
PY

if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v8-vulkan.txt 2>&1 || fail "Vulkan indisponivel"
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v8-vulkan.txt | head -20 || true
fi
command -v nvidia-smi >/dev/null 2>&1 && \
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true

printf '\n[3/9] Preparando corpo-base neutro...\n'
chmod +x "$PROJECT_DIR/scripts/prepare-neutral-player-linux.sh"
PROJECT_DIR="$PROJECT_DIR" UE_ROOT="$UE_ROOT" bash "$PROJECT_DIR/scripts/prepare-neutral-player-linux.sh" || true

NEUTRAL_MESH="$(find "$PROJECT_DIR/Content" -type f \( -iname 'SKM_Manny*.uasset' -o -iname 'SKM_UEFN_Mannequin*.uasset' -o -iname '*basebody*.uasset' -o -iname '*underwear*.uasset' \) -print -quit 2>/dev/null || true)"
NEUTRAL_ANIM="$(find "$PROJECT_DIR/Content" -type f \( -iname 'ABP_Manny.uasset' -o -iname 'ABP_SandboxCharacter.uasset' \) -print -quit 2>/dev/null || true)"
if [[ -n "$NEUTRAL_MESH" && -n "$NEUTRAL_ANIM" ]]; then
    echo "[V8-PLAYER] OK corpo neutro: $NEUTRAL_MESH"
    echo "[V8-PLAYER] OK animacao base: $NEUTRAL_ANIM"
else
    echo "[V8-PLAYER] AVISO: corpo neutro completo ainda nao foi localizado."
    echo "[V8-PLAYER] Greystone sera mantido como fallback ate Manny/UEFN existir em Content/."
fi

printf '\n[4/9] Auditando Fab/Epic local...\n'
: > "$ASSET_REPORT"
set +e
bash "$PROJECT_DIR/scripts/verify-fab-assets-linux.sh" "$PROJECT_DIR" | tee "$ASSET_REPORT"
ASSET_RC=${PIPESTATUS[0]}
set -e
(( ASSET_RC == 0 )) || echo "AVISO: auditoria retornou RC=$ASSET_RC; build continuara."

printf '\n[5/9] Encerrando instancias antigas e preservando logs...\n'
pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
sleep 2
STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v8-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v8-build-before-$STAMP.log" || true
: > "$BUILD_LOG"
: > "$GPU_LOG"

printf '\n[6/9] Build incremental NewWorld2Editor...\n'
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e
if (( BUILD_RC != 0 )); then
    echo "--- ERROS DE BUILD ---"
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 260 || true
    fail "build Unreal falhou com RC=$BUILD_RC"
fi
echo "OK: build V8 concluido."

printf '\n[7/9] Abrindo jogo...\n'
GPU_PID=""
cleanup_gpu() { [[ -n "$GPU_PID" ]] && kill "$GPU_PID" 2>/dev/null || true; }
trap 'cleanup_gpu' EXIT INT TERM
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
cleanup_gpu
GPU_PID=""

printf '\n[8/9] Diagnostico V8...\n'
if [[ -s "$RUNTIME_LOG" ]]; then
    echo "--- CORPO / EQUIPAMENTO ---"
    grep -a -E '\[PLAYER-BASE-V8\]|\[PLAYER-GEAR-V8\]|\[EQUIP\]|\[ARMA\]|\[LOADOUT\]' "$RUNTIME_LOG" | tail -n 220 || true

    echo "--- STARTUP / PERFORMANCE ---"
    grep -a -E '\[STARTUP-V8\]|PSO creation hitches|PSOPrecach|ShaderPipelineCache|Compiling System NiagaraSystem' "$RUNTIME_LOG" | tail -n 180 || true

    echo "--- INIMIGOS / ANIMACAO ---"
    grep -a -E '\[MOB-VISUAL-V7\]|\[MOB-ANIM-V8\]|\[WORLD BOSS\]' "$RUNTIME_LOG" | tail -n 180 || true

    echo "--- ERROS IMPORTANTES ---"
    grep -a -Ei 'Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|GPU crash|device lost|Out of memory|Assertion failed|Unhandled Exception|LogBlueprint: Error|Divide by zero|Failed to find object|deprecated' "$RUNTIME_LOG" | tail -n 180 || true

    BASE_COUNT="$(grep -ac '\[PLAYER-BASE-V8\] corpo neutro ativo' "$RUNTIME_LOG" || true)"
    ARMOR_COUNT="$(grep -ac '\[PLAYER-GEAR-V8\] armadura VESTIDA' "$RUNTIME_LOG" || true)"
    WEAPON_COUNT="$(grep -ac '\[PLAYER-GEAR-V8\] arma visivel' "$RUNTIME_LOG" || true)"
    STAFF_COUNT="$(grep -ac '\[PLAYER-GEAR-V8\] arma visivel: Cajado Arcano' "$RUNTIME_LOG" || true)"
    STARTUP_TIMEOUTS="$(grep -ac '\[STARTUP-V8\].*motivo=timeout' "$RUNTIME_LOG" || true)"
    MOB_EMPTY="$(grep -a '\[MOB-ANIM-V8\]' "$RUNTIME_LOG" | grep -c 'idle=-.*run=-.*attack=-' || true)"

    echo
    echo "Resumo V8:"
    echo "  corpo neutro ativado                 : $BASE_COUNT"
    echo "  pecas de armadura realmente vestidas : $ARMOR_COUNT"
    echo "  armas visiveis aplicadas              : $WEAPON_COUNT"
    echo "  cajado visual aplicado                : $STAFF_COUNT"
    echo "  startup timeout                        : $STARTUP_TIMEOUTS"
    echo "  familias de mob sem animacao propria  : $MOB_EMPTY"

    if (( BASE_COUNT == 0 )); then
        echo "AVISO: o teste ainda usou fallback. Adicione Third Person/Manny ou Game Animation Sample."
    fi
    if (( ARMOR_COUNT == 0 )); then
        echo "AVISO: nenhuma armadura instalada compartilha a familia de bones do corpo-base atual."
    fi
else
    echo "AVISO: runtime log vazio: $RUNTIME_LOG"
fi

printf '\n[9/9] GPU / encerramento...\n'
if [[ -s "$GPU_LOG" ]]; then
    awk -F',' '
    {g=$2+0; m=$4+0; n++; sg+=g; if(g>mg)mg=g; if(m>mm)mm=m}
    END{if(n) printf("GPU media %.1f%% | pico %.0f%% | VRAM pico %.0f MiB | amostras %d\n",sg/n,mg,mm,n); else print "sem amostras"}
    ' "$GPU_LOG"
fi

trap - EXIT INT TERM

echo
echo "============================================================"
echo " PREMIUM V8 ENCERRADO - GAME RC=$GAME_RC"
echo " Build   : $BUILD_LOG"
echo " Runtime : $RUNTIME_LOG"
echo " GPU     : $GPU_LOG"
echo " Assets  : $ASSET_REPORT"
echo "============================================================"

if (( GAME_RC == 130 )); then exit 0; fi
exit "$GAME_RC"
