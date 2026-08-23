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
  format [--check]          Format source/ with clang-format-21
  tidy [--all] [files...]   Incremental clang-tidy on changed files (or given
                            files; --all = full scan via CMake target)
  test [args...]            Configure, build and run devpiano_tests
  time-trace [args...]      Build with Clang -ftime-trace and analyze hotspots
  package [args...]         Run scripts/package_release.sh (Windows x64 zip + sha256)
  help                      Show this help

Examples:
  ./scripts/dev.sh self-check
  ./scripts/dev.sh wsl-build --configure-only
  ./scripts/dev.sh format
  ./scripts/dev.sh format --check
  ./scripts/dev.sh tidy                # Uncommitted changed files
  ./scripts/dev.sh tidy --all          # Full scan across all files (~19 mins)
  ./scripts/dev.sh tidy source/UI/CustomKeyboard.cpp
  ./scripts/dev.sh test
  ./scripts/dev.sh test --verbose
  ./scripts/dev.sh win-sync            # Fast sync business code (< 1s)
  ./scripts/dev.sh win-sync --full     # Full sync including submodules (~9s)
  ./scripts/dev.sh win-build           # Fast sync and validation build
  ./scripts/dev.sh win-build --full    # Full sync and validation build
  ./scripts/dev.sh package             # Package Release distribution (zip + sha256)
  ./scripts/dev.sh package --version 1.0.0
  ./scripts/dev.sh time-trace          # Profile build hotspots (slowest files/headers/templates)
  ./scripts/dev.sh time-trace --clean  # Clean build with full profiling
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
  format)
    CLANG_FORMAT_CMD="${CLANG_FORMAT_EXE:-}"
    if [[ -z "${CLANG_FORMAT_CMD}" ]]; then
      if command -v clang-format-21 >/dev/null 2>&1; then
        CLANG_FORMAT_CMD="clang-format-21"
      elif command -v clang-format >/dev/null 2>&1; then
        CLANG_FORMAT_CMD="clang-format"
      else
        fail 'clang-format not found (install clang-format-21 or clang-format)'
      fi
    fi
    log "Running ${CLANG_FORMAT_CMD} on source/"
    if [[ "${1:-}" == "--check" ]]; then
        find "${ROOT_DIR}/source" -name '*.cpp' -o -name '*.h' \
            | xargs "${CLANG_FORMAT_CMD}" -style=file --dry-run --Werror
        log 'clang-format check passed'
    else
        find "${ROOT_DIR}/source" -name '*.cpp' -o -name '*.h' \
            | xargs "${CLANG_FORMAT_CMD}" -i -style=file
        log 'clang-format applied'
    fi
    ;;
  tidy)
    # Incremental clang-tidy（ADR-007：只检查，不用 --fix）。
    # 无参数 = 未提交改动文件；带参数 = 指定文件；--all = 全量（CMake target）。
    if ! command -v clang-tidy-21 >/dev/null 2>&1; then
        fail 'clang-tidy-21 not found (apt install clang-tidy-21)'
    fi
    if [[ ! -f "${ROOT_DIR}/build-wsl-clang/compile_commands.json" ]]; then
        fail 'compile_commands.json missing - run ./scripts/dev.sh wsl-build --configure-only first'
    fi

    if [[ "${1:-}" == "--all" ]]; then
        shift
        log 'Running clang-tidy-21 full scan (all source/ files)'
        cmake --build "${ROOT_DIR}/build-wsl-clang" --target clang-tidy 2>&1
        exit $?
    fi

    files=()
    if [[ $# -gt 0 ]]; then
        files=("$@")
    else
        # 未提交改动（staged + unstaged + untracked）中的 source/ 下 .cpp/.h
        mapfile -t files < <({
            git -C "${ROOT_DIR}" diff --name-only HEAD
            git -C "${ROOT_DIR}" ls-files --others --exclude-standard
        } | sort -u | grep -E '^source/.*\.(cpp|h)$' || true)
    fi

    if [[ ${#files[@]} -eq 0 ]]; then
        log 'No changed source files to check'
        exit 0
    fi

    log "Running clang-tidy-21 on ${#files[@]} file(s): ${files[*]}"
    # --warnings-as-errors='*'：任何诊断（含 NOLINT 之外的全部 checker）即非零退出，
    # 与 Phase B 的"全量 0 诊断"门禁语义一致。
    clang-tidy-21 -p "${ROOT_DIR}/build-wsl-clang" --quiet \
        --warnings-as-errors='*' "${files[@]}" 2>&1
    ;;
  test)
    log 'Ensuring BUILD_TESTS=ON configured'
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-wsl-clang" --preset linux-clang-debug \
          -DBUILD_TESTS=ON 2>&1 | tail -3
    log 'Building devpiano_tests target'
    cmake --build "${ROOT_DIR}/build-wsl-clang" --target devpiano_tests 2>&1
    log 'Running devpiano_tests via ctest'
    ctest --test-dir "${ROOT_DIR}/build-wsl-clang" -R devpiano_tests \
          --output-on-failure "$@" 2>&1
    ;;
  time-trace)
    target="devpiano_tests"
    clean_first=false
    merge_trace="combined_time_trace.json"

    while [[ $# -gt 0 ]]; do
      case "$1" in
        --clean)
          clean_first=true
          shift
          ;;
        --target)
          target="$2"
          shift 2
          ;;
        --no-merge)
          merge_trace=""
          shift
          ;;
        *)
          target="$1"
          shift
          ;;
      esac
    done

    if [[ "${clean_first}" == true ]]; then
      log "Cleaning build directory for full profile: ${ROOT_DIR}/build-wsl-clang"
      rm -rf "${ROOT_DIR}/build-wsl-clang/CMakeFiles"
    fi

    log "Configuring with ENABLE_TIME_TRACE=ON (target: ${target})"
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-wsl-clang" --preset linux-clang-debug \
          -DBUILD_TESTS=ON -DENABLE_TIME_TRACE=ON 2>&1 | tail -5

    log "Building target ${target} with -ftime-trace profiling..."
    cmake --build "${ROOT_DIR}/build-wsl-clang" --target "${target}" 2>&1

    log "Analyzing build traces..."
    if [[ -n "${merge_trace}" ]]; then
      python3 "${ROOT_DIR}/scripts/analyze_build_time.py" \
        --build-dir "${ROOT_DIR}/build-wsl-clang" \
        --merge-trace "${merge_trace}"
    else
      python3 "${ROOT_DIR}/scripts/analyze_build_time.py" \
        --build-dir "${ROOT_DIR}/build-wsl-clang"
    fi
    ;;
  package)
    log 'dispatch -> scripts/package_release.sh'
    exec "${ROOT_DIR}/scripts/package_release.sh" "$@"
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    fail "unknown command: ${command_name}"
    ;;
esac
