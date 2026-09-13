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

BUILD_LOG="$HOME/nw2-premium-v7-build.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
GPU_LOG="$HOME/nw2-premium-v7-gpu.csv"
ASSET_REPORT="$HOME/nw2-premium-v7-assets.txt"

usage() {
cat <<'EOF'
Uso: premium-v7-test-biglinux.sh [opcoes]
  --ue-root PATH          Raiz da Unreal Engine 5.8
  --fps N                 Limite de FPS do playtest (padrao: 60)
  --resolution LxA        Resolucao do jogo; auto se omitida
  --max-parallel N        Maximo de acoes paralelas do UBT (padrao: 3)
  --profile                Mostra stat unit/game/gpu/fps durante o teste
  -h, --help               Mostra esta ajuda

Este script NAO apaga Content/ e NAO baixa assets licenciados da Fab.
Ele audita Content/, compila o projeto, abre o jogo e resume os principais logs V7.
EOF
}

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; shift 2 ;;
        --fps) FPS_LIMIT="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        --profile) PROFILE=1; shift ;;
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

[[ -f "$PROJECT_FILE" ]] || fail "NewWorld2.uproject ausente em $PROJECT_DIR"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
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
 NEW WORLD 2 - PREMIUM GAMEPLAY RECOVERY V7 / BIGLINUX
============================================================
Projeto       : $PROJECT_DIR
Branch        : $BRANCH
Commit        : $COMMIT
UE            : $UE_ROOT
Resolucao     : $RESOLUTION
FPS alvo      : $FPS_LIMIT
UBT jobs      : $MAX_PARALLEL
Profile       : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)
Build log     : $BUILD_LOG
Runtime log   : $RUNTIME_LOG
GPU log       : $GPU_LOG
Assets report : $ASSET_REPORT
============================================================
EOF

printf '\n[1/8] Preflight de codigo Premium V7...\n'
required=(
  Source/NewWorld2/NWEnemyVisualDirector.cpp
  Source/NewWorld2/NWEnemyAnimationDirector.cpp
  Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp
  Source/NewWorld2/NWCombatHUDWidget.cpp
  Source/NewWorld2/NWStartupWarmupDirector.cpp
  Source/NewWorld2/NWPremiumV6CombatDirector.cpp
  Source/NewWorld2/NWPremiumEnvironmentDirector.cpp
  Source/NewWorld2/NWPremiumVFXDirector.cpp
  scripts/play-biglinux.sh
  scripts/verify-fab-assets-linux.sh
)
for f in "${required[@]}"; do [[ -f "$f" ]] || fail "arquivo obrigatorio ausente: $f"; done

grep -q '\[MOB-VISUAL-V7\]' Source/NewWorld2/NWEnemyVisualDirector.cpp || fail "enemy visual V7 ausente"
grep -q '\[MOB-ANIM-V7\]' Source/NewWorld2/NWEnemyAnimationDirector.cpp || fail "enemy animation V7 ausente"
grep -q '\[PLAYER-GEAR-V7\]' Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp || fail "gear visual V7 ausente"
grep -q 'FindStaticArmorMesh' Source/NewWorld2/NWPlayerEquipmentVisualDirector.cpp || fail "fallback StaticMesh de armadura ausente"
grep -q 'AbilityIconGlyph' Source/NewWorld2/NWCombatHUDWidget.cpp || fail "icones de habilidade V7 ausentes"
grep -q '\[HUD-V7\]' Source/NewWorld2/NWCombatHUDWidget.cpp || fail "HUD V7 ausente"
grep -q '\[STARTUP-V7\]' Source/NewWorld2/NWStartupWarmupDirector.cpp || fail "startup warmup V7 ausente"
grep -q 'PollForCompilationComplete' Source/NewWorld2/NWStartupWarmupDirector.cpp || fail "warmup Niagara compilavel ausente"
grep -q '^t.MaxFPS=60$' Config/DefaultEngine.ini || fail "DefaultEngine nao esta em 60 FPS"
grep -q '^r.PSOPrecache.ProxyCreationWhenPSOReady=1$' Config/DefaultEngine.ini || fail "PSO ProxyCreationWhenPSOReady ausente"
grep -q '^r.PSOPrecache.ProxyCreationDelayStrategy=0$' Config/DefaultEngine.ini || fail "PSO DelayStrategy ausente"

echo "OK: contratos V7 encontrados."

printf '\n[2/8] Validando Unreal Engine 5.8, Vulkan e GPU...\n'
python3 - "$UE_ROOT/Engine/Build/Build.version" <<'PY'
import json, sys
path = sys.argv[1]
with open(path, encoding="utf-8") as f:
    v = json.load(f)
major = int(v.get("MajorVersion", 0))
minor = int(v.get("MinorVersion", 0))
patch = int(v.get("PatchVersion", 0))
print(f"UE detectada: {major}.{minor}.{patch}")
if (major, minor) != (5, 8):
    raise SystemExit("ERRO: Premium V7 foi validado para UE 5.8")
