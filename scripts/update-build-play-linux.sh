#!/usr/bin/env bash
set -Eeuo pipefail

REPO_URL="https://github.com/LuizBicalho3508/new-world2.git"
PROJECT_DIR="${NW2_PROJECT_DIR:-$HOME/Projetos/new-world2}"
UE_ROOT="${NW2_UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
MAX_PARALLEL="${NW2_MAX_PARALLEL:-3}"
BACKUP_ROOT="${NW2_BACKUP_ROOT:-$HOME/nw2-local-backups}"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"

log() { printf '\n\033[1;36m[NW2]\033[0m %s\n' "$*"; }
fail() { printf '\n\033[1;31m[NW2-ERRO]\033[0m %s\n' "$*" >&2; exit 1; }

log "Atualizacao segura + build + playtest Linux"

command -v git >/dev/null 2>&1 || fail "git nao encontrado."

if [[ ! -d "$PROJECT_DIR/.git" ]]; then
  log "Repositorio local nao encontrado; clonando em $PROJECT_DIR"
  mkdir -p "$(dirname "$PROJECT_DIR")"
  git clone "$REPO_URL" "$PROJECT_DIR"
fi

cd "$PROJECT_DIR"

mkdir -p "$BACKUP_ROOT"
if ! git diff --quiet || ! git diff --cached --quiet; then
  BACKUP_PATCH="$BACKUP_ROOT/local-$TIMESTAMP.patch"
  log "Ha alteracoes Git locais; salvando patch em $BACKUP_PATCH"
  {
    git diff --binary
    git diff --cached --binary
  } > "$BACKUP_PATCH" || true
fi

# Nunca usamos git clean: os assets Fab/Epic podem existir apenas localmente em Content/.
log "Abortando operacoes Git incompletas sem remover assets locais"
git merge --abort 2>/dev/null || true
git rebase --abort 2>/dev/null || true
git cherry-pick --abort 2>/dev/null || true
git am --abort 2>/dev/null || true

log "Sincronizando main com origin/main"
git fetch --prune origin
git switch main
git reset --hard origin/main
git config core.fileMode false

printf '\nCommit em teste:\n'
git log -1 --oneline

if [[ -d Content ]]; then
  UASSET_COUNT="$(find Content -type f -name '*.uasset' 2>/dev/null | wc -l | tr -d ' ')"
  log "Assets .uasset locais preservados: $UASSET_COUNT"
fi

[[ -d "$UE_ROOT" ]] || fail "Unreal Engine nao encontrada em $UE_ROOT. Defina NW2_UE_ROOT=/caminho/da/UE."
[[ -x "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ]] || fail "Build.sh da Unreal nao executavel em $UE_ROOT."
[[ -f scripts/play-biglinux.sh ]] || fail "scripts/play-biglinux.sh nao encontrado."

if [[ -f scripts/verify-fab-assets-linux.sh ]]; then
  log "Validando inventario Fab local (nao bloqueante)"
  bash scripts/verify-fab-assets-linux.sh || true
fi

log "Compilando e abrindo o jogo com UE em $UE_ROOT"
bash scripts/play-biglinux.sh \
  --ue-root "$UE_ROOT" \
  --max-parallel "$MAX_PARALLEL"

if [[ -f scripts/check-playable-log.sh ]]; then
  log "Analisando log do playtest"
  bash scripts/check-playable-log.sh || true
fi

log "Fluxo concluido. Se o jogo fechou por erro, revise ~/nw2-playable.log."
