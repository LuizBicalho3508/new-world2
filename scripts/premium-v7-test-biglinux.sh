#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

echo "============================================================"
echo " NEW WORLD 2 - COMPATIBILIDADE PREMIUM V7 -> V8"
echo "============================================================"
echo "A V8 substitui a V7 para corrigir corpo modular, armas flutuantes,"
echo "startup Niagara e CVars PSO da UE 5.8.2."
echo

exec bash "$SCRIPT_DIR/premium-v8-test-biglinux.sh" "$@"
