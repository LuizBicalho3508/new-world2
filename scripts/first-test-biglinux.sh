#!/usr/bin/env bash
set -Eeuo pipefail

REPO_URL="https://github.com/LuizBicalho3508/new-world2.git"
DESTINATION="${NW2_PROJECT_ROOT:-$HOME/Projetos/new-world2}"
UE_ROOT="${UE_ROOT:-}"
ENGINE_INSTALL_BASE="${NW2_UE_INSTALL_ROOT:-$HOME/Aplicativos}"
SKIP_SYSTEM_UPDATE=0
SKIP_ASSET_CHECK=0
SKIP_WORLD_PARTITION=0
CURRENT_STEP="inicializacao"
BOOTSTRAP_LOG="${NW2_BOOTSTRAP_LOG:-$HOME/nw2-bootstrap.log}"

# Mantem um log completo fora do repositorio para que sobreviva a falhas, atualizacoes
# do Git e encerramentos acidentais do terminal.
mkdir -p "$(dirname "$BOOTSTRAP_LOG")"
: > "$BOOTSTRAP_LOG"
exec > >(tee -a "$BOOTSTRAP_LOG") 2>&1

on_error() {
  local rc=$?
  local line="${BASH_LINENO[0]:-desconhecida}"
  trap - ERR
  echo
  echo "============================================================"
  echo " ERRO NO BOOTSTRAP"
  echo "============================================================"
  echo "Etapa: $CURRENT_STEP"
  echo "Linha aproximada: $line"
  echo "Exit code: $rc"
  echo "Log completo: $BOOTSTRAP_LOG"
  echo
  echo "Para mostrar as ultimas 200 linhas depois:"
  echo "tail -n 200 \"$BOOTSTRAP_LOG\""
  exit "$rc"
}
trap on_error ERR

usage() {
  cat <<USAGE
Uso: $0 [opcoes]
  --destination PATH          Projeto (padrao: $HOME/Projetos/new-world2)
  --ue-root PATH              Unreal Engine 5.8 ja instalada/descompactada
  --skip-system-update        Nao executa pacman -Syu
  --skip-asset-check          Nao valida Content/*.uasset
  --skip-world-partition      Testa no mapa fallback
USAGE
}
while [[ $# -gt 0 ]]; do
  case "$1" in
    --destination) DESTINATION="$2"; shift 2 ;;
    --ue-root) UE_ROOT="$2"; shift 2 ;;
    --skip-system-update) SKIP_SYSTEM_UPDATE=1; shift ;;
    --skip-asset-check) SKIP_ASSET_CHECK=1; shift ;;
    --skip-world-partition) SKIP_WORLD_PARTITION=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
  esac
done

step() {
  CURRENT_STEP="$*"
  echo
  echo "============================================================"
  echo " $*"
  echo "============================================================"
}

is_biglinux() {
  [[ -f /etc/os-release ]] || return 1
  grep -Eqi '(^ID=biglinux|BigLinux|ID_LIKE=.*(arch|manjaro))' /etc/os-release
}

ensure_packages() {
  command -v pacman >/dev/null 2>&1 || { echo "ERRO: este bootstrap espera BigLinux/Manjaro/Arch com pacman." >&2; exit 1; }
  local packages=(base-devel git curl wget unzip p7zip cmake ninja python clang lld rsync vulkan-tools xdg-utils)
  if (( SKIP_SYSTEM_UPDATE )); then
    sudo pacman -S --needed --noconfirm "${packages[@]}"
  else
    echo "Atualizando BigLinux e instalando dependencias..."
    sudo pacman -Syu --needed --noconfirm "${packages[@]}"
  fi
}

sync_repo() {
  mkdir -p "$(dirname "$DESTINATION")"

  if [[ -d "$DESTINATION/.git" ]]; then
    # No Linux o Git pode registrar chmod +x/-x como modificacao. O projeto executa
    # scripts via `bash script.sh`, portanto o bit de execucao local nao deve sujar o repo.
    git -C "$DESTINATION" config core.fileMode false

    # Se houver mudancas REAIS em arquivos versionados, preserve-as em stash.
    # Nao usamos -u/-a: Content Fab e outros arquivos nao versionados permanecem no lugar.
    if ! git -C "$DESTINATION" diff --quiet || ! git -C "$DESTINATION" diff --cached --quiet; then
      echo "Alteracoes reais em arquivos versionados foram encontradas:"
      git -C "$DESTINATION" status --short --untracked-files=no || true
      local stash_name="nw2-auto-bootstrap-$(date +%Y%m%d-%H%M%S)"
      echo "Preservando essas alteracoes em git stash: $stash_name"
      git -C "$DESTINATION" stash push -m "$stash_name" -- . >/dev/null
      echo "OK: codigo/config local preservado. Assets Fab nao versionados nao foram tocados."
    fi

    git -C "$DESTINATION" fetch origin
    git -C "$DESTINATION" switch main
    git -C "$DESTINATION" pull --ff-only origin main
  elif [[ -e "$DESTINATION" && -n "$(ls -A "$DESTINATION" 2>/dev/null)" ]]; then
    echo "ERRO: $DESTINATION existe e nao e um repositorio Git vazio." >&2
    exit 1
  else
    git clone "$REPO_URL" "$DESTINATION"
    git -C "$DESTINATION" config core.fileMode false
  fi
}

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
  candidates+=("$HOME"/UnrealEngine-5.8* "$HOME"/Linux_Unreal_Engine-5.8* "$HOME/Aplicativos"/UnrealEngine-5.8* /opt/UnrealEngine-5.8* /opt/Linux_Unreal_Engine-5.8*)
  shopt -u nullglob
  for c in "${candidates[@]}"; do
    [[ -d "$c" ]] || continue
    if is_ue58 "$c"; then realpath "$c"; return 0; fi
  done
  return 1
}

find_engine_zip() {
  find "$HOME/Downloads" "$HOME/Transferências" -maxdepth 1 -type f \
    \( -iname 'Linux_Unreal_Engine_5.8*.zip' -o -iname 'Linux_Unreal_Engine-5.8*.zip' -o -iname '*Unreal*Engine*5.8*Linux*.zip' -o -iname '*Unreal*5.8*.zip' \) \
    -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2- || true
}

extract_engine_zip() {
  local zip="$1"
  local target="$ENGINE_INSTALL_BASE/UnrealEngine-5.8"
  mkdir -p "$ENGINE_INSTALL_BASE"
  local free_gb
  free_gb="$(df -Pk "$ENGINE_INSTALL_BASE" | awk 'NR==2 {printf "%d", $4/1024/1024}')"
  echo "Espaco livre aproximado: ${free_gb} GB" >&2
  (( free_gb >= 80 )) || echo "AVISO: menos de 80 GB livres. UE + caches + assets podem ocupar bastante espaco." >&2

  if [[ -e "$target" && ! -x "$target/Engine/Binaries/Linux/UnrealEditor" && -n "$(ls -A "$target" 2>/dev/null)" ]]; then
    echo "ERRO: $target ja existe mas nao parece uma UE valida. Renomeie/remova e rode novamente." >&2
    return 1
  fi
  if [[ ! -x "$target/Engine/Binaries/Linux/UnrealEditor" ]]; then
    echo "Extraindo Unreal Engine 5.8 Linux para $target ..." >&2
    mkdir -p "$target"
    unzip -q "$zip" -d "$target"
  fi
  if is_ue58 "$target"; then echo "$target"; return 0; fi

  local editor root
  editor="$(find "$target" -type f -path '*/Engine/Binaries/Linux/UnrealEditor' -print -quit 2>/dev/null || true)"
  if [[ -n "$editor" ]]; then
    root="$(dirname "$(dirname "$(dirname "$(dirname "$editor")")")")"
    if is_ue58 "$root"; then echo "$root"; return 0; fi
  fi
  return 1
}

