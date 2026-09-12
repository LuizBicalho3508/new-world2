#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=""
UE_ROOT="${UE_ROOT:-}"
FORCE_RECREATE=0

usage() { echo "Uso: $0 [--project-root PATH] [--ue-root PATH] [--force-recreate]"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project-root) PROJECT_ROOT="$2"; shift 2 ;;
    --ue-root) UE_ROOT="$2"; shift 2 ;;
    --force-recreate) FORCE_RECREATE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
  esac
done

[[ -n "$PROJECT_ROOT" ]] || PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_ROOT="$(realpath "$PROJECT_ROOT")"
PROJECT_FILE="$PROJECT_ROOT/NewWorld2.uproject"
[[ -f "$PROJECT_FILE" ]] || { echo "ERRO: NewWorld2.uproject nao encontrado em $PROJECT_ROOT" >&2; exit 1; }

is_ue58() {
  local root="$1"
  [[ -x "$root/Engine/Binaries/Linux/UnrealEditor" && -f "$root/Engine/Build/Build.version" ]] || return 1
  python3 - "$root/Engine/Build/Build.version" <<'PY'
import json,sys
try:
    v=json.load(open(sys.argv[1],encoding='utf-8'))
    raise SystemExit(0 if int(v.get('MajorVersion',0))==5 and int(v.get('MinorVersion',0))==8 else 1)
except Exception:
    raise SystemExit(1)
PY
}

find_ue() {
  local c
  local candidates=()
  [[ -n "$UE_ROOT" ]] && candidates+=("$UE_ROOT")
  candidates+=("$HOME/UnrealEngine-5.8" "$HOME/UnrealEngine_5.8" "$HOME/UE_5.8" "$HOME/Aplicativos/UnrealEngine-5.8" "$HOME/Games/UnrealEngine-5.8" "/opt/UnrealEngine-5.8" "/opt/UE_5.8")
  shopt -s nullglob
  candidates+=("$HOME"/UnrealEngine-5.8* "$HOME"/Linux_Unreal_Engine-5.8* /opt/UnrealEngine-5.8* /opt/Linux_Unreal_Engine-5.8*)
  shopt -u nullglob
  for c in "${candidates[@]}"; do
    [[ -d "$c" ]] || continue
    if is_ue58 "$c"; then realpath "$c"; return 0; fi
  done
  return 1
}

RESOLVED_UE="$(find_ue || true)"
[[ -n "$RESOLVED_UE" ]] || { echo "ERRO: Unreal Engine 5.8 Linux nao encontrada. Defina UE_ROOT ou use --ue-root." >&2; exit 1; }

EDITOR_CMD="$RESOLVED_UE/Engine/Binaries/Linux/UnrealEditor-Cmd"
[[ -x "$EDITOR_CMD" ]] || EDITOR_CMD="$RESOLVED_UE/Engine/Binaries/Linux/UnrealEditor"
SOURCE_MAP="$RESOLVED_UE/Engine/Content/Maps/Entry.umap"
if [[ ! -f "$SOURCE_MAP" ]]; then
  SOURCE_MAP="$(find "$RESOLVED_UE/Engine/Content/Maps" -type f -name Entry.umap -print -quit 2>/dev/null || true)"
fi
[[ -f "$SOURCE_MAP" ]] || { echo "ERRO: Entry.umap nao encontrado na UE 5.8." >&2; exit 1; }

GENERATED_DIR="$PROJECT_ROOT/Content/GeneratedWorld"
MAP_FILE="$GENERATED_DIR/NW2_OpenWorld.umap"
MAP_INI="$GENERATED_DIR/NW2_OpenWorld.ini"
MARKER="$PROJECT_ROOT/Saved/NW2_WorldPartition.ready"

if (( FORCE_RECREATE )); then
  rm -rf "$GENERATED_DIR" "$PROJECT_ROOT/Content/__ExternalActors__/GeneratedWorld" "$PROJECT_ROOT/Content/__ExternalObjects__/GeneratedWorld" "$MARKER"
fi

if [[ -f "$MARKER" && -f "$MAP_FILE" ]]; then
  echo "World Partition local ja preparado: /Game/GeneratedWorld/NW2_OpenWorld"
  exit 0
fi

mkdir -p "$GENERATED_DIR" "$(dirname "$MARKER")"
[[ -f "$MAP_FILE" ]] || cp -f "$SOURCE_MAP" "$MAP_FILE"
cat > "$MAP_INI" <<'INI'
[/Script/UnrealEd.WorldPartitionConvertCommandlet]
EditorHashClass=Class'/Script/Engine.WorldPartitionEditorSpatialHash'
RuntimeHashClass=Class'/Script/Engine.WorldPartitionRuntimeSpatialHash'
HLODLayerAssetsPath=
DefaultHLODLayerName=

[/Script/Engine.WorldPartitionEditorSpatialHash]
CellSize=25600
WorldImage=None
INI

echo "Convertendo NW2_OpenWorld para World Partition..."
(
  cd "$GENERATED_DIR"
  "$EDITOR_CMD" "$PROJECT_FILE" -run=WorldPartitionConvertCommandlet NW2_OpenWorld.umap -AllowCommandletRendering -SCCProvider=None -Verbose -Unattended -NoSplash -NullRHI
)

echo "UE58 Linux World Partition preparado em $(date --iso-8601=seconds)" > "$MARKER"
echo "WORLD PARTITION PRONTO: /Game/GeneratedWorld/NW2_OpenWorld"