PY

if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary >/tmp/nw2-v7-vulkan.txt 2>&1 || fail "Vulkan indisponivel"
    grep -E 'deviceName|driverName|driverInfo|apiVersion' /tmp/nw2-v7-vulkan.txt | head -20 || true
else
    echo "AVISO: vulkaninfo nao encontrado; Unreal ainda tentara Vulkan."
fi
command -v nvidia-smi >/dev/null 2>&1 && \
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true

printf '\n[3/8] Auditando assets Fab/Epic instalados localmente...\n'
: > "$ASSET_REPORT"
set +e
bash "$PROJECT_DIR/scripts/verify-fab-assets-linux.sh" "$PROJECT_DIR" | tee "$ASSET_REPORT"
ASSET_RC=${PIPESTATUS[0]}
set -e
(( ASSET_RC == 0 )) || echo "AVISO: auditoria de assets retornou RC=$ASSET_RC; build continuara."

# Os nomes abaixo sao recomendacoes, nao requisitos de compilacao.
# Encontrar um .uasset com estes tokens significa que o pack foi realmente adicionado ao projeto local.
TMP_ASSETS="$(mktemp)"
trap 'rm -f "$TMP_ASSETS"' EXIT
find "$PROJECT_DIR/Content" -type f -iname '*.uasset' -print 2>/dev/null | tr '[:upper:]' '[:lower:]' > "$TMP_ASSETS" || true

asset_present() {
    local token
    for token in "$@"; do
        grep -Fqi -- "$token" "$TMP_ASSETS" && return 0
    done
    return 1
}

recommended_enemy=0
for spec in \
    'Paragon Minions|paragonminions|minions' \
    'Paragon Grux|paragongrux|grux' \
    'Paragon Khaimera|paragonkhaimera|khaimera' \
    'Paragon Rampage|paragonrampage|rampage' \
    'Paragon Sevarog|paragonsevarog|sevarog' \
    'Paragon Revenant|paragonrevenant|revenant' \
    'Paragon Countess|paragoncountess|countess'; do
    IFS='|' read -r label token1 token2 <<<"$spec"
    if asset_present "$token1" "$token2"; then
        echo "[V7-ASSET] OK inimigo: $label"
        recommended_enemy=$((recommended_enemy + 1))
    else
        echo "[V7-ASSET] FALTA recomendado: $label"
    fi
done

if (( recommended_enemy < 2 )); then
    cat <<'EOF'
[V7-ASSET] AVISO: ha poucos packs de criatura instalados.
Para variedade visual real, abra Unreal Engine 5.8 -> Fab -> Library e use "Add to Project"
principalmente em Paragon Minions, Grux, Khaimera, Rampage, Sevarog, Revenant e Countess.
O codigo V7 detecta esses assets automaticamente quando eles existirem em Content/.
EOF
fi

if ! asset_present 'armor' 'armour' 'cuirass' 'gauntlet' 'helmet'; then
    echo "[V7-ASSET] AVISO: nenhuma familia de armadura modular/estatica realista foi reconhecida em Content/."
fi
if ! asset_present 'bow' 'recurve' 'longbow'; then
    echo "[V7-ASSET] AVISO: arco realista nao reconhecido; instale Ethereal Recurve Bow ou outro arco da sua biblioteca."
fi

printf '\n[4/8] Encerrando instancias antigas e preservando logs...\n'
pkill -TERM -f 'UnrealEditor.*NewWorld2' 2>/dev/null || true
sleep 2
STAMP="$(date +%Y%m%d-%H%M%S)"
[[ -s "$RUNTIME_LOG" ]] && cp -a "$RUNTIME_LOG" "$HOME/nw2-playable-before-v7-$STAMP.log" || true
[[ -s "$BUILD_LOG" ]] && cp -a "$BUILD_LOG" "$HOME/nw2-premium-v7-build-before-$STAMP.log" || true
: > "$BUILD_LOG"
: > "$GPU_LOG"

printf '\n[5/8] Build incremental NewWorld2Editor...\n'
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" 2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e
if (( BUILD_RC != 0 )); then
    echo "--- ERROS DE BUILD ---"
    grep -a -nE '(^|[[:space:]])(error:|fatal error:)|Result: Failed|OtherCompilationError' "$BUILD_LOG" | tail -n 260 || true
    fail "build Unreal falhou com RC=$BUILD_RC"
fi
echo "OK: build V7 concluido."

printf '\n[6/8] Abrindo playtest Premium V7...\n'
GPU_PID=""
cleanup_gpu() { [[ -n "$GPU_PID" ]] && kill "$GPU_PID" 2>/dev/null || true; }
trap 'cleanup_gpu; rm -f "$TMP_ASSETS"' EXIT INT TERM
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
bash "$PROJECT_DIR/scripts/play-biglinux.sh" "${PLAY_ARGS[@]}"
GAME_RC=$?
set -e
cleanup_gpu
GPU_PID=""

