#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
FPS_LIMIT=60
RESOLUTION=""
MAX_PARALLEL=3
PROFILE=0
SKIP_ASSET_REPAIR=0

BUILD_LOG="$HOME/nw2-premium-v9-build.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
GPU_LOG="$HOME/nw2-premium-v9-gpu.csv"
ASSET_REPORT="$HOME/nw2-premium-v9-assets.txt"
REPAIR_LOG="$HOME/nw2-v9-environment-repair.log"

usage() {
cat <<'EOF'
Uso: premium-v9-test-biglinux.sh [opcoes]
  --ue-root PATH          Raiz da Unreal Engine 5.8
  --fps N                 Limite de FPS (padrao: 60)
  --resolution LxA        Resolucao; auto se omitida
  --max-parallel N        Jobs paralelos UBT (padrao: 3)
  --profile               Mostra stats de profiling na tela
  --skip-asset-repair     Nao executa o resave/material repair V9
  -h, --help              Ajuda

O modo padrao abre o jogo LIMPO, sem stat game/unit/gpu/fps.
EOF
}

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; shift 2 ;;
        --fps) FPS_LIMIT="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        --profile) PROFILE=1; shift ;;
        --skip-asset-repair) SKIP_ASSET_REPAIR=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Argumento desconhecido: $1" >&2; usage; exit 2 ;;
    esac
done

fail() {
    echo "[FALHA] $*" >&2
    echo "Build   : $BUILD_LOG" >&2
    echo "Runtime : $RUNTIME_LOG" >&2
    echo "Assets  : $ASSET_REPORT" >&2
    exit 1
}

EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
[[ -f "$PROJECT_FILE" ]] || fail "NewWorld2.uproject ausente"
[[ -x "$EDITOR" ]] || fail "UnrealEditor ausente: $EDITOR"
[[ -x "$BUILD_SH" ]] || fail "Build.sh ausente: $BUILD_SH"
[[ "$FPS_LIMIT" =~ ^[0-9]+$ ]] || fail "fps invalido: $FPS_LIMIT"
[[ "$MAX_PARALLEL" =~ ^[0-9]+$ ]] || fail "max-parallel invalido: $MAX_PARALLEL"

if [[ -z "$RESOLUTION" ]]; then
    SCREEN_MODE=""
    command -v xrandr >/dev/null 2>&1 && SCREEN_MODE="$(xrandr --current 2>/dev/null | awk '/\*/ {print $1; exit}')"
    if [[ "$SCREEN_MODE" =~ ^([0-9]+)x([0-9]+)$ ]] && (( BASH_REMATCH[1] >= 1700 )); then
        RESOLUTION="1600x900"
    else
        RESOLUTION="1280x720"
    fi
fi
[[ "$RESOLUTION" =~ ^[0-9]+x[0-9]+$ ]] || fail "resolucao invalida: $RESOLUTION"

cd "$PROJECT_DIR"
git config core.fileMode false || true
BRANCH="$(git branch --show-current 2>/dev/null || true)"
COMMIT="$(git rev-parse --short=12 HEAD 2>/dev/null || echo sem-git)"

cat <<EOF
============================================================
 NEW WORLD 2 - PREMIUM HYBRID WORLD V9 / BIGLINUX
============================================================
Projeto       : $PROJECT_DIR
Branch        : $BRANCH
Commit        : $COMMIT
UE            : $UE_ROOT
Resolucao     : $RESOLUTION
FPS alvo      : $FPS_LIMIT
UBT jobs      : $MAX_PARALLEL
Overlay stat  : $([[ $PROFILE -eq 1 ]] && echo LIGADO || echo DESLIGADO)
Asset repair  : $([[ $SKIP_ASSET_REPAIR -eq 1 ]] && echo PULADO || echo AUTO)
Build log     : $BUILD_LOG
Runtime log   : $RUNTIME_LOG
GPU log       : $GPU_LOG
Assets report : $ASSET_REPORT
============================================================
EOF

