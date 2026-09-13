#!/usr/bin/env bash
set -Eeuo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
echo "[COMPAT] Premium V8 foi sucedido pela V9 hybrid-world. Encaminhando parametros..."
exec bash "$SCRIPT_DIR/premium-v9-test-biglinux.sh" "$@"
