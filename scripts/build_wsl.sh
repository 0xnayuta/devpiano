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
  --configure-only  Configure and generate JuceHeader.h, do not build the app
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
cd "${ROOT_DIR}"
CONFIGURE_ONLY=0
RECONFIGURE=0
CLEAN=0
USE_RELEASE=0
NO_CONFIGURE=0
CCACHE_EXPLICIT=0
USE_CCACHE="${DEVPIANO_USE_CCACHE:-}"

case "${USE_CCACHE}" in
  ""|"0"|"1") ;;
  *) fail "DEVPIANO_USE_CCACHE must be 0 or 1 (got '${USE_CCACHE}')" ;;
esac

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
    --clean-win-build)
      fail '--clean-win-build is Windows-specific; use --clean'
      ;;
    --no-configure)
      NO_CONFIGURE=1
      ;;
    --ccache)
      USE_CCACHE=1
      CCACHE_EXPLICIT=1
      ;;
    --no-ccache)
      USE_CCACHE=0
      CCACHE_EXPLICIT=1
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

resolve_build_dir_from_presets() {
  local configure_preset="$1"
  local build_preset="$2"
  command -v python3 >/dev/null 2>&1 \
    || fail 'python3 not found; required to resolve CMake preset binaryDir'

  python3 - "${ROOT_DIR}" "${configure_preset}" "${build_preset}" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve()
configure_name = sys.argv[2]
build_name = sys.argv[3]
configure_presets = {}
build_presets = {}

for file_name in ("CMakePresets.json", "CMakeUserPresets.json"):
    preset_file = root / file_name
    if not preset_file.is_file():
        continue
    try:
        data = json.loads(preset_file.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise SystemExit(f"cannot read {preset_file}: {exc}")
    for preset in data.get("configurePresets") or []:
        if isinstance(preset, dict) and preset.get("name"):
            configure_presets[preset["name"]] = preset
    for preset in data.get("buildPresets") or []:
        if isinstance(preset, dict) and preset.get("name"):
            build_presets[preset["name"]] = preset

if configure_name not in configure_presets:
    raise SystemExit(f"configure preset not found: {configure_name}")
if build_name not in build_presets:
    raise SystemExit(f"build preset not found: {build_name}")

build_preset = build_presets[build_name]
configure_from_build = build_preset.get("configurePreset")
if configure_from_build and configure_from_build != configure_name:
    raise SystemExit(
        f"build preset '{build_name}' uses configure preset "
        f"'{configure_from_build}', not '{configure_name}'"
    )

configure_preset = configure_presets[configure_name]
binary_dir = configure_preset.get("binaryDir")
if not binary_dir:
    raise SystemExit(f"configure preset '{configure_name}' has no binaryDir")
binary_dir = binary_dir.replace("${sourceDir}", str(root))
build_dir = pathlib.Path(binary_dir)
if not build_dir.is_absolute():
    build_dir = root / build_dir
print(build_dir.resolve())
PY
}

if [[ "${USE_RELEASE}" == "1" ]]; then
  BUILD_PRESET="${CMAKE_BUILD_PRESET:-linux-clang-release}"
  CONFIGURE_PRESET="${CMAKE_CONFIGURE_PRESET:-linux-clang-release}"
else
  BUILD_PRESET="${CMAKE_BUILD_PRESET:-linux-clang-debug}"
  CONFIGURE_PRESET="${CMAKE_CONFIGURE_PRESET:-linux-clang-debug}"
fi

BUILD_DIR="$(resolve_build_dir_from_presets "${CONFIGURE_PRESET}" "${BUILD_PRESET}")" \
  || fail "failed to resolve build directory for preset '${CONFIGURE_PRESET}'"
case "${BUILD_DIR}" in
  "${ROOT_DIR}"/*) ;;
  *) fail "resolved build directory is outside the repository: ${BUILD_DIR}" ;;
esac

if [[ "${CONFIGURE_ONLY}" == "1" && "${CLEAN}" == "1" ]]; then
  warn 'configure-only + clean requested'
fi

if [[ "${NO_CONFIGURE}" == "1" ]]; then
  [[ "${CONFIGURE_ONLY}" != "1" ]] \
    || fail '--no-configure cannot be combined with --configure-only'
  [[ "${RECONFIGURE}" != "1" && "${CLEAN}" != "1" ]] \
    || fail '--no-configure cannot be combined with --reconfigure or --clean'
  [[ "${CCACHE_EXPLICIT}" != "1" ]] \
    || fail '--ccache/--no-ccache require a configure step'
  if [[ "${USE_CCACHE}" == "1" ]]; then
    warn 'DEVPIANO_USE_CCACHE is ignored with --no-configure'
  fi
  USE_CCACHE=""
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
