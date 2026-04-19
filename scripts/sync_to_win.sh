#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="sync_to_win"

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
TOOLS_DIR="${ROOT_DIR}/tools"
PS_SCRIPT="${TOOLS_DIR}/sync-to-win.ps1"
WIN_MIRROR_DIR_VALUE="${WIN_MIRROR_DIR:-G:\\source\\projects\\devpiano}"

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
[[ -f "${PS_SCRIPT}" ]] || fail "missing PowerShell script: ${PS_SCRIPT}"

WIN_SOURCE_DIR="$(wslpath -w "${ROOT_DIR}")"
WIN_PS_SCRIPT="$(wslpath -w "${PS_SCRIPT}")"

log "source (WSL): ${ROOT_DIR}"
log "source (Windows view): ${WIN_SOURCE_DIR}"
log "mirror dir: ${WIN_MIRROR_DIR_VALUE}"
log "windows shell: ${WINDOWS_POWERSHELL}"

"${WINDOWS_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${WIN_PS_SCRIPT}" \
  -SourceDir "${WIN_SOURCE_DIR}" \
  -MirrorDir "${WIN_MIRROR_DIR_VALUE}"

success 'sync complete'