printf '\n[1/10] Preflight V9...\n'
required=(
  Source/NewWorld2/NWProceduralWorldManager.cpp
  Source/NewWorld2/NWPremiumEnvironmentDirector.cpp
  Source/NewWorld2/NWPremiumSkyDirector.cpp
  Source/NewWorld2/NWDungeonSite.cpp
  Source/NewWorld2/NWCombatHUDWidget.cpp
  Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp
  scripts/prepare-neutral-player-linux.sh
  scripts/repair-environment-assets-linux.sh
  scripts/nw2_repair_environment_assets.py
  scripts/play-biglinux.sh
)
for f in "${required[@]}"; do [[ -f "$f" ]] || fail "arquivo obrigatorio ausente: $f"; done

grep -q '\[WORLD-V9\]' Source/NewWorld2/NWProceduralWorldManager.cpp || fail "macro world V9 ausente"
grep -q '\[TERRAIN-V9\]' Source/NewWorld2/NWProceduralWorldManager.cpp || fail "terrain V9 ausente"
grep -q '\[ENV-V9\]' Source/NewWorld2/NWPremiumEnvironmentDirector.cpp || fail "environment V9 ausente"
grep -q 'CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes)' Source/NewWorld2/NWPremiumEnvironmentDirector.cpp || fail "HISM material guard ausente"
grep -q 'bThemeKeywordMatched' Source/NewWorld2/NWDungeonSite.cpp || fail "dungeon theme guard ausente"
grep -q '\[HUD-V9\]' Source/NewWorld2/NWCombatHUDWidget.cpp || fail "HUD V9 ausente"
if grep -q 'SpawnSingletonActor<ANWPremiumV6EnvironmentBooster>' Source/NewWorld2/NWGameMode.cpp; then
    fail "booster V6 duplicado voltou ao boot"
fi
echo "OK: contratos V9 encontrados."

printf '\n[2/10] Validando UE/Vulkan/GPU...\n'
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    v = json.load(f)
version = (int(v.get("MajorVersion", 0)), int(v.get("MinorVersion", 0)), int(v.get("PatchVersion", 0)))
print(f"UE detectada: {version[0]}.{version[1]}.{version[2]}")
if version[:2] != (5, 8):
    raise SystemExit("ERRO: V9 foi preparada para UE 5.8")
PY
if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v9-vulkan.txt 2>&1 || fail "Vulkan indisponivel"
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v9-vulkan.txt | head -20 || true
fi
command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true

printf '\n[3/10] Preparando avatar neutro...\n'
chmod +x scripts/prepare-neutral-player-linux.sh
PROJECT_DIR="$PROJECT_DIR" UE_ROOT="$UE_ROOT" bash scripts/prepare-neutral-player-linux.sh || true
NEUTRAL_MESH="$(find Content -type f \( -iname 'SKM_Manny*.uasset' -o -iname 'SKM_Quinn*.uasset' -o -iname 'SKM_UEFN_Mannequin*.uasset' \) -print -quit 2>/dev/null || true)"
NEUTRAL_ANIM="$(find Content -type f \( -iname 'ABP_Manny.uasset' -o -iname 'ABP_Quinn.uasset' -o -iname 'ABP_SandboxCharacter.uasset' \) -print -quit 2>/dev/null || true)"
if [[ -n "$NEUTRAL_MESH" && -n "$NEUTRAL_ANIM" ]]; then
    echo "[V9-PLAYER] OK mesh: $NEUTRAL_MESH"
    echo "[V9-PLAYER] OK anim: $NEUTRAL_ANIM"
else
    echo "[V9-PLAYER] AVISO: avatar neutro completo ainda ausente; runtime manterá fallback atual."
fi

printf '\n[4/10] Reparando assets legados de ambiente...\n'
chmod +x scripts/repair-environment-assets-linux.sh
if (( SKIP_ASSET_REPAIR == 0 )); then
    NW2_ENV_REPAIR_LOG="$REPAIR_LOG" PROJECT_DIR="$PROJECT_DIR" UE_ROOT="$UE_ROOT" bash scripts/repair-environment-assets-linux.sh || true
else
    echo "Reparo persistente pulado por parametro."
fi

