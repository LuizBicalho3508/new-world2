#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
GAME_MAP="/Game/GeneratedWorld/NW2_OpenWorld"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
MAX_PARALLEL_ACTIONS=3
FPS_LIMIT=45
RES_X=1920
RES_Y=1080
PROFILE=0

while (($#)); do
    case "$1" in
        --ue-root)
            UE_ROOT="$2"
            shift 2
            ;;
        --profile)
            PROFILE=1
            shift
            ;;
        --fps)
            FPS_LIMIT="$2"
            shift 2
            ;;
        --max-parallel)
            MAX_PARALLEL_ACTIONS="$2"
            shift 2
            ;;
        *)
            echo "Argumento desconhecido: $1" >&2
            exit 2
            ;;
    esac
done

EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
LOG_FILE="$HOME/nw2-playable.log"
BACKUP_DIR="$HOME/nw2-config-backups/$(date +%Y%m%d-%H%M%S)"

if [[ ! -x "$EDITOR" ]]; then
    echo "ERRO: UnrealEditor nao encontrado em: $EDITOR" >&2
    exit 1
fi
if [[ ! -x "$BUILD_SH" ]]; then
    echo "ERRO: Build.sh nao encontrado em: $BUILD_SH" >&2
    exit 1
fi

cd "$PROJECT_DIR"
git config core.fileMode false || true

# O Editor/Fab pode salvar novamente um DefaultInput.ini antigo. Isso fez R
# disparar Ability3 e RegenerateWorld ao mesmo tempo, reconstruindo todo o mundo.
# Guardamos a diferenca para consulta, mas o playtest sempre usa o input versionado.
if ! git diff --quiet -- Config/DefaultInput.ini; then
    mkdir -p "$BACKUP_DIR"
    git diff -- Config/DefaultInput.ini > "$BACKUP_DIR/DefaultInput.local.patch"
    echo "[INPUT] Config local antigo salvo em: $BACKUP_DIR/DefaultInput.local.patch"
    git checkout -- Config/DefaultInput.ini
fi

# Overrides de Saved/Config tambem podem manter mappings antigos mesmo com o
# DefaultInput.ini correto. Guardamos e retiramos somente Input.ini do playtest.
shopt -s nullglob
for INPUT_OVERRIDE in \
    "$PROJECT_DIR"/Saved/Config/Linux*/Input.ini \
    "$PROJECT_DIR"/Saved/Config/LinuxEditor/Input.ini \
    "$PROJECT_DIR"/Saved/Config/Linux/Input.ini; do
    [[ -f "$INPUT_OVERRIDE" ]] || continue
    mkdir -p "$BACKUP_DIR"
    cp -a "$INPUT_OVERRIDE" "$BACKUP_DIR/$(basename "$(dirname "$INPUT_OVERRIDE")")-Input.ini"
    rm -f "$INPUT_OVERRIDE"
    echo "[INPUT] Override removido do playtest: $INPUT_OVERRIDE"
done
shopt -u nullglob

echo "============================================================"
echo " NEW WORLD 2 - PLAYABLE BIGLINUX"
echo "============================================================"
echo "Projeto : $PROJECT_DIR"
echo "UE      : $UE_ROOT"
echo "Build   : MaxParallelActions=$MAX_PARALLEL_ACTIONS"
echo "Video   : ${RES_X}x${RES_Y} Vulkan SM6 @ ${FPS_LIMIT} FPS"
echo "Log     : $LOG_FILE"
echo

echo "[1/2] Build incremental NewWorld2Editor..."
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL_ACTIONS"

echo
echo "[2/2] Abrindo game direto..."
if (( PROFILE )); then
    EXEC_CMDS="t.MaxFPS $FPS_LIMIT,stat unit,stat game,stat gpu,stat fps"
else
    EXEC_CMDS="t.MaxFPS $FPS_LIMIT"
fi

set +e
"$EDITOR" "$PROJECT_FILE" "$GAME_MAP" \
    -game -log -stdout -FullStdOutLogOutput \
    -vulkan -sm6 -windowed -ResX="$RES_X" -ResY="$RES_Y" \
    -NoVSync "-ExecCmds=$EXEC_CMDS" \
    2>&1 | tee "$LOG_FILE"
RC=${PIPESTATUS[0]}
set -e

echo
echo "============================================================"
echo " GAME FINALIZADO - EXIT CODE: $RC"
echo " Log: $LOG_FILE"
echo "============================================================"
exit "$RC"
