#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${NW2_PROJECT_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
UE_ROOT="${UE_ROOT:-}"

if [[ -z "$UE_ROOT" && -f "$HOME/.config/new-world2/env.sh" ]]; then
  source "$HOME/.config/new-world2/env.sh"
fi
[[ -n "$UE_ROOT" ]] || { echo "ERRO: UE_ROOT nao definido. Rode first-test-biglinux.sh primeiro." >&2; exit 1; }
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
PROJECT="$PROJECT_ROOT/NewWorld2.uproject"
[[ -x "$EDITOR" ]] || { echo "ERRO: UnrealEditor nao encontrado em $EDITOR" >&2; exit 1; }
[[ -f "$PROJECT" ]] || { echo "ERRO: NewWorld2.uproject nao encontrado em $PROJECT_ROOT" >&2; exit 1; }

mkdir -p "$PROJECT_ROOT/Saved/Logs"
LOG="$PROJECT_ROOT/Saved/Logs/editor-linux-console.log"
echo "Abrindo Unreal Editor 5.8 em Linux. Log: $LOG"
echo "Para adicionar assets: abra Fab no Editor, entre na conta Epic e use Add to Project."
"$EDITOR" "$PROJECT" -vulkan -log -stdout -FullStdOutLogOutput 2>&1 | tee "$LOG"