printf '\n[5/10] Auditando Fab/Epic local...\n'
: > "$ASSET_REPORT"
set +e
bash scripts/verify-fab-assets-linux.sh "$PROJECT_DIR" | tee "$ASSET_REPORT"
ASSET_RC=${PIPESTATUS[0]}
set -e
(( ASSET_RC == 0 )) || echo "AVISO: auditoria retornou RC=$ASSET_RC; build continuara."

printf '\n[6/10] Encerrando instancias antigas/preservando logs...\n'
pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
sleep 2
STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v9-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v9-build-before-$STAMP.log" || true
: > "$BUILD_LOG"
: > "$GPU_LOG"

printf '\n[7/10] Build incremental NewWorld2Editor...\n'
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e
if (( BUILD_RC != 0 )); then
    echo "--- ERROS DE BUILD ---"
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 320 || true
    fail "build Unreal falhou com RC=$BUILD_RC"
fi
echo "OK: build V9 concluido."

printf '\n[8/10] Abrindo jogo...\n'
GPU_PID=""
cleanup_gpu() { [[ -n "$GPU_PID" ]] && kill "$GPU_PID" 2>/dev/null || true; }
trap 'cleanup_gpu' EXIT INT TERM
if command -v nvidia-smi >/dev/null 2>&1; then
    (
      while true; do
        nvidia-smi --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used,memory.total,power.draw,clocks.current.graphics --format=csv,noheader,nounits 2>/dev/null || break
        sleep 2
      done
    ) > "$GPU_LOG" & GPU_PID=$!
fi

PLAY_ARGS=(--ue-root "$UE_ROOT" --fps "$FPS_LIMIT" --resolution "$RESOLUTION" --max-parallel "$MAX_PARALLEL" --skip-build)
(( PROFILE )) && PLAY_ARGS+=(--profile)
set +e
bash scripts/play-biglinux.sh "${PLAY_ARGS[@]}"
GAME_RC=$?
set -e
cleanup_gpu
GPU_PID=""

