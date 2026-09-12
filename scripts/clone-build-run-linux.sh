#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/LuizBicalho3508/new-world2.git"
DESTINATION="$HOME/Projetos/new-world2"
UE_ROOT="${UE_ROOT:-}"
SKIP_WP=0
SKIP_TOOLCHAIN=0
PROFILE=0
MAX_PARALLEL_ACTIONS=3
FPS_LIMIT=45
RES_X=1920
RES_Y=1080

usage() {
  echo "Uso: $0 [--destination PATH] [--ue-root PATH] [--skip-world-partition] [--skip-toolchain] [--profile] [--fps N] [--max-parallel N] [--resolution WIDTHxHEIGHT]"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --destination) DESTINATION="$2"; shift 2 ;;
    --ue-root) UE_ROOT="$2"; shift 2 ;;
    --skip-world-partition) SKIP_WP=1; shift ;;
    --skip-toolchain) SKIP_TOOLCHAIN=1; shift ;;
    --profile) PROFILE=1; shift ;;
    --fps) FPS_LIMIT="$2"; shift 2 ;;
    --max-parallel) MAX_PARALLEL_ACTIONS="$2"; shift 2 ;;
    --resolution)
      if [[ "$2" =~ ^([0-9]+)x([0-9]+)$ ]]; then
        RES_X="${BASH_REMATCH[1]}"
        RES_Y="${BASH_REMATCH[2]}"
      else
        echo "Resolucao invalida: $2. Use por exemplo 1920x1080." >&2
        exit 2
      fi
      shift 2
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
  esac
done

step() { echo; echo "============================================================"; echo " $*"; echo "============================================================"; }

is_ue58() {
  local root="$1"
  [[ -x "$root/Engine/Binaries/Linux/UnrealEditor" && -x "$root/Engine/Build/BatchFiles/Linux/Build.sh" && -f "$root/Engine/Build/Build.version" ]] || return 1
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

step "1/7 - CLONANDO / ATUALIZANDO REPOSITORIO"
mkdir -p "$(dirname "$DESTINATION")"
if [[ -d "$DESTINATION/.git" ]]; then
  if ! git -C "$DESTINATION" diff --quiet || ! git -C "$DESTINATION" diff --cached --quiet; then
    echo "ERRO: ha alteracoes locais em arquivos versionados. Commit/stash antes de atualizar." >&2
    exit 1
  fi
  git -C "$DESTINATION" fetch origin
  git -C "$DESTINATION" switch main
  git -C "$DESTINATION" pull --ff-only origin main
elif [[ -e "$DESTINATION" && -n "$(ls -A "$DESTINATION" 2>/dev/null)" ]]; then
  echo "ERRO: $DESTINATION existe e nao e um repositorio vazio." >&2
  exit 1
else
  git clone "$REPO_URL" "$DESTINATION"
fi
PROJECT_FILE="$DESTINATION/NewWorld2.uproject"
[[ -f "$PROJECT_FILE" ]] || { echo "ERRO: NewWorld2.uproject nao encontrado." >&2; exit 1; }
git -C "$DESTINATION" log -1 --oneline

step "2/7 - LOCALIZANDO UNREAL ENGINE 5.8 LINUX"
RESOLVED_UE="$(find_ue || true)"
[[ -n "$RESOLVED_UE" ]] || { echo "ERRO: UE 5.8 Linux nao encontrada. Use scripts/first-test-biglinux.sh para instalar/configurar." >&2; exit 2; }
UE_ROOT="$RESOLVED_UE"
export UE_ROOT
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
echo "UE_ROOT=$UE_ROOT"

step "3/7 - VALIDANDO VULKAN / GPU"
if command -v vulkaninfo >/dev/null 2>&1; then
  if ! vulkaninfo --summary >/tmp/nw2-vulkan-summary.txt 2>&1; then
    cat /tmp/nw2-vulkan-summary.txt >&2 || true
    echo "ERRO: Vulkan nao esta funcional. Corrija o driver da GPU antes de abrir a UE." >&2
    exit 3
  fi
  grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-vulkan-summary.txt | head -20 || true
else
  echo "AVISO: vulkaninfo nao instalado; o preflight da GPU sera ignorado."
fi
if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true
fi

step "4/7 - TOOLCHAIN NATIVO DA UNREAL"
TOOLCHAIN_SETUP="$UE_ROOT/Engine/Build/BatchFiles/Linux/SetupToolchain.sh"
TOOLCHAIN_MARKER="$HOME/.cache/new-world2/ue58-toolchain.ready"
if (( SKIP_TOOLCHAIN )); then
  echo "Toolchain ignorado por --skip-toolchain."
elif [[ -x "$TOOLCHAIN_SETUP" && ! -f "$TOOLCHAIN_MARKER" ]]; then
  mkdir -p "$(dirname "$TOOLCHAIN_MARKER")"
  "$TOOLCHAIN_SETUP"
  date --iso-8601=seconds > "$TOOLCHAIN_MARKER"
else
  echo "Toolchain ja preparado ou SetupToolchain.sh nao e necessario nesta distribuicao da UE."
fi

step "5/7 - COMPILANDO NEWWORLD2EDITOR PARA LINUX"
echo "MaxParallelActions=$MAX_PARALLEL_ACTIONS (perfil 16 GB RAM / CPU antiga)"
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL_ACTIONS"

GAME_MAP='/Engine/Maps/Entry?game=/Script/NewWorld2.NWGameMode'
step "6/7 - PREPARANDO WORLD PARTITION"
if (( SKIP_WP )); then
  echo "World Partition ignorado; usando mapa fallback."
else
  WP="$DESTINATION/scripts/prepare-worldpartition-linux.sh"
  if [[ -f "$WP" ]]; then
    chmod +x "$WP"
    if "$WP" --project-root "$DESTINATION" --ue-root "$UE_ROOT"; then
      if [[ -f "$DESTINATION/Content/GeneratedWorld/NW2_OpenWorld.umap" ]]; then
        GAME_MAP='/Game/GeneratedWorld/NW2_OpenWorld?game=/Script/NewWorld2.NWGameMode'
      fi
    else
      echo "AVISO: World Partition falhou; usando mapa fallback."
    fi
  fi
fi

step "7/7 - INICIANDO TESTE JOGAVEL LINUX"
mkdir -p "$DESTINATION/Saved/Logs"
LOG_FILE="$DESTINATION/Saved/Logs/first-test-linux-console.log"
if (( PROFILE )); then
  EXEC_CMDS="t.MaxFPS $FPS_LIMIT,stat unit,stat game,stat gpu,stat fps"
  echo "PROFILE ATIVO: observe Game, Draw, GPU e Frame na tela."
else
  EXEC_CMDS="t.MaxFPS $FPS_LIMIT,stat unit,stat fps"
fi

echo "Perfil: CPU-saver + GPU-quality | ${RES_X}x${RES_Y} | ${FPS_LIMIT} FPS | Vulkan SM6"
echo "Log: $LOG_FILE"
echo "Feche a janela do jogo para retornar ao terminal."
echo
set +e
"$EDITOR" "$PROJECT_FILE" "$GAME_MAP" \
  -game -log -stdout -FullStdOutLogOutput \
  -vulkan -sm6 -windowed -ResX="$RES_X" -ResY="$RES_Y" \
  -NoVSync "-ExecCmds=$EXEC_CMDS" 2>&1 | tee "$LOG_FILE"
GAME_STATUS=${PIPESTATUS[0]}
set -e
exit "$GAME_STATUS"
