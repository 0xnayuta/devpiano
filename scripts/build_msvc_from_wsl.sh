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
  --release             Use windows-msvc-release preset (default: windows-msvc-debug)
  --no-sync             Skip WSL -> Windows sync before build
  --full                Force full sync including submodules before build
  --fast-times          Use FAT timestamps for sync (may miss fast edits)
  --sync-only           Only sync to Windows mirror, do not build
  --reconfigure         Remove Windows build cache before configure/build
  --no-configure        Skip the explicit configure step and reuse build.ninja
  --clean-win-build     Delete Windows build directory before configure/build
                        (refused by scripts/win_clean_guard.sh if the directory
                        contains non-build assets such as archived evidence)
  --allow-non-build-assets
                        Allow --clean-win-build to delete a build directory that
                        contains non-build assets (requires manual confirmation)
  -h, --help            Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
TOOLS_DIR="${ROOT_DIR}/tools"
SYNC_SCRIPT="${ROOT_DIR}/scripts/sync_to_win.sh"
GUARD_SCRIPT="${ROOT_DIR}/scripts/win_clean_guard.sh"
PS_BUILD_SCRIPT="${TOOLS_DIR}/build-windows.ps1"
WIN_MIRROR_DIR_VALUE="${WIN_MIRROR_DIR:-G:\\source\\projects\\devpiano}"
SKIP_SYNC="${SKIP_SYNC_TO_WIN:-0}"
FULL_SYNC=0
FAST_TIMES=0
SYNC_ONLY=0
RECONFIGURE=0
CLEAN_WIN_BUILD=0
ALLOW_NON_BUILD_ASSETS=0
USE_RELEASE=0
CLANG_TIDY=0
NO_CONFIGURE=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      USE_RELEASE=1
      ;;
    --no-sync)
      SKIP_SYNC=1
      ;;
    --full|--full-sync)
      FULL_SYNC=1
      ;;
    --fast-times)
      FAST_TIMES=1
      ;;
    --sync-only)
      SYNC_ONLY=1
      ;;
    --reconfigure)
      RECONFIGURE=1
      ;;
    --no-configure)
      NO_CONFIGURE=1
      ;;
    --clean)
      fail '--clean is not supported for Windows builds; use --clean-win-build (guarded deletion)'
      ;;
    --clean-win-build)
      CLEAN_WIN_BUILD=1
      ;;
    --allow-non-build-assets)
      ALLOW_NON_BUILD_ASSETS=1
      ;;
    --clang-tidy)
      CLANG_TIDY=1
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
  BUILD_PRESET="${WINDOWS_CMAKE_BUILD_PRESET:-windows-msvc-release}"
  CONFIGURE_PRESET="${WINDOWS_CMAKE_CONFIGURE_PRESET:-windows-msvc-release}"
else
  BUILD_PRESET="${WINDOWS_CMAKE_BUILD_PRESET:-windows-msvc-debug}"
  CONFIGURE_PRESET="${WINDOWS_CMAKE_CONFIGURE_PRESET:-windows-msvc-debug}"
fi

resolve_windows_build_dir_name() {
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
binary_dir = configure_presets[configure_name].get("binaryDir")
if not binary_dir:
    raise SystemExit(f"configure preset '{configure_name}' has no binaryDir")
binary_dir = binary_dir.replace("${sourceDir}", str(root))
print(pathlib.Path(binary_dir).name)
PY
}

WIN_BUILD_DIR_NAME="$(resolve_windows_build_dir_name "${CONFIGURE_PRESET}" "${BUILD_PRESET}")" \
  || fail "failed to resolve Windows build directory for preset '${CONFIGURE_PRESET}'"
case "${WIN_BUILD_DIR_NAME}" in
  build-win-msvc|build-win-msvc-release|build-win-msvc-*) ;;
  *) fail "unexpected Windows build directory name: ${WIN_BUILD_DIR_NAME}" ;;
esac

if [[ "${SYNC_ONLY}" == "1" && ( "${RECONFIGURE}" == "1" || "${CLEAN_WIN_BUILD}" == "1" ) ]]; then
  fail '--sync-only cannot be combined with --reconfigure or --clean-win-build'
