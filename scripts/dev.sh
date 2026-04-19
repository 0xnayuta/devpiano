#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="dev"
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  C_RESET='\033[0m'
  C_INFO='\033[1;34m'
  C_ERROR='\033[1;31m'
else
  C_RESET=''
  C_INFO=''
  C_ERROR=''
fi

log() {
  printf '%b[%s]%b %s\n' "${C_INFO}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

fail() {
  printf '%b[%s ERROR]%b %s\n' "${C_ERROR}" "${SCRIPT_NAME}" "${C_RESET}" "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage: ./scripts/dev.sh <command> [args...]

Commands:
  self-check [args...]      Run scripts/self_check.sh
  wsl-configure [args...]   Run scripts/configure_wsl.sh
  wsl-build [args...]       Run scripts/build_wsl.sh
  win-sync [args...]        Run scripts/sync_to_win.sh
  win-build [args...]       Run scripts/build_msvc_from_wsl.sh
  help                      Show this help

Examples:
  ./scripts/dev.sh self-check
  ./scripts/dev.sh wsl-build --configure-only
  ./scripts/dev.sh wsl-build --clean
  ./scripts/dev.sh win-sync
  ./scripts/dev.sh win-build --no-sync
  ./scripts/dev.sh win-build --reconfigure
EOF
}

command_name="${1:-help}"
if [[ $# -gt 0 ]]; then
  shift
fi

case "${command_name}" in
  self-check)
    log 'dispatch -> scripts/self_check.sh'
    exec "${ROOT_DIR}/scripts/self_check.sh" "$@"
    ;;
  wsl-configure)
    log 'dispatch -> scripts/configure_wsl.sh'
    exec "${ROOT_DIR}/scripts/configure_wsl.sh" "$@"
    ;;
  wsl-build)
    log 'dispatch -> scripts/build_wsl.sh'
    exec "${ROOT_DIR}/scripts/build_wsl.sh" "$@"
    ;;
  win-sync)
    log 'dispatch -> scripts/sync_to_win.sh'
    exec "${ROOT_DIR}/scripts/sync_to_win.sh" "$@"
    ;;
  win-build)
    log 'dispatch -> scripts/build_msvc_from_wsl.sh'
    exec "${ROOT_DIR}/scripts/build_msvc_from_wsl.sh" "$@"
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    fail "unknown command: ${command_name}"
    ;;
esac
