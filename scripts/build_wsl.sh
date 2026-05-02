#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="build_wsl"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  C_RESET='\033[0m'
  C_INFO='\033[1;34m'
  C_WARN='\033[1;33m'
  C_ERROR='\033[1;31m'
  C_SUCCESS='\033[1;32m'
else
  C_RESET=''
  C_INFO=''
  C_WARN=''
  C_ERROR=''
  C_SUCCESS=''
fi

log() {
  printf '%b[%s]%b %s\n' "${C_INFO}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

warn() {
  printf '%b[%s WARN]%b %s\n' "${C_WARN}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

success() {
  printf '%b[%s]%b %s\n' "${C_SUCCESS}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

fail() {
  printf '%b[%s ERROR]%b %s\n' "${C_ERROR}" "${SCRIPT_NAME}" "${C_RESET}" "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage: ./scripts/build_wsl.sh [options]

Options:
  --release         Use linux-clang-release preset (default: linux-clang-debug)
  --configure-only  Only run CMake configure, do not build
  --reconfigure     Remove WSL CMake cache before configure/build
  --clean           Delete the entire WSL build directory before configure/build
  -h, --help        Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
CONFIGURE_ONLY=0
RECONFIGURE=0
CLEAN=0
USE_RELEASE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      USE_RELEASE=1
      ;;
    --configure-only)
      CONFIGURE_ONLY=1
      ;;
    --reconfigure)
      RECONFIGURE=1
      ;;
    --clean)
      CLEAN=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown option: $1"
      ;;
  esac
  shift
done

if [[ "${USE_RELEASE}" == "1" ]]; then
  BUILD_PRESET="${CMAKE_BUILD_PRESET:-linux-clang-release}"
  CONFIGURE_PRESET="${CMAKE_CONFIGURE_PRESET:-linux-clang-release}"
  BUILD_DIR="${ROOT_DIR}/build-wsl-clang-release"
else
  BUILD_PRESET="${CMAKE_BUILD_PRESET:-linux-clang-debug}"
  CONFIGURE_PRESET="${CMAKE_CONFIGURE_PRESET:-linux-clang-debug}"
  BUILD_DIR="${ROOT_DIR}/build-wsl-clang"
fi

if [[ "${CONFIGURE_ONLY}" == "1" && "${CLEAN}" == "1" ]]; then
  warn 'configure-only + clean requested'
fi

command -v cmake >/dev/null 2>&1 || fail 'cmake not found in PATH'

if [[ "${CLEAN}" == "1" ]]; then
  log "cleaning WSL build directory: ${BUILD_DIR}"
  rm -rf -- "${BUILD_DIR}"
fi

if [[ "${RECONFIGURE}" == "1" ]]; then
  log "removing WSL CMake cache: ${BUILD_DIR}/CMakeCache.txt"
  rm -f -- "${BUILD_DIR}/CMakeCache.txt"
  rm -rf -- "${BUILD_DIR}/CMakeFiles"
fi

log "project root: ${ROOT_DIR}"
log "configure preset: ${CONFIGURE_PRESET}"
cmake --preset "${CONFIGURE_PRESET}" -S "${ROOT_DIR}"

# Generate JuceHeader.h so clangd/LSP can resolve it without a full build.
# JuceHeader.h is normally generated during the build step by juceaide.
# Running it here makes --configure-only sufficient for clangd to work.
generate_juce_header() {
  local juceaide
  juceaide="$(find "${BUILD_DIR}" -name juceaide -type f -executable 2>/dev/null | head -1)"
  if [[ -z "${juceaide}" ]]; then
    warn "juceaide not found in ${BUILD_DIR}; skipping JuceHeader.h generation"
    return 0
  fi

  local defs_file
  defs_file="$(find "${BUILD_DIR}" -path "*/JuceLibraryCode/*/Defs.txt" 2>/dev/null | head -1)"
  if [[ -z "${defs_file}" ]]; then
    warn "Defs.txt not found in ${BUILD_DIR}; skipping JuceHeader.h generation"
    return 0
  fi

  local header_dir
  header_dir="$(dirname "${defs_file}")"
  # Defs.txt is in a config subdirectory (e.g. Debug/); header goes one level up
  header_dir="$(dirname "${header_dir}")"
  local header_file="${header_dir}/JuceHeader.h"

  "${juceaide}" header "${defs_file}" "${header_file}"
  log "generated: ${header_file}"
}

generate_juce_header

if [[ "${CONFIGURE_ONLY}" == "1" ]]; then
  success 'configure-only mode complete'
  exit 0
fi

log "build preset: ${BUILD_PRESET}"
cmake --build --preset "${BUILD_PRESET}"

success 'build complete'
