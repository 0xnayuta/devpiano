#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="configure_wsl"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  C_RESET='\033[0m'
  C_INFO='\033[1;34m'
  C_ERROR='\033[1;31m'
  C_SUCCESS='\033[1;32m'
else
  C_RESET=''
  C_INFO=''
  C_ERROR=''
  C_SUCCESS=''
fi

log() {
  printf '%b[%s]%b %s\n' "${C_INFO}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

success() {
  printf '%b[%s]%b %s\n' "${C_SUCCESS}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

fail() {
  printf '%b[%s ERROR]%b %s\n' "${C_ERROR}" "${SCRIPT_NAME}" "${C_RESET}" "$*" >&2
  exit 1
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
PRESET="${1:-${CMAKE_PRESET:-linux-clang-debug}}"

command -v cmake >/dev/null 2>&1 || fail 'cmake not found in PATH'

log "project root: ${ROOT_DIR}"
log "configure preset: ${PRESET}"

cmake --preset "${PRESET}" -S "${ROOT_DIR}"

if [[ -f "${ROOT_DIR}/build-wsl-clang/compile_commands.json" ]]; then
  success "compile_commands.json: ${ROOT_DIR}/build-wsl-clang/compile_commands.json"
fi

success 'configure complete'
