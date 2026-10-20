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
USE_CCACHE="${DEVPIANO_USE_CCACHE:-}"
PRESET=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      USE_RELEASE=1
      ;;
    --ccache)
      USE_CCACHE=1
      ;;
    --no-ccache)
      USE_CCACHE=0
      ;;
    -h|--help)
      cat <<'EOF'
Usage: ./scripts/configure_wsl.sh [--release] [--ccache|--no-ccache] [preset-name]

Options:
  --release     Use the linux-clang-release preset
  --ccache      Enable the ccache compiler launcher
  --no-ccache   Disable the ccache compiler launcher
  -h, --help    Show this help
EOF
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

CCACHE_ARGS=()
if [[ "${USE_CCACHE}" == "1" ]]; then
  command -v ccache >/dev/null 2>&1 || fail 'ccache requested but not found in PATH'
  export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-time_macros}"
  export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${ROOT_DIR}}"
  CCACHE_ARGS=(
    -DCMAKE_C_COMPILER_LAUNCHER=ccache
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
  )
  log 'ccache compiler launcher enabled'
elif [[ "${USE_CCACHE}" == "0" ]]; then
  CCACHE_ARGS=(
    -DCMAKE_C_COMPILER_LAUNCHER=
    -DCMAKE_CXX_COMPILER_LAUNCHER=
  )
  log 'ccache compiler launcher disabled'
fi

log "project root: ${ROOT_DIR}"
log "configure preset: ${PRESET}"

cmake --preset "${PRESET}" -S "${ROOT_DIR}" "${CCACHE_ARGS[@]}"

if [[ -f "${ROOT_DIR}/${BUILD_DIR_NAME}/compile_commands.json" ]]; then
  success "compile_commands.json: ${ROOT_DIR}/${BUILD_DIR_NAME}/compile_commands.json"
fi

success 'configure complete'
