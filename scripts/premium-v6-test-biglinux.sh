#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
V7_SCRIPT="$SCRIPT_DIR/premium-v7-test-biglinux.sh"

if [[ ! -f "$V7_SCRIPT" ]]; then
    echo "ERRO: Premium V7 nao encontrado: $V7_SCRIPT" >&2
    exit 1
fi

cat <<'EOF'
============================================================
 NEW WORLD 2 - PREMIUM V6 COMPATIBILITY SHIM
============================================================
A V6 foi substituida pela V7 porque o projeto agora usa:
- inimigos por skeleton/AnimSequence sem fallback obrigatorio Greystone;
- armadura modular + fallback StaticMesh visivel;
- HUD com icones tematicos;
- PSO/Niagara warm-up atualizado para UE 5.8;
- alvo padrao de 60 FPS.

Todos os argumentos recebidos serao encaminhados para premium-v7-test-biglinux.sh.
============================================================
EOF

exec bash "$V7_SCRIPT" "$@"
