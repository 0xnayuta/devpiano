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
  self-check                 Validate local toolchain and workspace
  wsl-configure [opts]       Raw CMake configure (no JuceHeader.h generation)
  wsl-build [opts]           Configure/build WSL; prefer --configure-only for clangd
  win-sync [--check] [--full] [--fast-times]
                             Sync the workspace to the Windows mirror
  win-build [opts]           Sync and build with MSVC (Debug by default)
  format [--check]           Format source/ with clang-format-21
  tidy [files...]            Incremental clang-tidy on changed or given files
  tidy --all                 Full clang-tidy scan (iteration-boundary gate)
  tidy --clear-cache         Clear the local tidy result cache
  test [ctest args...]       Configure, build and run devpiano_tests (Linux/WSL)
  time-trace [options] [target]
                             Profile Clang build hotspots
  package [opts]             Package Windows x64 zip or Linux x64 tar.gz + sha256
  desktop-install [opts]     Install/remove Linux desktop entry and icons
  help                       Show this help

Examples:
  # Environment
  ./scripts/dev.sh self-check
  ./scripts/dev.sh wsl-configure                # Raw configure; no JuceHeader.h
  ./scripts/dev.sh wsl-configure linux-clang-release
  ./scripts/dev.sh wsl-build --configure-only   # Preferred for clangd
  ./scripts/dev.sh wsl-build --configure-only --ccache
  ./scripts/dev.sh wsl-build --configure-only --no-ccache
  ./scripts/dev.sh wsl-build --configure-only --reconfigure
  ./scripts/dev.sh wsl-build --release
  ./scripts/dev.sh wsl-build --no-configure    # Requires existing build.ninja; skips JuceHeader.h

  # Sync and build
  ./scripts/dev.sh win-sync --check
  ./scripts/dev.sh win-sync --fast-times        # FAT timestamps; may miss fast edits
  ./scripts/dev.sh win-sync --full
  ./scripts/dev.sh win-build
  ./scripts/dev.sh win-build --no-sync
  ./scripts/dev.sh win-build --no-configure     # Requires existing Windows build tree
  ./scripts/dev.sh win-build --reconfigure
  ./scripts/dev.sh win-build --clean-win-build
  ./scripts/dev.sh win-build --release
  ./scripts/dev.sh win-build --sync-only

  # Quality
  ./scripts/dev.sh format
  ./scripts/dev.sh format --check
  ./scripts/dev.sh tidy
  ./scripts/dev.sh tidy source/UI/CustomKeyboard.cpp
  ./scripts/dev.sh tidy --all
  ./scripts/dev.sh tidy --clear-cache

  # Tests: args are forwarded to ctest; TestRunner flags need the binary
  ./scripts/dev.sh test
  ./scripts/dev.sh test --verbose

  # Profiling and packaging
  ./scripts/dev.sh time-trace
  ./scripts/dev.sh time-trace --clean
  ./scripts/dev.sh time-trace --target devpiano
  ./scripts/dev.sh package
  ./scripts/dev.sh package --version 1.0.0
  ./scripts/dev.sh package --linux
  ./scripts/dev.sh package --local-dist

  # Linux desktop integration
  ./scripts/dev.sh desktop-install
  ./scripts/dev.sh desktop-install --uninstall

Run './scripts/dev.sh <command> --help' for command-specific options.
EOF
}

