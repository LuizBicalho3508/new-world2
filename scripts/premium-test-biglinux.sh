#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
FPS_LIMIT=45
RESOLUTION=""
PROFILE=0
MAX_PARALLEL=3
LOG_FILE="$HOME/nw2-playable.log"
BUILD_LOG="$HOME/nw2-premium-build.log"

usage() {
    cat <<'EOF'
Uso: premium-test-biglinux.sh [opcoes]
  --ue-root PATH          Unreal Engine 5.8 Linux
  --fps N                 limite de FPS (padrao 45)
  --resolution LxA        ex.: 1600x900; se omitido detecta a tela
  --profile               abre stat unit/game/gpu/fps
  --max-parallel N        paralelismo do UBT (padrao 3)
EOF
}

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; shift 2 ;;
        --fps) FPS_LIMIT="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --profile) PROFILE=1; shift ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
    esac
done

fail() {
    echo
    echo "[FALHA] $*" >&2
    echo "Build log  : $BUILD_LOG" >&2
    echo "Runtime log: $LOG_FILE" >&2
    exit 1
}

[[ -f "$PROJECT_FILE" ]] || fail "NewWorld2.uproject nao encontrado em $PROJECT_DIR"
[[ -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]] || fail "UnrealEditor nao encontrado em $UE_ROOT"
[[ -x "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ]] || fail "Build.sh nao encontrado em $UE_ROOT"

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

[[ "$RESOLUTION" =~ ^[0-9]+x[0-9]+$ ]] || fail "Resolucao invalida: $RESOLUTION"

cd "$PROJECT_DIR"
git config core.fileMode false || true

CURRENT_COMMIT="$(git rev-parse --short=12 HEAD 2>/dev/null || echo sem-git)"
CURRENT_BRANCH="$(git branch --show-current 2>/dev/null || echo sem-branch)"

cat <<EOF
============================================================
 NEW WORLD 2 - PREMIUM V2 ACTION RPG / BIGLINUX
============================================================
Projeto    : $PROJECT_DIR
Branch     : $CURRENT_BRANCH
Commit     : $CURRENT_COMMIT
UE         : $UE_ROOT
Resolucao  : $RESOLUTION
FPS        : $FPS_LIMIT
UBT jobs   : $MAX_PARALLEL
Profiler   : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)
Build log  : $BUILD_LOG
Runtime log: $LOG_FILE
============================================================
EOF

# ---------------------------------------------------------------------------
# PRE-FLIGHT DE CONTRATOS CRITICOS
# ---------------------------------------------------------------------------
echo
echo "[1/6] Validando contratos Premium V2..."

required_files=(
    "Source/NewWorld2/NWPremiumGameplayDirector.h"
    "Source/NewWorld2/NWPremiumGameplayDirector.cpp"
    "Source/NewWorld2/NWPremiumEnvironmentDirector.h"
    "Source/NewWorld2/NWPremiumEnvironmentDirector.cpp"
    "Source/NewWorld2/NWEnemyVisualDirector.h"
    "Source/NewWorld2/NWEnemyVisualDirector.cpp"
    "Source/NewWorld2/NWActionReticleWidget.h"
    "Source/NewWorld2/NWActionReticleWidget.cpp"
    "Source/NewWorld2/NWAbilityFeedbackActor.h"
    "Source/NewWorld2/NWAbilityFeedbackActor.cpp"
    "Source/NewWorld2/NWContentPresentationManager.cpp"
    "Source/NewWorld2/NWWorldEventDirector.cpp"
    "Source/NewWorld2/NWGameplaySafetyActor.cpp"
    "scripts/play-biglinux.sh"
    "scripts/check-playable-log.sh"
)
for file in "${required_files[@]}"; do
    [[ -f "$PROJECT_DIR/$file" ]] || fail "arquivo Premium V2 ausente: $file"
done

grep -q 'ANWPremiumGameplayDirector' Source/NewWorld2/NWGameMode.cpp || fail "PremiumGameplayDirector nao esta ligado ao GameMode"
grep -q 'FREE AIM' Source/NewWorld2/NWPremiumGameplayDirector.cpp || fail "free aim nao esta habilitado no gameplay director"
grep -q 'ResolveFreeAimPoint' Source/NewWorld2/NWPremiumGameplayDirector.cpp || fail "camera free-aim resolver ausente"
grep -q 'UNWActionReticleWidget' Source/NewWorld2/NWPremiumGameplayDirector.cpp || fail "reticulo action-RPG nao esta ligado ao gameplay"
grep -q 'ANWPremiumEnvironmentDirector' Source/NewWorld2/NWPremiumGameplayDirector.cpp || fail "diretor de ambiente realista nao esta ligado ao gameplay"
grep -q 'ANWEnemyVisualDirector' Source/NewWorld2/NWPremiumGameplayDirector.cpp || fail "diretor visual de mobs nao esta ligado ao gameplay"
grep -q 'DesiredNearbyTrainingEnemies' Source/NewWorld2/NWPremiumGameplayDirector.h || fail "encontro de teste garantido ausente"
grep -q 'Megascans' Source/NewWorld2/NWPremiumEnvironmentDirector.cpp || fail "curadoria de natureza realista ausente"
grep -q 'lowpoly' Source/NewWorld2/NWPremiumEnvironmentDirector.cpp || fail "filtro anti-lowpoly ausente"
grep -q 'ParagonGreystone' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "safety de arma duplicada do Greystone ausente"
grep -q 'bEnableDynamicPresentationAssets = false' Source/NewWorld2/NWWorldEventDirector.h || fail "apresentacao Niagara/audio insegura voltou a ser default"
grep -q 'bEnableNiagaraPresentation = false' Source/NewWorld2/NWContentPresentationManager.h || fail "Niagara automatico voltou a ser default"
grep -q 'presentation manager unico ativo' Source/NewWorld2/NWGameMode.cpp || fail "dono visual principal nao confirmado"
grep -q 'InvasionIntervalSeconds);' Source/NewWorld2/NWProceduralWorldManager.cpp || fail "primeira invasao ainda pode usar delay legado"
grep -q 'bool IsInventoryVisible() const' Source/NewWorld2/NWCombatHUDWidget.h || fail "contrato do HUD/Bag incompleto: IsInventoryVisible ausente"
grep -q -- '--skip-build' scripts/play-biglinux.sh || fail "launcher nao suporta build prevalidado"

echo "OK: contratos Premium V2 presentes."

# ---------------------------------------------------------------------------
# VALIDACAO UE / VULKAN
# ---------------------------------------------------------------------------
echo
echo "[2/6] Validando Unreal Engine 5.8 e Vulkan..."
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json, sys
p=sys.argv[1]
try:
    v=json.load(open(p, encoding='utf-8'))
    assert int(v.get('MajorVersion',0)) == 5 and int(v.get('MinorVersion',0)) == 8
    print(f"UE {v.get('MajorVersion')}.{v.get('MinorVersion')}.{v.get('PatchVersion',0)} confirmada")
except Exception as exc:
    print(f"ERRO: Build.version nao e UE 5.8: {exc}", file=sys.stderr)
    raise SystemExit(1)
PY

if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-premium-vulkan.txt 2>&1 || {
        cat /tmp/nw2-premium-vulkan.txt >&2 || true
        fail "Vulkan indisponivel"
    }
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-premium-vulkan.txt | head -20 || true
fi

# ---------------------------------------------------------------------------
# FECHAR APENAS INSTANCIA DESTE PROJETO / ISOLAR LOGS
# ---------------------------------------------------------------------------
echo
echo "[3/6] Garantindo runtime limpo..."
if pgrep -af 'UnrealEditor.*NewWorld2' >/dev/null 2>&1; then
    echo "Encerrando instancia anterior de NewWorld2..."
    pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
    sleep 2
fi

mkdir -p "$PROJECT_DIR/Saved/Logs"
find "$PROJECT_DIR/Saved/Logs" -maxdepth 1 -type f -name 'NewWorld2-backup-*.log' -mtime +7 -delete 2>/dev/null || true

if [[ -s "$LOG_FILE" ]]; then
    cp -a "$LOG_FILE" "$HOME/nw2-playable.previous.log"
fi
: > "$LOG_FILE"
: > "$BUILD_LOG"

# ---------------------------------------------------------------------------
# BUILD INCREMENTAL REAL
# ---------------------------------------------------------------------------
echo
echo "[4/6] Compilando NewWorld2Editor..."
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

if (( BUILD_RC != 0 )); then
    echo
    echo "============================================================"
    echo " BUILD FALHOU - O GAME NAO SERA ABERTO"
    echo "============================================================"
    echo "Exit code: $BUILD_RC"
    echo "Build log: $BUILD_LOG"
    echo
    echo "Erros relevantes da compilacao:"
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 160 || true
    echo
    echo "Runtime nao foi aberto nesta rodada; nenhum log antigo sera confundido com este build."
    exit "$BUILD_RC"
fi

echo "OK: build concluido."

DEPRECATED_COUNT="$(grep -a -c 'deprecated:' "$BUILD_LOG" 2>/dev/null || true)"
if [[ "$DEPRECATED_COUNT" =~ ^[0-9]+$ ]] && (( DEPRECATED_COUNT > 0 )); then
    echo "[INFO] Build passou com $DEPRECATED_COUNT warning(s) de API deprecated; nao bloqueiam esta rodada, mas ficaram registrados."
fi

# ---------------------------------------------------------------------------
# EXECUCAO VIA LAUNCHER SEGURO
# ---------------------------------------------------------------------------
echo
echo "[5/6] Abrindo Premium V2..."
echo "Objetivos desta rodada: reticulo central, Q/E/R free-cast, 5 mobs proximos, ambiente com meshes reais e sem primitives de debug."
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
# DIAGNOSTICO
# ---------------------------------------------------------------------------
echo
echo "[6/6] Analisando o log desta execucao..."
CHECK_RC=0
if [[ -f "$LOG_FILE" ]]; then
    bash "$PROJECT_DIR/scripts/check-playable-log.sh" "$LOG_FILE" || CHECK_RC=$?
else
    echo "[ATENCAO] Log nao encontrado: $LOG_FILE"
    CHECK_RC=2
fi

echo
cat <<EOF
============================================================
 RESULTADO DO PREMIUM V2 PLAYTEST
============================================================
Build exit code: $BUILD_RC
Game exit code : $GAME_RC
Diagnostico    : $CHECK_RC
Build log      : $BUILD_LOG
Runtime log    : $LOG_FILE
Commit         : $CURRENT_COMMIT
============================================================
EOF

# Fechar pelo WM ou Ctrl+C pode retornar 130 no Linux. So assinaturas fatais do
# checker transformam isso em crash real.
if (( GAME_RC != 0 && GAME_RC != 130 )); then
    echo
    echo "Ultimas 220 linhas do runtime:"
    tail -n 220 "$LOG_FILE" 2>/dev/null || true
    exit "$GAME_RC"
fi

if (( CHECK_RC >= 2 )); then
    exit "$CHECK_RC"
fi
exit 0