persist_ue_root() {
  local root="$1" rc
  mkdir -p "$HOME/.config/environment.d" "$HOME/.config/new-world2"
  printf 'UE_ROOT=%s\n' "$root" > "$HOME/.config/environment.d/90-unreal-engine.conf"
  printf 'export UE_ROOT=%q\n' "$root" > "$HOME/.config/new-world2/env.sh"
  for rc in "$HOME/.bashrc" "$HOME/.zshrc"; do
    [[ -e "$rc" ]] || continue
    if ! grep -Fq '.config/new-world2/env.sh' "$rc"; then
      printf '\n# New World 2 / Unreal Engine\n[[ -f "$HOME/.config/new-world2/env.sh" ]] && source "$HOME/.config/new-world2/env.sh"\n' >> "$rc"
    fi
  done
}

install_fab_if_downloaded() {
  local helper="$DESTINATION/scripts/install-fab-plugin-linux.sh" fab_zip
  [[ -f "$helper" ]] || return 0
  fab_zip="$(find "$HOME/Downloads" "$HOME/Transferências" -maxdepth 1 -type f \( -iname 'Linux_Fab_5.8*.zip' -o -iname '*Fab*5.8*Linux*.zip' \) -print -quit 2>/dev/null || true)"
  if [[ -n "$fab_zip" ]]; then
    echo "Fab ZIP detectado: $fab_zip"
    bash "$helper" --ue-root "$UE_ROOT" --zip "$fab_zip"
  else
    echo "Plugin Fab Linux 5.8 ainda nao foi encontrado em Downloads (opcional para o primeiro boot)."
  fi
}

preflight_vulkan() {
  if ! vulkaninfo --summary >/tmp/nw2-vulkan-summary.txt 2>&1; then
    cat /tmp/nw2-vulkan-summary.txt >&2 || true
    echo "ERRO: Vulkan nao esta funcional. A UE 5.8 Linux depende de driver Vulkan funcional." >&2
    echo "No BigLinux, abra Central de Controle/Drivers e instale o driver recomendado para sua GPU." >&2
    exit 3
  fi
  grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-vulkan-summary.txt | head -20 || true
  if command -v nvidia-smi >/dev/null 2>&1; then
    echo
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true
  fi
}

