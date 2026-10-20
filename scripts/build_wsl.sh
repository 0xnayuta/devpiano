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
  --no-configure    Skip the explicit configure step and reuse build.ninja
  --ccache          Enable the ccache compiler launcher
  --no-ccache       Disable the ccache compiler launcher
  -h, --help        Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
CONFIGURE_ONLY=0
RECONFIGURE=0
CLEAN=0
USE_RELEASE=0
NO_CONFIGURE=0
USE_CCACHE="${DEVPIANO_USE_CCACHE:-}"

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
    --no-configure)
      NO_CONFIGURE=1
      ;;
    --ccache)
      USE_CCACHE=1
      ;;
    --no-ccache)
      USE_CCACHE=0
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

if [[ "${NO_CONFIGURE}" == "1" ]]; then
  [[ "${CONFIGURE_ONLY}" != "1" ]] \
    || fail '--no-configure cannot be combined with --configure-only'
  [[ "${RECONFIGURE}" != "1" && "${CLEAN}" != "1" ]] \
    || fail '--no-configure cannot be combined with --reconfigure or --clean'
  [[ -z "${USE_CCACHE}" ]] \
    || fail '--ccache/--no-ccache require a configure step'
  [[ -f "${BUILD_DIR}/build.ninja" ]] \
    || fail "missing ${BUILD_DIR}/build.ninja; run configure first"
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
if [[ "${NO_CONFIGURE}" == "0" ]]; then
  log "configure preset: ${CONFIGURE_PRESET}"
  cmake --preset "${CONFIGURE_PRESET}" -S "${ROOT_DIR}" "${CCACHE_ARGS[@]}"
else
  log 'skipping configure; reusing the existing build tree'
fi

# Generate JuceHeader.h through the build system so clangd/LSP can resolve it
# without a full build. Building the file targets also avoids guessing which
# Defs.txt belongs to the application or test target.
generate_juce_header() {
  local header_targets=(
    "devpiano_artefacts/JuceLibraryCode/JuceHeader.h"
  )
  if grep -q 'devpiano_tests_artefacts/JuceLibraryCode/JuceHeader.h' \
      "${BUILD_DIR}/build.ninja" 2>/dev/null; then
    header_targets+=("devpiano_tests_artefacts/JuceLibraryCode/JuceHeader.h")
  fi

  log 'generating JuceHeader.h for clangd/LSP'
  cmake --build "${BUILD_DIR}" --target "${header_targets[@]}"
}

if [[ "${NO_CONFIGURE}" == "0" ]]; then
  generate_juce_header
fi

if [[ "${CONFIGURE_ONLY}" == "1" ]]; then
  success 'configure-only mode complete'
  exit 0
fi

log "build preset: ${BUILD_PRESET}"
cmake --build --preset "${BUILD_PRESET}"

success 'build complete'
