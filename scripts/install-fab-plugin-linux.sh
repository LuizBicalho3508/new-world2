#!/usr/bin/env bash
set -euo pipefail

UE_ROOT="${UE_ROOT:-}"
FAB_ZIP=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ue-root) UE_ROOT="$2"; shift 2 ;;
    --zip) FAB_ZIP="$2"; shift 2 ;;
    -h|--help) echo "Uso: $0 --ue-root PATH [--zip Linux_Fab_5.8...zip]"; exit 0 ;;
    *) echo "Argumento desconhecido: $1" >&2; exit 2 ;;
  esac
done

[[ -n "$UE_ROOT" ]] || { echo "ERRO: informe --ue-root ou exporte UE_ROOT." >&2; exit 1; }
UE_ROOT="$(realpath "$UE_ROOT")"
[[ -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]] || { echo "ERRO: UnrealEditor Linux nao encontrado em $UE_ROOT" >&2; exit 1; }

if [[ -z "$FAB_ZIP" ]]; then
  FAB_ZIP="$(find "$HOME/Downloads" "$HOME/Transferências" -maxdepth 1 -type f \( -iname 'Linux_Fab_5.8*.zip' -o -iname '*Fab*5.8*Linux*.zip' \) -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2- || true)"
fi

if [[ -z "$FAB_ZIP" || ! -f "$FAB_ZIP" ]]; then
  echo "Plugin Fab Linux 5.8 nao encontrado em Downloads." >&2
  echo "Baixe o ZIP Linux_Fab_5.8.x na pagina Linux da Unreal e execute novamente." >&2
  exit 2
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
unzip -q "$FAB_ZIP" -d "$TMP"
UPLUGIN="$(find "$TMP" -type f -name 'Fab.uplugin' -print -quit)"
[[ -n "$UPLUGIN" ]] || { echo "ERRO: Fab.uplugin nao encontrado dentro de $FAB_ZIP" >&2; exit 1; }
SOURCE_DIR="$(dirname "$UPLUGIN")"
TARGET_BASE="$UE_ROOT/Engine/Plugins/Marketplace"
TARGET="$TARGET_BASE/Fab"

if [[ -w "$UE_ROOT/Engine/Plugins" ]]; then
  mkdir -p "$TARGET_BASE"
  rm -rf "$TARGET"
  cp -a "$SOURCE_DIR" "$TARGET"
else
  echo "A instalacao da UE exige permissao administrativa para copiar o Fab."
  sudo mkdir -p "$TARGET_BASE"
  sudo rm -rf "$TARGET"
  sudo cp -a "$SOURCE_DIR" "$TARGET"
fi

[[ -f "$TARGET/Fab.uplugin" ]] || { echo "ERRO: instalacao do Fab nao foi confirmada." >&2; exit 1; }
echo "Fab Plugin instalado em: $TARGET"
echo "O NewWorld2 referencia Fab como plugin opcional; se presente, a UE pode carrega-lo automaticamente."
