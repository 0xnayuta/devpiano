#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="build_msvc_from_wsl"

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
Usage: ./scripts/build_msvc_from_wsl.sh [options]

Options:
  --no-sync         Skip WSL -> Windows sync before build
  --sync-only       Only sync to Windows mirror, do not build
  --reconfigure     Remove Windows build cache before configure/build
  --clean-win-build Delete Windows build directory before configure/build
  -h, --help        Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
TOOLS_DIR="${ROOT_DIR}/tools"
SYNC_SCRIPT="${ROOT_DIR}/scripts/sync_to_win.sh"
PS_BUILD_SCRIPT="${TOOLS_DIR}/build-windows.ps1"
WIN_MIRROR_DIR_VALUE="${WIN_MIRROR_DIR:-G:\\source\\projects\\devpiano}"
BUILD_PRESET="${WINDOWS_CMAKE_BUILD_PRESET:-windows-msvc-debug}"
CONFIGURE_PRESET="${WINDOWS_CMAKE_CONFIGURE_PRESET:-windows-msvc-debug}"
SKIP_SYNC="${SKIP_SYNC_TO_WIN:-0}"
SYNC_ONLY=0
RECONFIGURE=0
CLEAN_WIN_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-sync)
      SKIP_SYNC=1
      ;;
    --sync-only)
      SYNC_ONLY=1
      ;;
    --reconfigure)
      RECONFIGURE=1
      ;;
    --clean-win-build)
      CLEAN_WIN_BUILD=1
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

if [[ "${SYNC_ONLY}" == "1" && ( "${RECONFIGURE}" == "1" || "${CLEAN_WIN_BUILD}" == "1" ) ]]; then
  fail '--sync-only cannot be combined with --reconfigure or --clean-win-build'
fi

resolve_windows_powershell() {
  local candidates=()

  if [[ -n "${WINDOWS_PWSH_EXE:-}" ]]; then
    candidates+=("${WINDOWS_PWSH_EXE}")
  fi

  if command -v pwsh.exe >/dev/null 2>&1; then
    command -v pwsh.exe
    return 0
  fi

  if command -v powershell.exe >/dev/null 2>&1; then
    command -v powershell.exe
    return 0
  fi

  candidates+=(
    "/mnt/c/Program Files/PowerShell/7/pwsh.exe"
    "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

WINDOWS_POWERSHELL="$(resolve_windows_powershell)" || fail 'No Windows PowerShell executable found. Set WINDOWS_PWSH_EXE, or ensure pwsh.exe / powershell.exe is callable from WSL.'
command -v wslpath >/dev/null 2>&1 || fail 'wslpath not found in PATH'
[[ -f "${PS_BUILD_SCRIPT}" ]] || fail "missing PowerShell script: ${PS_BUILD_SCRIPT}"

if [[ "${SKIP_SYNC}" != "1" ]]; then
  log 'syncing WSL tree to Windows mirror'
  "${SYNC_SCRIPT}"
else
  warn 'skipping sync step'
fi

if [[ "${SYNC_ONLY}" == "1" ]]; then
  success 'sync-only mode complete'
  exit 0
fi

WIN_MIRROR_DIR_WSL="$(wslpath -u "${WIN_MIRROR_DIR_VALUE}")"
WIN_BUILD_DIR_WSL="${WIN_MIRROR_DIR_WSL%/}/build-win-msvc"
WIN_PS_BUILD_SCRIPT="$(wslpath -w "${PS_BUILD_SCRIPT}")"

if [[ "${CLEAN_WIN_BUILD}" == "1" ]]; then
  log "cleaning Windows build directory: ${WIN_BUILD_DIR_WSL}"
  rm -rf -- "${WIN_BUILD_DIR_WSL}"
fi

if [[ "${RECONFIGURE}" == "1" ]]; then
  log "removing Windows CMake cache: ${WIN_BUILD_DIR_WSL}/CMakeCache.txt"
  rm -f -- "${WIN_BUILD_DIR_WSL}/CMakeCache.txt"
  rm -rf -- "${WIN_BUILD_DIR_WSL}/CMakeFiles"
fi

log "mirror dir: ${WIN_MIRROR_DIR_VALUE}"
log "configure preset: ${CONFIGURE_PRESET}"
log "build preset: ${BUILD_PRESET}"
log "windows shell: ${WINDOWS_POWERSHELL}"
if [[ "${RECONFIGURE}" == "1" ]]; then
  log 'mode: reconfigure'
fi
if [[ "${CLEAN_WIN_BUILD}" == "1" ]]; then
  log 'mode: clean-win-build'
fi

"${WINDOWS_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${WIN_PS_BUILD_SCRIPT}" \
  -MirrorDir "${WIN_MIRROR_DIR_VALUE}" \
  -ConfigurePreset "${CONFIGURE_PRESET}" \
  -BuildPreset "${BUILD_PRESET}"

success 'MSVC validation build complete'