printf '\n[9/10] Diagnostico V9...\n'
if [[ -s "$RUNTIME_LOG" ]]; then
    echo "--- WORLD / AMBIENTE ---"
    grep -a -E '\[WORLD-V9\]|\[TERRAIN-V9\]|\[ENV-V9\]|\[SKY-V9\]|\[DUNGEON-V9\]' "$RUNTIME_LOG" | tail -n 220 || true

    echo "--- PLAYER / EQUIPAMENTO ---"
    grep -a -E '\[PLAYER-BASE-V8\]|\[PLAYER-GEAR-V8\]|\[EQUIP\]|\[ARMA\]|\[LOADOUT\]' "$RUNTIME_LOG" | tail -n 220 || true

    echo "--- PERFORMANCE ---"
    grep -a -E '\[STARTUP-V8\]|PSO creation hitches|PSOPrecach|Compiling System NiagaraSystem' "$RUNTIME_LOG" | tail -n 180 || true

    echo "--- ERROS IMPORTANTES ---"
    grep -a -Ei 'Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|GPU crash|device lost|Out of memory|Assertion failed|Unhandled Exception|LogBlueprint: Error|Divide by zero|Default Material will be used|missing usage flag InstancedStaticMeshes|recomputing physics on load|Failed to find object' "$RUNTIME_LOG" | tail -n 240 || true

    WORLD_OK="$(grep -ac '\[WORLD-V9\] macro mundo HIBRIDO fixo' "$RUNTIME_LOG" || true)"
    TERRAIN_OK="$(grep -ac '\[TERRAIN-V9\] macro fixo criado' "$RUNTIME_LOG" || true)"
    ENV_OK="$(grep -ac '\[ENV-V9\] ambiente hibrido pronto' "$RUNTIME_LOG" || true)"
    GRAY_WARN="$(grep -ac 'Default Material will be used in game' "$RUNTIME_LOG" || true)"
    HISM_WARN="$(grep -ac 'missing usage flag InstancedStaticMeshes' "$RUNTIME_LOG" || true)"
    RESAVE_WARN="$(grep -ac 'recomputing physics on load\|should be resaved to improve async compilation performance' "$RUNTIME_LOG" || true)"
    DUNGEON_WEAPON="$(grep -a '\[DUNGEON-V9\]' "$RUNTIME_LOG" | grep -Eic '/SWORD/|/DAGGER/|/SHIELD/|/WEAPON/' || true)"
    BASE_COUNT="$(grep -ac '\[PLAYER-BASE-V8\] corpo neutro ativo' "$RUNTIME_LOG" || true)"
    ARMOR_COUNT="$(grep -ac '\[PLAYER-GEAR-V8\] armadura VESTIDA' "$RUNTIME_LOG" || true)"
    STAFF_COUNT="$(grep -a '\[PLAYER-GEAR-V8\] arma visivel: Cajado Arcano' "$RUNTIME_LOG" | wc -l | tr -d ' ')"
    FONT_WARN="$(grep -a -c 'Could not find Glyph Index.*U+2694\|Could not find Glyph Index.*U+3df' "$RUNTIME_LOG" || true)"
    PSO_LAST="$(grep -a 'PSO creation hitches so far' "$RUNTIME_LOG" | tail -1 || true)"

    echo
    echo "Resumo V9:"
    echo "  hybrid macro world ativo            : $WORLD_OK"
    echo "  terrain V9 construido               : $TERRAIN_OK"
    echo "  ambiente HISM V9 construido         : $ENV_OK"
    echo "  default-material warnings           : $GRAY_WARN"
    echo "  HISM usage warnings                 : $HISM_WARN"
    echo "  resave/physics legacy warnings      : $RESAVE_WARN"
    echo "  dungeon selecionou arma indevida    : $DUNGEON_WEAPON"
    echo "  corpo neutro ativado                : $BASE_COUNT"
    echo "  armaduras realmente vestidas        : $ARMOR_COUNT"
    echo "  cajado visual aplicado              : $STAFF_COUNT"
    echo "  warnings de glyph antigos           : $FONT_WARN"
    [[ -n "$PSO_LAST" ]] && echo "  ultimo contador PSO                 : $PSO_LAST"

    if (( GRAY_WARN > 0 || HISM_WARN > 0 )); then
        echo "AVISO: ainda ha material legado sem permutation HISM; rode NW2_FORCE_ENV_REPAIR=1 no proximo teste."
    fi
    if (( BASE_COUNT == 0 )); then
        echo "AVISO: Manny/UEFN completo ainda nao ativou. O log acima mostra se mesh ou AnimBP faltou."
    fi
    if (( ARMOR_COUNT == 0 )); then
        echo "AVISO: as armaduras instaladas nao compartilham a familia de bones do corpo-base atual; nao sao forçadas para evitar flutuacao."
    fi
    if (( STAFF_COUNT == 0 )); then
        echo "AVISO: nenhum StaticMesh de staff/scepter/wand/polearm foi encontrado em /Game; gameplay do cajado funciona, visual aguarda asset real."
    fi
else
    echo "AVISO: runtime log vazio: $RUNTIME_LOG"
fi

printf '\n[10/10] GPU / encerramento...\n'
if [[ -s "$GPU_LOG" ]]; then
    awk -F',' '
    {g=$2+0; m=$4+0; n++; sg+=g; if(g>mg)mg=g; if(m>mm)mm=m}
    END{if(n) printf("GPU media %.1f%% | pico %.0f%% | VRAM pico %.0f MiB | amostras %d\n",sg/n,mg,mm,n); else print "sem amostras"}
    ' "$GPU_LOG"
fi

trap - EXIT INT TERM

echo
echo "============================================================"
echo " PREMIUM V9 ENCERRADO - GAME RC=$GAME_RC"
echo " Build   : $BUILD_LOG"
echo " Runtime : $RUNTIME_LOG"
echo " GPU     : $GPU_LOG"
echo " Assets  : $ASSET_REPORT"
echo " Repair  : $REPAIR_LOG"
echo "============================================================"

if (( GAME_RC == 130 )); then exit 0; fi
exit "$GAME_RC"
