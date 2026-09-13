#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_DIR/NewWorld2.uproject"
UE_ROOT="${UE_ROOT:-$HOME/Aplicativos/UnrealEngine-5.8}"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
PLAY_SH="$PROJECT_DIR/scripts/play-biglinux.sh"
BUILD_LOG="$HOME/nw2-v10-build.log"
RUNTIME_LOG="$HOME/nw2-playable.log"
ARCHIVE_DIR="$HOME/nw2-v10-logs"
FPS=60
RESOLUTION="1600x900"
MAX_PARALLEL=3
PROFILE=0

while (($#)); do
    case "$1" in
        --ue-root) UE_ROOT="$2"; BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"; shift 2 ;;
        --fps) FPS="$2"; shift 2 ;;
        --resolution) RESOLUTION="$2"; shift 2 ;;
        --max-parallel) MAX_PARALLEL="$2"; shift 2 ;;
        --profile) PROFILE=1; shift ;;
        *) echo "Argumento desconhecido: $1" >&2; exit 2 ;;
    esac
done

[[ "$FPS" =~ ^[0-9]+$ ]] || { echo "FPS invalido" >&2; exit 2; }
[[ "$MAX_PARALLEL" =~ ^[0-9]+$ ]] || { echo "max-parallel invalido" >&2; exit 2; }
[[ "$RESOLUTION" =~ ^[0-9]+x[0-9]+$ ]] || { echo "Resolucao invalida: $RESOLUTION" >&2; exit 2; }

mkdir -p "$ARCHIVE_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
if [[ -s "$RUNTIME_LOG" ]]; then
    cp -a "$RUNTIME_LOG" "$ARCHIVE_DIR/nw2-playable-$STAMP.log"
fi

cd "$PROJECT_DIR"
git config core.fileMode false || true

cat <<INFO
============================================================
 NEW WORLD 2 - STABILITY V10 / BIGLINUX
============================================================
Projeto    : $PROJECT_DIR
Branch     : $(git branch --show-current 2>/dev/null || true)
Commit     : $(git rev-parse --short=12 HEAD 2>/dev/null || true)
UE         : $UE_ROOT
Resolucao  : $RESOLUTION
FPS        : $FPS
UBT jobs   : $MAX_PARALLEL
Profile    : $([[ $PROFILE -eq 1 ]] && echo SIM || echo NAO)
Build log  : $BUILD_LOG
Runtime    : $RUNTIME_LOG
============================================================
INFO

echo
echo "[1/6] Preflight V10..."
for token in '[ENV-V10]' '[VFX-V10]' '[STARTUP-V10]' '[STARTER-V10]' '[PLAYER-GEAR-V10]'; do
    if ! grep -RqsF "$token" Source/NewWorld2; then
        echo "ERRO: contrato V10 ausente: $token" >&2
        exit 3
    fi
done

if grep -Rqs '/Engine/BasicShapes/Capsule.Capsule' Source/NewWorld2/NWEnemy.cpp Source/NewWorld2/NWCivilian.cpp; then
    echo "ERRO: fallback Capsule removido do UE 5.8 ainda existe em Enemy/Civilian." >&2
    exit 3
fi

if grep -Rqs 'NiagaraExamples' Source/NewWorld2/NWPremiumVFXDirector.cpp; then
    echo "AVISO: texto NiagaraExamples ainda existe no diretor VFX; validando apenas que nao esta em filtro de carga."
fi

echo "OK: baseline V10 presente."

echo
echo "[2/6] Validando Unreal/Vulkan/GPU..."
[[ -x "$BUILD_SH" ]] || { echo "ERRO: Build.sh nao encontrado: $BUILD_SH" >&2; exit 4; }
[[ -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]] || { echo "ERRO: UnrealEditor nao encontrado." >&2; exit 4; }
[[ -f "$PROJECT_FILE" ]] || { echo "ERRO: NewWorld2.uproject nao encontrado." >&2; exit 4; }
command -v vulkaninfo >/dev/null 2>&1 && vulkaninfo --summary 2>/dev/null | grep -E 'deviceName|driverName|driverInfo|apiVersion' | head -12 || true
command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader || true

echo
echo "[3/6] Limpando apenas overrides/cache de sessao seguros..."
# Do not delete Content or the shared DDC. Only remove per-project transient
# config/shader-pipeline state that can keep old V7/V8/V9 CVars alive.
rm -rf "$PROJECT_DIR/Saved/Config/Linux" \
       "$PROJECT_DIR/Saved/Config/LinuxEditor" \
       "$PROJECT_DIR/Saved/ShaderDebugInfo" 2>/dev/null || true
mkdir -p "$PROJECT_DIR/Saved"

echo
echo "[4/6] Build incremental NewWorld2Editor..."
: > "$BUILD_LOG"
set +e
nice -n 5 "$BUILD_SH" NewWorld2Editor Linux Development "$PROJECT_FILE" \
    -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MAX_PARALLEL" \
    2>&1 | tee "$BUILD_LOG"
BUILD_RC=${PIPESTATUS[0]}
set -e