command_name="${1:-help}"
if [[ $# -gt 0 ]]; then
  shift
fi

case "${command_name}" in
  self-check)
    if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
        cat <<'EOF'
Usage: ./scripts/dev.sh self-check

Checks required local tools, CMake presets and the Windows mirror path.
EOF
        exit 0
    fi
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
    if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
        cat <<'EOF'
Usage: ./scripts/dev.sh format [--check]

Formats or checks all .cpp and .h files under source/ with clang-format.
  --check     Dry-run with --dry-run --Werror; do not modify files
  -h, --help  Show this help

Environment:
  FORMAT_JOBS Number of parallel clang-format workers (default: 8)
EOF
        exit 0
    fi
    if [[ $# -gt 1 ]]; then
        fail 'format accepts only --check'
    fi
    if [[ $# -eq 1 && "${1}" != "--check" ]]; then
        fail "unknown argument: ${1}"
    fi
    FORMAT_JOBS="${FORMAT_JOBS:-8}"
    [[ "${FORMAT_JOBS}" =~ ^[1-9][0-9]*$ ]] \
        || fail 'FORMAT_JOBS must be a positive integer'
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
        find "${ROOT_DIR}/source" -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
            | xargs -0 -r -P "${FORMAT_JOBS}" -n 24 \
                "${CLANG_FORMAT_CMD}" -style=file --dry-run --Werror
        log 'clang-format check passed'
    else
        find "${ROOT_DIR}/source" -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
            | xargs -0 -r -P "${FORMAT_JOBS}" -n 24 \
                "${CLANG_FORMAT_CMD}" -i -style=file
        log 'clang-format applied'
    fi
    ;;
  tidy)
    # Incremental / Full clang-tidy with multi-core parallelism and local result caching (ADR-007).
    if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
        cat <<'EOF'
Usage: ./scripts/dev.sh tidy [options] [files...]

Runs clang-tidy using build-wsl-clang/compile_commands.json.
Without files, checks changed and untracked files under source/.
Files are resolved relative to the repository root.

Options:
  --all          Full parallel scan of source/ (iteration-boundary gate)
  --clear-cache  Clear the local tidy result cache
  -h, --help     Show this help

Environment:
  TIDY_JOBS      Parallel clang-tidy processes (default: nproc)
EOF
        exit 0
    fi

    case "${1:-}" in
      --all)
        [[ $# -eq 1 ]] || fail '--all does not accept file arguments'
        ;;
      --clear-cache)
        [[ $# -eq 1 ]] || fail '--clear-cache does not accept arguments'
        ;;
      *)
        for arg in "$@"; do
          [[ "${arg}" != -* ]] || fail "unknown option: ${arg}"
        done
        ;;
    esac

    # Run from the repository root so relative paths, .clang-tidy and the
    # default cache directory are resolved consistently.
    cd "${ROOT_DIR}"

    WRAPPER="${ROOT_DIR}/tools/tidy-cache.py"
    [[ -x "${WRAPPER}" ]] || fail "tidy cache wrapper not executable: ${WRAPPER}"
    command -v python3 >/dev/null 2>&1 \
        || fail 'python3 not found (required by tools/tidy-cache.py)'

    if [[ "${1:-}" == "--clear-cache" ]]; then
        "${WRAPPER}" --clear-cache
        exit 0
    fi

    if ! command -v clang-tidy-21 >/dev/null 2>&1; then
        fail 'clang-tidy-21 not found (apt install clang-tidy-21)'
    fi
    if [[ ! -f "${ROOT_DIR}/build-wsl-clang/compile_commands.json" ]]; then
        fail 'compile_commands.json missing - run ./scripts/dev.sh wsl-build --configure-only first'
    fi

    NPROCS="${TIDY_JOBS:-$(nproc 2>/dev/null || echo 4)}"
    [[ "${NPROCS}" =~ ^[1-9][0-9]*$ ]] \
        || fail 'TIDY_JOBS must be a positive integer'

    if [[ "${1:-}" == "--all" ]]; then
        log "Running clang-tidy full scan in parallel (-j ${NPROCS}, cached)"
        if command -v run-clang-tidy-21 >/dev/null 2>&1; then
            run-clang-tidy-21 -clang-tidy-binary "${WRAPPER}" \
                              -p "${ROOT_DIR}/build-wsl-clang" \
                              -warnings-as-errors='*' \
                              -j "${NPROCS}" -quiet 'source/.*' 2>&1
        elif command -v run-clang-tidy >/dev/null 2>&1; then
            run-clang-tidy -clang-tidy-binary "${WRAPPER}" \
                           -p "${ROOT_DIR}/build-wsl-clang" \
                           -warnings-as-errors='*' \
                           -j "${NPROCS}" -quiet 'source/.*' 2>&1
        else
            cmake --build "${ROOT_DIR}/build-wsl-clang" --target clang-tidy 2>&1
        fi
        exit 0
    fi

    files=()
    if [[ $# -gt 0 ]]; then
        for arg in "$@"; do
            if [[ -f "${arg}" ]]; then
                files+=("${arg}")
            else
                fail "file not found: ${arg}"
            fi
        done
    else
        # 未提交改动（staged + unstaged + untracked）中的 source/ 下 .cpp/.h（排除已删除文件）
        mapfile -t files < <({
            git -C "${ROOT_DIR}" diff --diff-filter=d --name-only HEAD
            git -C "${ROOT_DIR}" ls-files --others --exclude-standard
        } | sort -u | grep -E '^source/.*\.(cpp|h)$' || true)
    fi

    if printf '%s\n' "${files[@]}" | grep -qE '\.(h|hpp|inc)$'; then
        log 'Header change detected; checking all source files (cache-aware)'
        mapfile -t files < <(find "source" -type f -name '*.cpp' -print | sort)
    fi

    if [[ ${#files[@]} -eq 0 ]]; then
        log 'No changed source files to check'
        exit 0
    fi

    log "Running clang-tidy on ${#files[@]} file(s) in ${NPROCS} worker(s) (cached)"
    printf '%s\0' "${files[@]}" \
        | xargs -0 -r -P "${NPROCS}" -n 1 \
            "${WRAPPER}" -p "${ROOT_DIR}/build-wsl-clang" --quiet \
            --warnings-as-errors='*' 2>&1
    ;;
  test)
    if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
        cat <<'EOF'
Usage: ./scripts/dev.sh test [ctest args...]

Configures build-wsl-clang, builds devpiano_tests, then runs ctest.
All arguments are forwarded to ctest (for example --verbose).

TestRunner options (--include-juce, --include-files, --category,
--name) are not ctest options. Pass them to the test binary directly:
  build-wsl-clang/devpiano_tests_artefacts/Debug/devpiano_tests --help

Environment:
  DEVPIANO_USE_CCACHE=1  Use ccache for the test build
EOF
        exit 0
    fi
    log 'Ensuring BUILD_TESTS=ON configured'
    CCACHE_ARGS=()
    if [[ "${DEVPIANO_USE_CCACHE:-0}" == "1" ]]; then
        command -v ccache >/dev/null 2>&1 \
            || fail 'DEVPIANO_USE_CCACHE=1 but ccache is not in PATH'
        export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-time_macros}"
        export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${ROOT_DIR}}"
        CCACHE_ARGS=(
            -DCMAKE_C_COMPILER_LAUNCHER=ccache
            -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
        )
        log 'ccache compiler launcher enabled'
    fi
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-wsl-clang" --preset linux-clang-debug \
          -DBUILD_TESTS=ON "${CCACHE_ARGS[@]}"
    log 'Building devpiano_tests target'
    cmake --build "${ROOT_DIR}/build-wsl-clang" --target devpiano_tests 2>&1
    log 'Running devpiano_tests via ctest'
    ctest --test-dir "${ROOT_DIR}/build-wsl-clang" \
          --output-on-failure "$@" 2>&1
    ;;
  time-trace)
    if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
        cat <<'EOF'
Usage: ./scripts/dev.sh time-trace [options] [target]

Builds with Clang -ftime-trace and analyzes the slowest files,
headers and template instantiations.

Options:
  --clean          Remove build CMakeFiles before profiling
  --target <name>  CMake target to profile (default: devpiano_tests)
  --no-merge       Skip the merged Chrome Tracing / Perfetto timeline
  -h, --help       Show this help
EOF
        exit 0
    fi
    target="devpiano_tests"
    target_set=false
    clean_first=false
    merge_trace="combined_time_trace.json"

    while [[ $# -gt 0 ]]; do
      case "$1" in
        --clean)
          clean_first=true
          shift
          ;;
        --target)
          [[ $# -ge 2 && -n "$2" && "$2" != -* ]] \
              || fail '--target requires a target name'
          [[ "${target_set}" == false ]] || fail 'target specified more than once'
          target="$2"
          target_set=true
          shift 2
          ;;
        --no-merge)
          merge_trace=""
          shift
          ;;
        *)
          [[ "$1" != -* ]] || fail "unknown option: $1"
          [[ "${target_set}" == false ]] || fail 'target specified more than once'
          target="$1"
          target_set=true
          shift
          ;;
      esac
    done

    command -v python3 >/dev/null 2>&1 \
        || fail 'python3 not found (required by scripts/analyze_build_time.py)'
    [[ -f "${ROOT_DIR}/scripts/analyze_build_time.py" ]] \
        || fail 'scripts/analyze_build_time.py not found'

    if [[ "${clean_first}" == true ]]; then
      log "Removing ${ROOT_DIR}/build-wsl-clang/CMakeFiles for a full profile"
      rm -rf "${ROOT_DIR}/build-wsl-clang/CMakeFiles"
    fi

    log "Configuring with ENABLE_TIME_TRACE=ON (target: ${target})"
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-wsl-clang" --preset linux-clang-debug \
          -DBUILD_TESTS=ON -DENABLE_TIME_TRACE=ON

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
  desktop-install)
    log 'dispatch -> scripts/install_linux_desktop.sh'
    exec "${ROOT_DIR}/scripts/install_linux_desktop.sh" "$@"
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