printf '============================================================\n'
printf ' NEW WORLD 2 - BIGLINUX - PRIMEIRA INSTALACAO E TESTE\n'
printf '============================================================\n'
printf ' Log persistente: %s\n' "$BOOTSTRAP_LOG"
printf ' Inicio: %s\n' "$(date --iso-8601=seconds)"

step "1/8 - IDENTIFICANDO BIGLINUX E INSTALANDO DEPENDENCIAS"
if is_biglinux; then grep -E '^(NAME|PRETTY_NAME|ID|ID_LIKE)=' /etc/os-release || true; else echo "AVISO: BigLinux nao confirmado; continuarei se houver pacman."; fi
ensure_packages

step "2/8 - CLONANDO / ATUALIZANDO NEWWORLD2"
sync_repo
git -C "$DESTINATION" log -1 --oneline

step "3/8 - LOCALIZANDO / INSTALANDO UNREAL ENGINE 5.8 LINUX"
RESOLVED_UE="$(find_ue || true)"
if [[ -z "$RESOLVED_UE" ]]; then
  ENGINE_ZIP="$(find_engine_zip)"
  if [[ -n "$ENGINE_ZIP" ]]; then
    echo "ZIP detectado: $ENGINE_ZIP"
    RESOLVED_UE="$(extract_engine_zip "$ENGINE_ZIP" || true)"
  fi
fi

if [[ -z "$RESOLVED_UE" ]]; then
  echo
  echo "A UE 5.8 Linux ainda nao esta nesta maquina."
  echo "A Epic exige login para liberar o build Linux pre-compilado; o script nao pode baixar essa etapa anonimamente."
  echo
  echo "No navegador que sera aberto:"
  echo "  1. Entre na sua conta Epic."
  echo "  2. Baixe o ZIP da Unreal Engine 5.8 para Linux."
  echo "  3. Se disponivel, baixe tambem Linux_Fab_5.8.x.zip."
  echo "  4. Deixe os ZIPs em ~/Downloads (ou ~/Transferências)."
  echo "  5. Execute ESTE MESMO script novamente."
  command -v xdg-open >/dev/null 2>&1 && xdg-open 'https://www.unrealengine.com/en-US/linux' >/dev/null 2>&1 || true
  exit 2
fi
UE_ROOT="$RESOLVED_UE"
export UE_ROOT
persist_ue_root "$UE_ROOT"
echo "UE_ROOT=$UE_ROOT"
chmod +x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" 2>/dev/null || true
chmod +x "$UE_ROOT/Engine/Build/BatchFiles/Linux/"*.sh 2>/dev/null || true

step "4/8 - VALIDANDO GPU / VULKAN"
preflight_vulkan

step "5/8 - CONFIGURANDO FAB PARA LINUX (SE O ZIP ESTIVER BAIXADO)"
install_fab_if_downloaded
if [[ ! -f "$UE_ROOT/Engine/Plugins/Marketplace/Fab/Fab.uplugin" && ! -f "$UE_ROOT/Engine/Plugins/Fab/Fab.uplugin" ]]; then
  echo "Fab Plugin nao instalado. Isso NAO impede o primeiro teste com os fallbacks do projeto."
fi

step "6/8 - VALIDANDO ASSETS FAB JA PRESENTES NO PROJETO"
if (( SKIP_ASSET_CHECK )); then
  echo "Asset check ignorado."
elif [[ -f "$DESTINATION/scripts/verify-fab-assets-linux.sh" ]]; then
  bash "$DESTINATION/scripts/verify-fab-assets-linux.sh" "$DESTINATION" || true
fi

step "7/8 - PREPARANDO TOOLCHAIN NATIVO"
TOOLCHAIN="$UE_ROOT/Engine/Build/BatchFiles/Linux/SetupToolchain.sh"
MARKER="$HOME/.cache/new-world2/ue58-toolchain.ready"
if [[ -x "$TOOLCHAIN" && ! -f "$MARKER" ]]; then
  mkdir -p "$(dirname "$MARKER")"
  echo "Executando SetupToolchain.sh da propria UE..."
  "$TOOLCHAIN"
  date --iso-8601=seconds > "$MARKER"
else
  echo "Toolchain ja preparado ou nao necessario para este installed build."
fi

step "8/8 - COMPILANDO E ABRINDO O GAME"
ARGS=(--destination "$DESTINATION" --ue-root "$UE_ROOT" --skip-toolchain --profile)
(( SKIP_WORLD_PARTITION )) && ARGS+=(--skip-world-partition)

# Nao usamos exec aqui: se o build/game falhar, este bootstrap ainda consegue registrar
# a etapa, o exit code e o caminho do log antes de devolver o controle ao terminal.
bash "$DESTINATION/scripts/clone-build-run-linux.sh" "${ARGS[@]}"
FINAL_STATUS=$?

echo
echo "============================================================"
echo " BOOTSTRAP FINALIZADO"
echo "============================================================"
echo "Exit code: $FINAL_STATUS"
echo "Log completo: $BOOTSTRAP_LOG"
exit "$FINAL_STATUS"
