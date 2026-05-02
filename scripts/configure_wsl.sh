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
USE_RELEASE=0
PRESET=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      USE_RELEASE=1
      ;;
    -h|--help)
      echo "Usage: $0 [--release] [preset-name]"
      exit 0
      ;;
    *)
      PRESET="$1"
      ;;
  esac
  shift
done

if [[ -z "${PRESET}" ]]; then
  if [[ "${USE_RELEASE}" == "1" ]]; then
    PRESET="${CMAKE_PRESET:-linux-clang-release}"
  else
    PRESET="${CMAKE_PRESET:-linux-clang-debug}"
  fi
fi

if [[ "${USE_RELEASE}" == "1" ]]; then
  BUILD_DIR_NAME="build-wsl-clang-release"
else
  BUILD_DIR_NAME="build-wsl-clang"
fi

command -v cmake >/dev/null 2>&1 || fail 'cmake not found in PATH'

log "project root: ${ROOT_DIR}"
log "configure preset: ${PRESET}"

cmake --preset "${PRESET}" -S "${ROOT_DIR}"

if [[ -f "${ROOT_DIR}/${BUILD_DIR_NAME}/compile_commands.json" ]]; then
  success "compile_commands.json: ${ROOT_DIR}/${BUILD_DIR_NAME}/compile_commands.json"
fi

success 'configure complete'
