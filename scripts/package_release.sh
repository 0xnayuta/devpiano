#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="package_release"

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
Usage: ./scripts/dev.sh package [options]
   or: ./scripts/package_release.sh [options]

Package the Windows x64 Release build into zip and SHA256 checksums.

Options:
  -v, --version <X.Y.Z>   Release version (e.g. 1.0.0 or v1.0.0, defaults to CMakeLists.txt)
  --win-mirror-dir <dir>  Override Windows mirror directory (default: WIN_MIRROR_DIR or G:\source\projects\devpiano)
  --dist-dir <dir>        Custom output distribution directory (default: <WIN_MIRROR_DIR>/dist/v<VERSION>)
  --local-dist            Save dist package to local repo dist/v<VERSION> instead of Windows mirror
  -h, --help              Show this help

Examples:
  ./scripts/dev.sh package
  ./scripts/dev.sh package --version 1.0.0
  ./scripts/dev.sh package -v v1.0.0
  ./scripts/dev.sh package --local-dist
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

VERSION_INPUT=""
WIN_MIRROR_DIR_VALUE="${WIN_MIRROR_DIR:-G:\\source\\projects\\devpiano}"
DIST_DIR_INPUT=""
USE_LOCAL_DIST=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -v|--version)
      [[ $# -ge 2 ]] || fail '--version requires an argument'
      VERSION_INPUT="$2"
      shift 2
      ;;
    --win-mirror-dir)
      [[ $# -ge 2 ]] || fail '--win-mirror-dir requires an argument'
      WIN_MIRROR_DIR_VALUE="$2"
      shift 2
      ;;
    --dist-dir)
      [[ $# -ge 2 ]] || fail '--dist-dir requires an argument'
      DIST_DIR_INPUT="$2"
      shift 2
      ;;
    --local-dist)
      USE_LOCAL_DIST=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "Unknown argument: $1 (run with --help for usage)"
      ;;
  esac
done

# 1. 检查基础打包依赖
command -v wslpath >/dev/null 2>&1 || fail 'wslpath command not found'
command -v zip >/dev/null 2>&1 || fail 'zip command not found (run: sudo apt-get install -y zip)'
command -v sha256sum >/dev/null 2>&1 || fail 'sha256sum command not found'

# 2. 解析版本号
CMAKELIST_FILE="${ROOT_DIR}/CMakeLists.txt"
[[ -f "${CMAKELIST_FILE}" ]] || fail "CMakeLists.txt not found at ${CMAKELIST_FILE}"

CMAKE_VERSION="$(grep -E '^\s*project\s*\(\s*devpiano\s+VERSION\s+[0-9.]+' "${CMAKELIST_FILE}" | sed -E 's/.*VERSION\s+([0-9.]+).*/\1/' || true)"

if [[ -z "${VERSION_INPUT}" ]]; then
  if [[ -z "${CMAKE_VERSION}" ]]; then
    fail 'Failed to extract project version from CMakeLists.txt. Please specify --version explicitly.'
  fi
  VERSION="${CMAKE_VERSION}"
  log "Using version from CMakeLists.txt: ${VERSION}"
else
  # 去掉可能带有的 'v' 或 'V' 前缀
  VERSION="${VERSION_INPUT#[vV]}"
  if [[ "${VERSION}" != "${CMAKE_VERSION}" ]]; then
    warn "Specified version (${VERSION}) differs from CMakeLists.txt version (${CMAKE_VERSION})"
  fi
fi

TAG="v${VERSION}"
log "Packaging release for tag: ${TAG} (version: ${VERSION})"

# 3. 校验 CHANGELOG.md 中是否存在该版本
CHANGELOG_FILE="${ROOT_DIR}/CHANGELOG.md"
if [[ -f "${CHANGELOG_FILE}" ]]; then
  if ! grep -q -E "^##\s*\[${VERSION}\]" "${CHANGELOG_FILE}"; then
    warn "CHANGELOG.md does not appear to have an entry for ## [${VERSION}]"
  else
    log "CHANGELOG.md verified with entry for [${VERSION}]"
  fi
else
  fail "CHANGELOG.md not found at ${CHANGELOG_FILE}"
fi

# 4. 定位 Windows Release 产物
WIN_MIRROR_DIR_WSL="$(wslpath -u "${WIN_MIRROR_DIR_VALUE}")"
RELEASE_ARTIFACTS_DIR="${WIN_MIRROR_DIR_WSL}/build-win-msvc-release/devpiano_artefacts/Release"
RELEASE_EXE="${RELEASE_ARTIFACTS_DIR}/DevPiano.exe"

if [[ ! -f "${RELEASE_EXE}" ]]; then
  fail "Release executable not found at:
  ${RELEASE_EXE}
Please ensure Windows Release build has succeeded:
  ./scripts/dev.sh win-build --release"
fi

log "Found Windows x64 Release executable: ${RELEASE_EXE}"

# 5. 确定目标输出目录
if [[ -n "${DIST_DIR_INPUT}" ]]; then
  DIST_DIR="${DIST_DIR_INPUT}"
elif [[ ${USE_LOCAL_DIST} -eq 1 ]]; then
  DIST_DIR="${ROOT_DIR}/dist/${TAG}"
else
  DIST_DIR="${WIN_MIRROR_DIR_WSL}/dist/${TAG}"
fi

log "Preparing distribution directory: ${DIST_DIR}"
mkdir -p "${DIST_DIR}"

# 6. 复制产物
cp -f "${RELEASE_EXE}" "${DIST_DIR}/"
cp -f "${CHANGELOG_FILE}" "${DIST_DIR}/"

# 7. 打包 ZIP 与生成 SHA256 校验和
ZIP_FILENAME="DevPiano-${TAG}-win-x64.zip"
SHA_FILENAME="DevPiano-${TAG}-win-x64.sha256"

cd "${DIST_DIR}"

log "Compressing ${ZIP_FILENAME}..."
rm -f "${ZIP_FILENAME}" "${SHA_FILENAME}"
zip -9 "${ZIP_FILENAME}" DevPiano.exe CHANGELOG.md >/dev/null

log "Calculating SHA256 checksum..."
sha256sum "${ZIP_FILENAME}" > "${SHA_FILENAME}"

# 8. 打印打包结果
success "=========================================================="
success " Release Package Created Successfully for ${TAG}"
success "=========================================================="
printf '%bLocation:%b %s\n' "${C_INFO}" "${C_RESET}" "${DIST_DIR}"
printf '%bContents:%b\n' "${C_INFO}" "${C_RESET}"
ls -lh DevPiano.exe CHANGELOG.md "${ZIP_FILENAME}" "${SHA_FILENAME}"

printf '\n%bSHA256 Checksum:%b\n' "${C_INFO}" "${C_RESET}"
cat "${SHA_FILENAME}"

printf '\n%bNext steps:%b\n' "${C_INFO}" "${C_RESET}"
printf '  1. Perform manual smoke testing on DevPiano.exe\n'
printf '  2. Create git tag and push:\n'
printf '     git tag -a %s -m "Release %s"\n' "${TAG}" "${TAG}"
printf '     git push origin main && git push origin %s\n' "${TAG}"
printf '  3. Create GitHub Release:\n'
printf '     gh release create "%s" \\\n' "${TAG}"
printf '       --title "DevPiano %s" \\\n' "${TAG}"
printf '       --notes-file "%s/CHANGELOG.md" \\\n' "${ROOT_DIR}"
printf '       "%s/%s" \\\n' "${DIST_DIR}" "${ZIP_FILENAME}"
printf '       "%s/%s"\n\n' "${DIST_DIR}" "${SHA_FILENAME}"