fi

if [[ "${ALLOW_NON_BUILD_ASSETS}" == "1" && "${CLEAN_WIN_BUILD}" != "1" ]]; then
  fail '--allow-non-build-assets is only meaningful together with --clean-win-build'
fi

if [[ "${NO_CONFIGURE}" == "1" && ( "${SYNC_ONLY}" == "1" || "${RECONFIGURE}" == "1" || "${CLEAN_WIN_BUILD}" == "1" ) ]]; then
  fail '--no-configure cannot be combined with --sync-only, --reconfigure or --clean-win-build'
fi

if [[ "${SYNC_ONLY}" == "1" && "${SKIP_SYNC}" == "1" ]]; then
  fail '--sync-only cannot be combined with --no-sync'
fi

if [[ "${SKIP_SYNC}" == "1" && ( "${FULL_SYNC}" == "1" || "${FAST_TIMES}" == "1" ) ]]; then
  fail '--full and --fast-times require sync; remove --no-sync'
fi

normalize_windows_mirror_dir() {
  local value="$1"
  if [[ "${value}" == /* ]]; then
    wslpath -w -- "${value}"
  else
    printf '%s\n' "${value}"
  fi
}

resolve_windows_powershell() {
  if [[ -n "${WINDOWS_PWSH_EXE:-}" ]]; then
    local override="${WINDOWS_PWSH_EXE}"
    if [[ "${override}" == [A-Za-z]:* ]]; then
      override="$(wslpath -u "${override}" 2>/dev/null)" \
        || fail "WINDOWS_PWSH_EXE cannot be translated from Windows path: ${WINDOWS_PWSH_EXE}"
    elif [[ "${override}" != */* ]]; then
      override="$(command -v "${override}" 2>/dev/null)" \
        || fail "WINDOWS_PWSH_EXE not found in PATH: ${WINDOWS_PWSH_EXE}"
    fi
    [[ -x "${override}" ]] \
      || fail "WINDOWS_PWSH_EXE is not executable: ${override}"
    printf '%s\n' "${override}"
    return 0
  fi

  if command -v pwsh.exe >/dev/null 2>&1; then
    command -v pwsh.exe
    return 0
  fi

  if command -v powershell.exe >/dev/null 2>&1; then
    command -v powershell.exe
    return 0
  fi

  local candidate
  for candidate in \
    "/mnt/c/Program Files/PowerShell/7/pwsh.exe" \
    "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"; do
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

command -v wslpath >/dev/null 2>&1 || fail 'wslpath not found in PATH'
WIN_MIRROR_DIR_VALUE="$(normalize_windows_mirror_dir "${WIN_MIRROR_DIR_VALUE}")" \
  || fail "cannot normalize WIN_MIRROR_DIR: ${WIN_MIRROR_DIR_VALUE}"
case "${WIN_MIRROR_DIR_VALUE}" in
  [A-Za-z]:\\*|[A-Za-z]:/*) ;;
  *) fail "WIN_MIRROR_DIR must be a Windows path or a /mnt/... path: ${WIN_MIRROR_DIR_VALUE}" ;;
esac

WINDOWS_POWERSHELL="$(resolve_windows_powershell)" || fail 'No Windows PowerShell executable found. Set WINDOWS_PWSH_EXE, or ensure pwsh.exe / powershell.exe is callable from WSL.'
[[ -f "${PS_BUILD_SCRIPT}" ]] || fail "missing PowerShell script: ${PS_BUILD_SCRIPT}"

if [[ "${SKIP_SYNC}" != "1" ]]; then
  log 'syncing WSL tree to Windows mirror'
  sync_args=()
  if [[ "${FULL_SYNC}" == "1" ]]; then
    sync_args+=(--full)
  fi
  if [[ "${FAST_TIMES}" == "1" ]]; then
    sync_args+=(--fast-times)
  fi
  "${SYNC_SCRIPT}" "${sync_args[@]}"
else
  warn 'skipping sync step'
fi

if [[ "${SYNC_ONLY}" == "1" ]]; then
  success 'sync-only mode complete'
  exit 0
fi

WIN_MIRROR_DIR_WSL="$(wslpath -u "${WIN_MIRROR_DIR_VALUE}")"
WIN_MIRROR_DIR_WSL="$(realpath -m -- "${WIN_MIRROR_DIR_WSL}")" \
  || fail "cannot resolve Windows mirror path: ${WIN_MIRROR_DIR_VALUE}"
case "${WIN_MIRROR_DIR_WSL}" in
  /mnt/[a-zA-Z]/*) ;;
  *) fail "mirror path is not under a Windows-mounted drive: ${WIN_MIRROR_DIR_WSL}" ;;
esac
WIN_BUILD_DIR_WSL="$(realpath -m -- "${WIN_MIRROR_DIR_WSL}/${WIN_BUILD_DIR_NAME}")" \
  || fail "cannot resolve Windows build directory: ${WIN_BUILD_DIR_NAME}"
[[ "$(dirname -- "${WIN_BUILD_DIR_WSL}")" == "${WIN_MIRROR_DIR_WSL}" ]] \
  || fail "Windows build directory is not a direct child of the mirror: ${WIN_BUILD_DIR_WSL}"
[[ ! -L "${WIN_BUILD_DIR_WSL}" ]] \
  || fail "Windows build directory is a symlink: ${WIN_BUILD_DIR_WSL}"
if [[ ! -f "${WIN_MIRROR_DIR_WSL}/CMakeLists.txt" || ! -d "${WIN_MIRROR_DIR_WSL}/source" ]]; then
  fail "Windows mirror does not look like a devpiano checkout: ${WIN_MIRROR_DIR_WSL}"
fi
WIN_PS_BUILD_SCRIPT="$(wslpath -w "${PS_BUILD_SCRIPT}")"

if [[ "${CLEAN_WIN_BUILD}" == "1" ]]; then
  [[ -f "${GUARD_SCRIPT}" ]] || fail "missing clean-build guard script: ${GUARD_SCRIPT}"
  guard_args=( "${WIN_MIRROR_DIR_WSL}" "${WIN_BUILD_DIR_NAME}" )
  if [[ "${ALLOW_NON_BUILD_ASSETS}" == "1" ]]; then
    guard_args+=( --allow-non-build-assets )
  fi
  if ! bash "${GUARD_SCRIPT}" "${guard_args[@]}"; then
    fail 'refusing to delete the Windows build directory (guard diagnostics above)'
  fi
  log "cleaning Windows build directory: ${WIN_BUILD_DIR_WSL}"
  rm -rf -- "${WIN_BUILD_DIR_WSL}"
fi

if [[ "${RECONFIGURE}" == "1" ]]; then
  [[ -f "${GUARD_SCRIPT}" ]] || fail "missing clean-build guard script: ${GUARD_SCRIPT}"
  if ! bash "${GUARD_SCRIPT}" "${WIN_MIRROR_DIR_WSL}" "${WIN_BUILD_DIR_NAME}"; then
    fail 'refusing to touch the Windows CMake cache (guard diagnostics above)'
  fi
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
if [[ "${CLANG_TIDY}" == "1" ]]; then
  log 'mode: clang-tidy'
fi
if [[ "${NO_CONFIGURE}" == "1" ]]; then
  log 'mode: no-configure'
fi

PS_ARGS=(
  -NoProfile
  -ExecutionPolicy Bypass
  -File "${WIN_PS_BUILD_SCRIPT}"
  -MirrorDir "${WIN_MIRROR_DIR_VALUE}"
  -ConfigurePreset "${CONFIGURE_PRESET}"
  -BuildPreset "${BUILD_PRESET}"
)
if [[ "${CLANG_TIDY}" == "1" ]]; then
  PS_ARGS+=(-ClangTidy)
fi
if [[ "${NO_CONFIGURE}" == "1" ]]; then
  PS_ARGS+=(-SkipConfigure)
fi

"${WINDOWS_POWERSHELL}" "${PS_ARGS[@]}"

success 'MSVC validation build complete'