printf '\n[7/8] Diagnostico automatico do runtime...\n'
if [[ -s "$RUNTIME_LOG" ]]; then
    echo "--- STARTUP / PERFORMANCE ---"
    grep -a -E '\[STARTUP-V7\]|PSO creation hitches|PSOPrecach|ShaderPipelineCache|Compiling System NiagaraSystem' "$RUNTIME_LOG" | tail -n 180 || true

    echo "--- INIMIGOS / ANIMACAO ---"
    grep -a -E '\[MOB-VISUAL-V7\]|\[MOB-ANIM-V7\]|\[PLAYTEST-MOB\]|\[WORLD BOSS\]' "$RUNTIME_LOG" | tail -n 220 || true

    echo "--- PLAYER / GEAR / HUD ---"
    grep -a -E '\[PLAYER-GEAR-V7\]|\[HUD-V7\]|\[EQUIP\]|\[ARMA\]|\[LOADOUT\]' "$RUNTIME_LOG" | tail -n 220 || true

    echo "--- COMBATE / VFX ---"
    grep -a -E '\[ABILITY-V6\]|\[ABILITY-CHECK\]|\[ABILITY-ASSIST\]|\[VFX-V6\]|\[COMBO\]' "$RUNTIME_LOG" | tail -n 260 || true

    echo "--- AMBIENTE ---"
    grep -a -E '\[ENV-PREMIUM\]|\[ENV-V6\]|\[SKY-V6\]|\[BIOMA\]' "$RUNTIME_LOG" | tail -n 160 || true

    echo "--- REGRESSOES / ERROS IMPORTANTES ---"
    grep -a -Ei 'Fatal error|LowLevelFatalError|Segmentation fault|SIGSEGV|GPU crash|device lost|Out of memory|Assertion failed|Unhandled Exception|LogBlueprint: Error|Divide by zero|Failed to find object' "$RUNTIME_LOG" | tail -n 180 || true

    GREYSTONE_ENEMIES="$(grep -a '\[MOB-VISUAL-V7\]' "$RUNTIME_LOG" | grep -ci '/ParagonGreystone/' || true)"
    UNIQUE_MOB_MESHES="$(grep -a '\[MOB-VISUAL-V7\]' "$RUNTIME_LOG" | sed -n 's/.* -> \([^ ]*\) |.*/\1/p' | sort -u | wc -l | tr -d ' ')"
    STATIC_ARMOR_COUNT="$(grep -ac '\[PLAYER-GEAR-V7\] armadura StaticMesh visivel' "$RUNTIME_LOG" || true)"
    MODULAR_ARMOR_COUNT="$(grep -ac '\[PLAYER-GEAR-V7\] armadura modular visivel' "$RUNTIME_LOG" || true)"
    STARTUP_TIMEOUTS="$(grep -ac '\[STARTUP-V7\].*motivo=timeout' "$RUNTIME_LOG" || true)"

    echo
    echo "Resumo V7:"
    echo "  meshes unicos de inimigo detectados : $UNIQUE_MOB_MESHES"
    echo "  inimigos V7 ainda usando Greystone  : $GREYSTONE_ENEMIES"
    echo "  armaduras StaticMesh visualizadas    : $STATIC_ARMOR_COUNT"
    echo "  armaduras modulares visualizadas     : $MODULAR_ARMOR_COUNT"
    echo "  startup gates que atingiram timeout  : $STARTUP_TIMEOUTS"

    if (( GREYSTONE_ENEMIES > 0 )); then
        echo "AVISO: ainda houve fallback Greystone em inimigos. Instale mais packs de criatura e envie este log para ajuste fino."
    fi
    if (( UNIQUE_MOB_MESHES < 2 )); then
        echo "AVISO: variedade visual baixa no teste; confirme os packs recomendados em Content/."
    fi
else
    echo "AVISO: runtime log vazio: $RUNTIME_LOG"
fi

printf '\n[8/8] Resumo de GPU e encerramento...\n'
if [[ -s "$GPU_LOG" ]]; then
    awk -F',' '
    {g=$2+0; m=$4+0; n++; sg+=g; if(g>mg)mg=g; if(m>mm)mm=m}
    END{if(n) printf("GPU media %.1f%% | pico %.0f%% | VRAM pico %.0f MiB | amostras %d\n",sg/n,mg,mm,n); else print "sem amostras"}
    ' "$GPU_LOG"
else
    echo "Monitor de GPU indisponivel ou sem amostras."
fi

rm -f "$TMP_ASSETS"
trap - EXIT INT TERM

echo
echo "============================================================"
echo " PREMIUM V7 ENCERRADO - GAME RC=$GAME_RC"
echo " Build   : $BUILD_LOG"
echo " Runtime : $RUNTIME_LOG"
echo " GPU     : $GPU_LOG"
echo " Assets  : $ASSET_REPORT"
echo "============================================================"

# Ctrl+C fecha o editor de forma intencional no playtest Linux.
if (( GAME_RC == 130 )); then exit 0; fi
exit "$GAME_RC"
