#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="self_check"
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
WIN_MIRROR_DIR_VALUE="${WIN_MIRROR_DIR:-G:\\source\\projects\\devpiano}"
FAILURES=0

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

ok() {
  printf '%b[%s OK]%b %s\n' "${C_SUCCESS}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

warn() {
  printf '%b[%s WARN]%b %s\n' "${C_WARN}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

err() {
  printf '%b[%s ERROR]%b %s\n' "${C_ERROR}" "${SCRIPT_NAME}" "${C_RESET}" "$*" >&2
  FAILURES=$((FAILURES + 1))
}

check_cmd() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    ok "$cmd -> $(command -v "$cmd")"
  else
    err "$cmd not found in PATH"
  fi
}

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

log "project root: ${ROOT_DIR}"

check_cmd cmake
check_cmd ninja
check_cmd clang
check_cmd clangd
check_cmd wslpath

if [[ -f "${ROOT_DIR}/CMakePresets.json" ]]; then
  ok "CMakePresets.json present"
else
  err "CMakePresets.json missing"
fi

if [[ -f "${ROOT_DIR}/.clangd" ]]; then
  ok ".clangd present"
else
  err ".clangd missing"
fi

if [[ -f "${ROOT_DIR}/build-wsl-clang/compile_commands.json" ]]; then
  ok "compile_commands.json present: ${ROOT_DIR}/build-wsl-clang/compile_commands.json"
else
  warn "compile_commands.json missing; run ./scripts/dev.sh wsl-build --configure-only"
fi

if command -v cmake >/dev/null 2>&1; then
  if cmake --list-presets >/dev/null 2>&1; then
    ok "cmake presets readable"
  else
    err "cmake --list-presets failed"
  fi
fi

if [[ -n "${WIN_MIRROR_DIR:-}" ]]; then
  ok "WIN_MIRROR_DIR set: ${WIN_MIRROR_DIR}"
else
  warn "WIN_MIRROR_DIR not set; using default example: ${WIN_MIRROR_DIR_VALUE}"
fi

if [[ -n "${WINDOWS_PWSH_EXE:-}" ]]; then
  ok "WINDOWS_PWSH_EXE set: ${WINDOWS_PWSH_EXE}"
else
  warn 'WINDOWS_PWSH_EXE not set; auto-detection will be used'
fi

WINDOWS_POWERSHELL="$(resolve_windows_powershell || true)"
if [[ -n "${WINDOWS_POWERSHELL}" ]]; then
  ok "Windows PowerShell detected: ${WINDOWS_POWERSHELL}"
else
  err 'No Windows PowerShell executable detected from WSL'
fi

if command -v wslpath >/dev/null 2>&1; then
  WIN_MIRROR_DIR_WSL="$(wslpath -u "${WIN_MIRROR_DIR_VALUE}" 2>/dev/null || true)"
  if [[ -n "${WIN_MIRROR_DIR_WSL}" ]]; then
    if [[ -d "${WIN_MIRROR_DIR_WSL}" ]]; then
      ok "Windows mirror dir visible from WSL: ${WIN_MIRROR_DIR_WSL}"
    else
      err "Windows mirror dir not found from WSL: ${WIN_MIRROR_DIR_WSL}"
    fi
  else
    err "failed to translate WIN_MIRROR_DIR to WSL path: ${WIN_MIRROR_DIR_VALUE}"
  fi
fi

if [[ -f "/mnt/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" ]]; then
  ok 'vswhere.exe found at default path'
else
  warn 'vswhere.exe not found at default path; build-windows.ps1 may still work if Visual Studio Installer is elsewhere'
fi

if [[ ${FAILURES} -gt 0 ]]; then
  err "self-check finished with ${FAILURES} failure(s)"
  exit 1
fi

ok 'self-check passed'
