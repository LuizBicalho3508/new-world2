#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
CONTENT_DIR="$PROJECT_DIR/Content"
DEST_DIR="$CONTENT_DIR/Characters/Mannequins"

log() { printf '%s\n' "$*"; }

has_manny() {
    find "$CONTENT_DIR" -type f \( -iname 'SKM_Manny*.uasset' -o -iname 'SKM_UEFN_Mannequin*.uasset' \) -print -quit 2>/dev/null | grep -q .
}

has_manny_anim() {
    find "$CONTENT_DIR" -type f \( -iname 'ABP_Manny.uasset' -o -iname 'ABP_SandboxCharacter.uasset' \) -print -quit 2>/dev/null | grep -q .
}

log "============================================================"
log " NEW WORLD 2 - PREPARAR CORPO NEUTRO V8 / LINUX"
log "============================================================"
log "Projeto : $PROJECT_DIR"
log "UE      : $UE_ROOT"
log

mkdir -p "$CONTENT_DIR"

if has_manny && has_manny_anim; then
    log "[OK] Manny/UEFN Mannequin e Animation Blueprint ja existem no projeto."
    find "$CONTENT_DIR" -type f \( -iname 'SKM_Manny*.uasset' -o -iname 'SKM_UEFN_Mannequin*.uasset' -o -iname 'ABP_Manny.uasset' -o -iname 'ABP_SandboxCharacter.uasset' \) -print | head -20
    exit 0
fi

log "[1/3] Procurando Manny nos templates descompactados da Unreal..."
SOURCE_MESH=""
for ROOT in "$UE_ROOT/Templates" "$UE_ROOT/Samples" "$UE_ROOT/FeaturePacks"; do
    [[ -d "$ROOT" ]] || continue
    SOURCE_MESH="$(find "$ROOT" -type f \( -iname 'SKM_Manny.uasset' -o -iname 'SKM_Manny_Simple.uasset' \) -print -quit 2>/dev/null || true)"
    [[ -n "$SOURCE_MESH" ]] && break
done

if [[ -n "$SOURCE_MESH" ]]; then
    SOURCE_CONTENT="$(dirname "$SOURCE_MESH")"
    while [[ "$SOURCE_CONTENT" != "/" && "$(basename "$SOURCE_CONTENT")" != "Content" ]]; do
        SOURCE_CONTENT="$(dirname "$SOURCE_CONTENT")"
    done

    if [[ "$(basename "$SOURCE_CONTENT")" == "Content" && -d "$SOURCE_CONTENT/Characters/Mannequins" ]]; then
        log "Template encontrado: $SOURCE_CONTENT/Characters/Mannequins"
        mkdir -p "$CONTENT_DIR/Characters"
        cp -a "$SOURCE_CONTENT/Characters/Mannequins" "$CONTENT_DIR/Characters/"
        log "[OK] Manny copiado do template local da UE para Content/Characters/Mannequins."
    fi
fi

if has_manny && has_manny_anim; then
    exit 0
fi

log
log "[2/3] Procurando Manny dentro de arquivos .upack..."
TMP_PY="$(mktemp)"
trap 'rm -f "$TMP_PY"' EXIT
cat > "$TMP_PY" <<'PY'
import os
import sys
import zipfile

ue_root, project_root = sys.argv[1:3]
prefixes = (
    "Content/Characters/Mannequins/",
)
archives = []
for base in ("Templates", "FeaturePacks", "Samples"):
    root = os.path.join(ue_root, base)
    if not os.path.isdir(root):
        continue
    for current, _, files in os.walk(root):
        for name in files:
            if name.lower().endswith((".upack", ".zip")):
                archives.append(os.path.join(current, name))

for archive in archives:
    try:
        with zipfile.ZipFile(archive) as zf:
            names = zf.namelist()
            lower = [n.lower() for n in names]
            if not any("skm_manny" in n for n in lower):
                continue
            extracted = 0
            for member in names:
                normalized = member.replace("\\", "/")
                if not normalized.startswith(prefixes):
                    continue
                if normalized.endswith("/"):
                    continue
                target = os.path.abspath(os.path.join(project_root, normalized))
                project_abs = os.path.abspath(project_root) + os.sep
                if not target.startswith(project_abs):
                    continue
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with zf.open(member) as src, open(target, "wb") as dst:
                    dst.write(src.read())
                extracted += 1
            if extracted:
                print(f"EXTRACTED={extracted}")
                print(f"ARCHIVE={archive}")
                raise SystemExit(0)
    except (zipfile.BadZipFile, OSError):
        continue
raise SystemExit(2)
PY

set +e
python3 "$TMP_PY" "$UE_ROOT" "$PROJECT_DIR"
PY_RC=$?
set -e

if (( PY_RC == 0 )) && has_manny && has_manny_anim; then
    log "[OK] Manny extraido de pacote de template da UE."
    exit 0
fi

log
log "[3/3] Resultado"
if has_manny; then
    log "[PARCIAL] Mesh Manny encontrada, mas o Animation Blueprint esperado nao foi localizado."
else
    log "[FALTA] Manny/UEFN Mannequin nao foi encontrado automaticamente nesta instalacao da UE."
fi

cat <<'EOF'

Para o visual modular premium, use UMA destas opcoes gratuitas:
  1) Unreal Engine -> Add Feature or Content Pack -> Third Person -> Add to Project
     (isso adiciona Manny/Quinn e ABP_Manny), ou
  2) Fab -> Game Animation Sample -> Add to Project
     (isso adiciona UEFN Mannequin + ABP_SandboxCharacter / Motion Matching).

A V8 nao apaga o Greystone. Se o corpo neutro nao existir, ele continua apenas como fallback.
Assim que Manny/UEFN estiver em Content/, a V8 passa a preferi-lo automaticamente.
EOF

exit 0
