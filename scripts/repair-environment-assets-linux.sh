#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
PY_SCRIPT="$PROJECT_DIR/scripts/nw2_repair_environment_assets.py"
LOG_FILE="${NW2_ENV_REPAIR_LOG:-$HOME/nw2-v9-environment-repair.log}"
MARKER="$PROJECT_DIR/Saved/NW2_V9_ENV_REPAIR_DONE"

[[ -x "$EDITOR" ]] || { echo "[ENV-REPAIR-V9] UnrealEditor nao encontrado: $EDITOR" >&2; exit 2; }
[[ -f "$PROJECT_FILE" ]] || { echo "[ENV-REPAIR-V9] projeto nao encontrado: $PROJECT_FILE" >&2; exit 2; }
[[ -f "$PY_SCRIPT" ]] || { echo "[ENV-REPAIR-V9] Python de reparo ausente: $PY_SCRIPT" >&2; exit 2; }

if [[ -f "$MARKER" && "${NW2_FORCE_ENV_REPAIR:-0}" != "1" ]]; then
    echo "[ENV-REPAIR-V9] ja executado nesta copia do projeto; use NW2_FORCE_ENV_REPAIR=1 para repetir."
    exit 0
fi

mkdir -p "$(dirname "$MARKER")"

echo "============================================================"
echo " NEW WORLD 2 - REPARO DE ASSETS DE AMBIENTE V9"
echo "============================================================"
echo "Projeto : $PROJECT_DIR"
echo "UE      : $UE_ROOT"
echo "Log     : $LOG_FILE"
echo

echo "Persistindo 'Used with Instanced Static Meshes' e resalvando meshes utilizados..."
set +e
"$EDITOR" "$PROJECT_FILE" \
    -unattended \
    -nop4 \
    -nosplash \
    -NoSound \
    -NullRHI \
    -ExecutePythonScript="$PY_SCRIPT" \
    -log="$LOG_FILE"
RC=$?
set -e

if (( RC != 0 )); then
    echo "[ENV-REPAIR-V9] AVISO: editor retornou RC=$RC. O runtime ainda possui reparo em memoria; veja $LOG_FILE"
    exit 0
fi

if grep -a -q '\[ENV-REPAIR-V9\].*failures=0' "$LOG_FILE" 2>/dev/null; then
    date -Is > "$MARKER"
    echo "[ENV-REPAIR-V9] OK: materiais/meshes persistidos e marker criado."
else
    echo "[ENV-REPAIR-V9] concluido com avisos; marker nao foi criado para permitir nova tentativa."
    grep -a '\[ENV-REPAIR-V9\]' "$LOG_FILE" | tail -n 40 || true
fi

exit 0
