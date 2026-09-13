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
BUILD_LOG="$HOME/nw2-premium-v3-build.log"
RUNTIME_LOG="$HOME/nw2-playable.log"

usage() {
    cat <<'EOF'
Uso: premium-v3-test-biglinux.sh [opcoes]
  --ue-root PATH       Unreal Engine 5.8 Linux
  --fps N              FPS alvo (padrao 45)
  --resolution LxA     ex. 1600x900; se omitido detecta tela
  --max-parallel N     acoes paralelas UBT (padrao 3)
  --profile            habilita stat unit/game/gpu/fps
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
    echo
    echo "[FALHA] $*" >&2
    echo "Build log  : $BUILD_LOG" >&2
    echo "Runtime log: $RUNTIME_LOG" >&2
    exit 1
}

[[ -f "$PROJECT_FILE" ]] || fail "uproject ausente: $PROJECT_FILE"
[[ -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]] || fail "UnrealEditor ausente em $UE_ROOT"
[[ -x "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ]] || fail "Build.sh ausente em $UE_ROOT"

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
 NEW WORLD 2 - PREMIUM V3 / BIGLINUX
============================================================
Projeto    : $PROJECT_DIR
Branch     : $BRANCH
Commit     : $COMMIT
UE         : $UE_ROOT
Resolucao  : $RESOLUTION
FPS        : $FPS_LIMIT
UBT jobs   : $MAX_PARALLEL
Profiler   : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)
Build log  : $BUILD_LOG
Runtime log: $RUNTIME_LOG
============================================================
EOF

# ---------------------------------------------------------------------------
# 1. PREFLIGHT DE CONTRATO V3
# ---------------------------------------------------------------------------
echo
echo "[1/7] Validando contratos Premium V3..."

required_files=(
    "Source/NewWorld2/NWEnemyHealthBarWidget.h"
    "Source/NewWorld2/NWEnemyHealthBarWidget.cpp"
    "Source/NewWorld2/NWPremiumVFXDirector.h"
    "Source/NewWorld2/NWPremiumVFXDirector.cpp"
    "Source/NewWorld2/NWEnemyVisualDirector.cpp"
    "Source/NewWorld2/NWPremiumGameplayDirector.cpp"
    "Source/NewWorld2/NWPremiumEnvironmentDirector.cpp"
    "scripts/play-biglinux.sh"
    "scripts/check-premium-v3-log.sh"
)
for file in "${required_files[@]}"; do
    [[ -f "$file" ]] || fail "arquivo V3 ausente: $file"
done

grep -q 'ANWPremiumVFXDirector' Source/NewWorld2/NWGameMode.cpp || fail "PremiumVFXDirector nao ligado ao GameMode"
grep -q 'UWidgetComponent' Source/NewWorld2/NWEnemy.cpp || fail "barra flutuante de mob nao esta ligada ao inimigo"
grep -q 'MaxSingleHitFraction' Source/NewWorld2/NWEnemy.cpp || fail "anti-one-shot nao encontrado"
grep -q 'MOB-HP' Source/NewWorld2/NWEnemy.cpp || fail "telemetria de HP dos mobs ausente"
grep -q 'SetColorParameterValueOnMaterials' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "variacao visual V3 de mob ausente"
grep -q 'IsUnsafeRuntimeAssetPath' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "blacklist de packs demo quebrados ausente"
grep -q 'free_magic/demo' Source/NewWorld2/NWPremiumVFXDirector.cpp || fail "filtro Free_Magic Demo ausente"
grep -q 'WarmupCachedSystems' Source/NewWorld2/NWPremiumVFXDirector.cpp || fail "warm-up VFX ausente"
grep -q 'LayersPerCast = 3' Source/NewWorld2/NWPremiumVFXDirector.h || fail "camadas premium de VFX nao configuradas"
grep -q 'SoftAimRadius = 380.0f' Source/NewWorld2/NWPremiumGameplayDirector.h || fail "soft aim V3 nao aplicado"
grep -q 'r.PSOPrecache.Validation=1' Config/DefaultEngine.ini || fail "validacao PSO V3 ausente"
grep -q 'ReplaceActionMapping(Settings, TEXT("Crouch")' Source/NewWorld2/NWGameMode.cpp || fail "compatibilidade Crouch do AnimStarterPack ausente"

echo "OK: contratos Premium V3 presentes."

# ---------------------------------------------------------------------------
# 2. UE 5.8 / VULKAN
# ---------------------------------------------------------------------------
echo
echo "[2/7] Validando Unreal Engine 5.8 e Vulkan..."
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
    raise SystemExit("ERRO: Premium V3 exige Unreal Engine 5.8")
PY

if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v3-vulkan.txt 2>&1 || {
        cat /tmp/nw2-v3-vulkan.txt >&2 || true
        fail "Vulkan indisponivel"
    }
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v3-vulkan.txt | head -20 || true
fi

# ---------------------------------------------------------------------------
# 3. ISOLAR LOGS / PRESERVAR CACHE
# ---------------------------------------------------------------------------
echo
echo "[3/7] Preparando rodada limpa sem apagar DDC/PSO..."
if pgrep -af 'UnrealEditor.*NewWorld2' >/dev/null 2>&1; then
    echo "Encerrando instancia anterior de NewWorld2..."
    pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
    sleep 2
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v3-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v3-build-$STAMP.log" || true
: > "$BUILD_LOG"

# Nao removemos DerivedDataCache, shader cache ou Content/Fab. O objetivo e medir
# se o warm-up/PSO cache reduz o stutter entre a primeira e a segunda abertura.

# ---------------------------------------------------------------------------
# 4. BUILD REAL
# ---------------------------------------------------------------------------
echo
echo "[4/7] Compilando NewWorld2Editor Premium V3..."
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

if (( BUILD_RC != 0 )); then
    echo
    echo "============================================================"
    echo " BUILD V3 FALHOU - GAME NAO SERA ABERTO"
    echo "============================================================"
    echo "Exit code: $BUILD_RC"
    echo
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 200 || true
    exit "$BUILD_RC"
fi

echo "OK: build Premium V3 concluido."

# ---------------------------------------------------------------------------
# 5. PLAYTEST
# ---------------------------------------------------------------------------
echo
echo "[5/7] Abrindo jogo..."
echo "Primeiros 10-30s podem incluir aquecimento de shaders/PSOs. Depois teste Q/E/R varias vezes."
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

# ---------------------------------------------------------------------------
# 6. DIAGNOSTICO V3
# ---------------------------------------------------------------------------
echo
echo "[6/7] Diagnosticando Premium V3..."
CHECK_RC=0
bash "$PROJECT_DIR/scripts/check-premium-v3-log.sh" "$RUNTIME_LOG" || CHECK_RC=$?

# ---------------------------------------------------------------------------
# 7. RESUMO
# ---------------------------------------------------------------------------
echo
echo "[7/7] Resumo da rodada..."
cat <<EOF
============================================================
 RESULTADO PREMIUM V3
============================================================
Build exit code: $BUILD_RC
Game exit code : $GAME_RC
Diagnostico    : $CHECK_RC
Commit         : $COMMIT
Build log      : $BUILD_LOG
Runtime log    : $RUNTIME_LOG
============================================================
EOF

# Ctrl+C/WM pode retornar 130; fatal real e responsabilidade do checker.
if (( GAME_RC != 0 && GAME_RC != 130 )); then
    echo "Runtime retornou erro real: $GAME_RC"
    tail -n 220 "$RUNTIME_LOG" 2>/dev/null || true
    exit "$GAME_RC"
fi
if (( CHECK_RC >= 2 )); then
    exit "$CHECK_RC"
fi
exit 0
