#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
GAME_MAP="/Engine/Maps/Entry?game=/Script/NewWorld2.NWGameMode"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
MAX_PARALLEL_ACTIONS=3
FPS_LIMIT=45
RES_X=1280
RES_Y=720
PROFILE=0
USE_WORLD_PARTITION=0
SKIP_BUILD=0

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
        --world-partition)
            USE_WORLD_PARTITION=1
            shift
            ;;
        --skip-build)
            SKIP_BUILD=1
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
        --resolution)
            if [[ "$2" =~ ^([0-9]+)x([0-9]+)$ ]]; then
                RES_X="${BASH_REMATCH[1]}"
                RES_Y="${BASH_REMATCH[2]}"
            else
                echo "Resolucao invalida: $2. Exemplo: 1600x900" >&2
                exit 2
            fi
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

: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

cd "$PROJECT_DIR"
git config core.fileMode false || true

# O Editor/Fab pode salvar novamente um DefaultInput.ini antigo. O playtest sempre
# usa o arquivo versionado; uma eventual diferenca local e guardada para consulta.
if ! git diff --quiet -- Config/DefaultInput.ini; then
    mkdir -p "$BACKUP_DIR"
    git diff -- Config/DefaultInput.ini > "$BACKUP_DIR/DefaultInput.local.patch"
    echo "[INPUT] Config local antigo salvo em: $BACKUP_DIR/DefaultInput.local.patch"
    git checkout -- Config/DefaultInput.ini
fi

# Overrides de runtime podem anular tanto input quanto renderer. Preservamos uma
# copia antes de remover apenas os INIs gerados; Content/Fab nunca e tocado.
shopt -s nullglob
for CONFIG_OVERRIDE in \
    "$PROJECT_DIR"/Saved/Config/Linux*/Input.ini \
    "$PROJECT_DIR"/Saved/Config/Linux*/Engine.ini \
    "$PROJECT_DIR"/Saved/Config/Linux*/Scalability.ini \
    "$PROJECT_DIR"/Saved/Config/Linux*/GameUserSettings.ini \
    "$PROJECT_DIR"/Saved/Config/LinuxEditor/Input.ini \
    "$PROJECT_DIR"/Saved/Config/LinuxEditor/Engine.ini \
    "$PROJECT_DIR"/Saved/Config/LinuxEditor/Scalability.ini \
    "$PROJECT_DIR"/Saved/Config/LinuxEditor/GameUserSettings.ini; do
    [[ -f "$CONFIG_OVERRIDE" ]] || continue
    mkdir -p "$BACKUP_DIR"
    PARENT_NAME="$(basename "$(dirname "$CONFIG_OVERRIDE")")"
    FILE_NAME="$(basename "$CONFIG_OVERRIDE")"
    cp -a "$CONFIG_OVERRIDE" "$BACKUP_DIR/${PARENT_NAME}-${FILE_NAME}"
    rm -f "$CONFIG_OVERRIDE"
    echo "[CONFIG] Override runtime removido do playtest: $CONFIG_OVERRIDE"
done
shopt -u nullglob

if (( USE_WORLD_PARTITION )); then
    WP_SCRIPT="$PROJECT_DIR/scripts/prepare-worldpartition-linux.sh"
    if [[ -f "$WP_SCRIPT" ]]; then
        echo "[MAPA] World Partition solicitado explicitamente."
        if bash "$WP_SCRIPT" --project-root "$PROJECT_DIR" --ue-root "$UE_ROOT"; then
            if [[ -f "$PROJECT_DIR/Content/GeneratedWorld/NW2_OpenWorld.umap" ]]; then
                GAME_MAP="/Game/GeneratedWorld/NW2_OpenWorld?game=/Script/NewWorld2.NWGameMode"
            fi
        fi
    fi
else
    echo "[MAPA] Vertical slice seguro: /Engine/Maps/Entry + mundo procedural runtime."
fi

echo "============================================================"
echo " NEW WORLD 2 - PLAYABLE BIGLINUX"
echo "============================================================"
echo "Projeto : $PROJECT_DIR"
echo "UE      : $UE_ROOT"
echo "Mapa    : $GAME_MAP"
echo "Build   : $([[ $SKIP_BUILD -eq 1 ]] && echo PREVALIDADO || echo MaxParallelActions=$MAX_PARALLEL_ACTIONS)"
echo "Video   : ${RES_X}x${RES_Y} Vulkan SM6 @ ${FPS_LIMIT} FPS"
echo "Profile : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)"
echo "Log     : $LOG_FILE"
echo

if (( ! SKIP_BUILD )); then
    echo "[1/2] Build incremental NewWorld2Editor..."
    nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
        -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL_ACTIONS"
else
    echo "[1/2] Build ignorado: caller ja validou esta revisao."
fi

echo
echo "[2/2] Abrindo game direto..."
if (( PROFILE )); then
    EXEC_CMDS="t.MaxFPS $FPS_LIMIT,stat unit,stat game,stat gpu,stat fps"
else
    EXEC_CMDS="t.MaxFPS $FPS_LIMIT,stat none"
fi

set +e
"$EDITOR" "$PROJECT_FILE" "$GAME_MAP" \
    -game -log -stdout -FullStdOutLogOutput -NoSplash \
    -vulkan -sm6 -windowed -ResX="$RES_X" -ResY="$RES_Y" \
    -NoVSync "-ExecCmds=$EXEC_CMDS"
RC=$?
set -e

echo
echo "============================================================"
echo " GAME FINALIZADO - EXIT CODE: $RC"
echo " Log completo: $LOG_FILE"
echo "============================================================"
exit "$RC"
