#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
CONTENT_DIR="$PROJECT_DIR/Content"

log() { printf '%s\n' "$*"; }

find_neutral_mesh() {
    find "$CONTENT_DIR" -type f \( -iname 'SKM_Manny*.uasset' -o -iname 'SKM_Quinn*.uasset' -o -iname 'SKM_UEFN_Mannequin*.uasset' \) -print -quit 2>/dev/null || true
}

find_neutral_anim() {
    find "$CONTENT_DIR" -type f \( -iname 'ABP_Manny.uasset' -o -iname 'ABP_Quinn.uasset' -o -iname 'ABP_SandboxCharacter.uasset' \) -print -quit 2>/dev/null || true
}

has_complete_neutral() {
    [[ -n "$(find_neutral_mesh)" && -n "$(find_neutral_anim)" ]]
}

copy_content_subtree() {
    local source_content="$1"
    local subtree="$2"
    [[ -d "$source_content/$subtree" ]] || return 1
    mkdir -p "$CONTENT_DIR/$(dirname "$subtree")"
    cp -a "$source_content/$subtree" "$CONTENT_DIR/$(dirname "$subtree")/"
}

log "============================================================"
log " NEW WORLD 2 - PREPARAR AVATAR NEUTRO V9 / LINUX"
log "============================================================"
log "Projeto : $PROJECT_DIR"
log "UE      : $UE_ROOT"
log

mkdir -p "$CONTENT_DIR"

if has_complete_neutral; then
    log "[OK] corpo neutro + Animation Blueprint ja existem no projeto."
    log "Mesh: $(find_neutral_mesh)"
    log "Anim: $(find_neutral_anim)"
    exit 0
fi

log "[1/4] Procurando Third Person/Manny descompactado dentro da UE..."
# Different UE distributions put template content at different nesting levels.
# Search broadly, then recover the real Content root from each match.
while IFS= read -r ASSET; do
    [[ -n "$ASSET" ]] || continue
    CUR="$(dirname "$ASSET")"
    CONTENT_ROOT=""
    while [[ "$CUR" != "/" ]]; do
        if [[ "$(basename "$CUR")" == "Content" ]]; then CONTENT_ROOT="$CUR"; break; fi
        CUR="$(dirname "$CUR")"
    done
    [[ -n "$CONTENT_ROOT" ]] || continue

    if [[ -d "$CONTENT_ROOT/Characters/Mannequins" ]]; then
        log "Template unpacked: $CONTENT_ROOT/Characters/Mannequins"
        copy_content_subtree "$CONTENT_ROOT" "Characters/Mannequins" || true
    fi
    if [[ -d "$CONTENT_ROOT/ThirdPerson" ]]; then
        copy_content_subtree "$CONTENT_ROOT" "ThirdPerson" || true
    fi
    has_complete_neutral && break
done < <(find "$UE_ROOT" -type f \( -iname 'SKM_Manny*.uasset' -o -iname 'ABP_Manny.uasset' \) -print 2>/dev/null | head -80)

if has_complete_neutral; then
    log "[OK] Manny/Quinn copiado de template descompactado."
    log "Mesh: $(find_neutral_mesh)"
    log "Anim: $(find_neutral_anim)"
    exit 0
fi

log
log "[2/4] Procurando Manny dentro de .upack/.zip..."
TMP_PY="$(mktemp)"
trap 'rm -f "$TMP_PY"' EXIT
cat > "$TMP_PY" <<'PY'
import os
import sys
import zipfile

ue_root, project_root = sys.argv[1:3]
needles = (
    "content/characters/mannequins/",
    "content/thirdperson/",
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
            low = [n.replace("\\", "/").lower() for n in names]
            if not any("skm_manny" in n for n in low):
                continue
            if not any("abp_manny" in n or "abp_quinn" in n for n in low):
                continue

            extracted = 0
            for member in names:
                normalized = member.replace("\\", "/")
                lowered = normalized.lower()
                pos = -1
                for needle in needles:
                    found = lowered.find(needle)
                    if found >= 0 and (pos < 0 or found < pos):
                        pos = found
                if pos < 0 or normalized.endswith("/"):
                    continue

                # Important V9 fix: FeaturePack archives may prefix members with
                # TP_ThirdPerson/.../Content/. Strip everything before Content/.
                relative = normalized[pos:]
                if not relative.lower().startswith("content/"):
                    continue
                target = os.path.abspath(os.path.join(project_root, relative))
                project_abs = os.path.abspath(project_root) + os.sep
                if not target.startswith(project_abs):
                    continue
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with zf.open(member) as src, open(target, "wb") as dst:
                    dst.write(src.read())
                extracted += 1

            if extracted:
                print(f"[V9-NEUTRAL] EXTRACTED={extracted}")
                print(f"[V9-NEUTRAL] ARCHIVE={archive}")
                raise SystemExit(0)
    except (zipfile.BadZipFile, OSError):
        continue
raise SystemExit(2)
PY

set +e
python3 "$TMP_PY" "$UE_ROOT" "$PROJECT_DIR"
PY_RC=$?
set -e

if (( PY_RC == 0 )) && has_complete_neutral; then
    log "[OK] Manny/Quinn extraido de Feature Pack com mesh + AnimBP."
    log "Mesh: $(find_neutral_mesh)"
    log "Anim: $(find_neutral_anim)"
    exit 0
fi

log
log "[3/4] Verificando Game Animation Sample ja instalado localmente..."
GAS_MESH="$(find "$CONTENT_DIR" -type f -iname '*UEFN*Mannequin*.uasset' -print -quit 2>/dev/null || true)"
GAS_ANIM="$(find "$CONTENT_DIR" -type f -iname 'ABP_SandboxCharacter.uasset' -print -quit 2>/dev/null || true)"
if [[ -n "$GAS_MESH" && -n "$GAS_ANIM" ]]; then
    log "[OK] Game Animation Sample ja fornece base neutra completa."
    exit 0
fi

log
log "[4/4] Resultado"
MESH="$(find_neutral_mesh)"
ANIM="$(find_neutral_anim)"
[[ -n "$MESH" ]] && log "[PARCIAL] Mesh encontrada: $MESH" || log "[FALTA] Mesh Manny/Quinn/UEFN."
[[ -n "$ANIM" ]] && log "[PARCIAL] AnimBP encontrado: $ANIM" || log "[FALTA] ABP_Manny/ABP_Quinn/ABP_SandboxCharacter."

cat <<'EOF'

A V9 nao vai mascarar este erro usando Greystone como se fosse corpo modular.
Para completar o avatar gratuito, no Unreal 5.8 use uma das opcoes:
  1) Add Feature or Content Pack -> Third Person -> Add to Project
  2) Fab -> Game Animation Sample -> Add to Project

Depois rode novamente o teste V9. Os assets locais continuam ignorados pelo Git.
EOF

exit 0