if (( BUILD_RC != 0 )); then
    echo
    echo "============================================================"
    echo " BUILD V10 FALHOU"
    echo "============================================================"
    grep -a -nE 'error:|fatal error:|Result: Failed|OtherCompilationError|Assertion failed|Unhandled Exception' "$BUILD_LOG" | tail -n 250 || true
    echo "Log completo: $BUILD_LOG"
    exit "$BUILD_RC"
fi

echo "OK: build C++ concluido."

echo
echo "[5/6] Abrindo playtest limpo..."
PLAY_ARGS=(--skip-build --fps "$FPS" --resolution "$RESOLUTION" --max-parallel "$MAX_PARALLEL")
if (( PROFILE )); then PLAY_ARGS+=(--profile); fi

set +e
bash "$PLAY_SH" "${PLAY_ARGS[@]}"
GAME_RC=$?
set -e

# Ctrl+C/window close is normal during an interactive development playtest.
if (( GAME_RC != 0 && GAME_RC != 130 && GAME_RC != 143 )); then
    echo "AVISO: Unreal encerrou com codigo $GAME_RC."
fi

echo
echo "[6/6] Diagnostico automatico do run..."
if [[ ! -s "$RUNTIME_LOG" ]]; then
    echo "ERRO: runtime log nao foi gerado: $RUNTIME_LOG" >&2
    exit 5
fi

echo
printf '%s\n' '--- V10 / AMBIENTE / EQUIPAMENTO ---'
grep -aE '\[ENV-V10\]|\[VFX-V10\]|\[STARTUP-V10\]|\[STARTER-V10\]|\[PLAYER-BASE-V10\]|\[PLAYER-GEAR-V10\]' "$RUNTIME_LOG" | tail -n 140 || true

echo
printf '%s\n' '--- TEMPOS DE STARTUP ---'
grep -aE 'Took .* seconds to LoadMap|Engine Initialization.*Total time|Load map complete' "$RUNTIME_LOG" | tail -n 30 || true

echo
printf '%s\n' '--- ERROS REAIS / REGRESSOES ---'
ERROR_PATTERN='LogBlueprint: Error|CDO Constructor|Fatal error|Assertion failed|Unhandled Exception|Segmentation fault|Divide by zero|Failed to find object|missing usage flag|Unable to load .*class .* does not exist'
grep -aE "$ERROR_PATTERN" "$RUNTIME_LOG" | tail -n 180 || true

echo
printf '%s\n' '--- PERFORMANCE / SHADERS ---'
PSO_LAST="$(grep -a 'PSO creation hitches so far' "$RUNTIME_LOG" | tail -1 || true)"
[[ -n "$PSO_LAST" ]] && echo "$PSO_LAST" || echo "Nenhum contador de PSO hitch registrado."
NIAGARA_COMPILES="$(grep -ac 'LogNiagara: Compiling System' "$RUNTIME_LOG" 2>/dev/null || true)"
echo "Compilacoes Niagara registradas: ${NIAGARA_COMPILES:-0}"

if grep -aq 'Icon_Sock' "$RUNTIME_LOG"; then
    echo "FALHA-V10: Icon_Sock apareceu no ambiente."
else
    echo "OK: nenhum Icon_Sock usado como vegetacao."
fi

if grep -aqE 'SM_EuropeanBeech_Forest_0[78]' "$RUNTIME_LOG"; then
    echo "AVISO-V10: arvore full-forest pesada ainda foi carregada."
else
    echo "OK: full-forest 07/08 nao foi selecionada pelo V10."
fi

if grep -aq "Failed to find object 'StaticMesh /Engine/BasicShapes/Capsule.Capsule'" "$RUNTIME_LOG"; then
    echo "FALHA-V10: referencia Capsule antiga ainda atingiu runtime."
else
    echo "OK: erro CDO do Capsule removido/redirectado."
fi

if grep -aqE 'GreystonePlayerCharacter.*\[Compiler\].*(ResetOrientationAndPosition|não existe mais)' "$RUNTIME_LOG"; then
    echo "AVISO-V10: Blueprint legado Greystone ainda pede reparo binario/local. Gameplay C++ continua independente dele."
else
    echo "OK: erro legado ResetOrientationAndPosition nao apareceu."
fi

if grep -aq 'nenhum mesh de arma encontrado para Cajado' "$RUNTIME_LOG"; then
    echo "FALHA-V10: cajado ainda ficou sem visual."
elif grep -aq 'arma visivel: Cajado' "$RUNTIME_LOG"; then
    echo "OK: cajado recebeu visual."
fi

echo
printf '%s\n' '--- RESULTADO ---'
echo "Build  : $BUILD_LOG"
echo "Runtime: $RUNTIME_LOG"
echo "Backup : $ARCHIVE_DIR"
echo "Game RC: $GAME_RC"
echo

echo "A baseline V10 prioriza estabilidade: sem reparo automatico de assets,"
echo "sem warm-up Niagara em massa, sem Nanite runtime para packs Fab pesados."

exit 0
