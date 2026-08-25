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
With --linux, package the Linux x64 Release build into tar.gz and SHA256.

Options:
  -v, --version <X.Y.Z>   Release version (e.g. 1.0.0 or v1.0.0, defaults to CMakeLists.txt)
  --linux                 Package Linux x64 Release build (tar.gz + sha256, local dist)
  --win-mirror-dir <dir>  Override Windows mirror directory (default: WIN_MIRROR_DIR or G:\source\projects\devpiano)
  --dist-dir <dir>        Custom output distribution directory (default: <WIN_MIRROR_DIR>/dist/v<VERSION>)
  --local-dist            Save dist package to local repo dist/v<VERSION> instead of Windows mirror
  -h, --help              Show this help

Examples:
  ./scripts/dev.sh package
  ./scripts/dev.sh package --version 1.0.0
  ./scripts/dev.sh package -v v1.0.0
  ./scripts/dev.sh package --local-dist
  ./scripts/dev.sh package --linux
  ./scripts/dev.sh package --linux --version 1.0.0
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

VERSION_INPUT=""
WIN_MIRROR_DIR_VALUE="${WIN_MIRROR_DIR:-G:\\source\\projects\\devpiano}"
DIST_DIR_INPUT=""
USE_LOCAL_DIST=0
PACKAGE_LINUX=0

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
    --linux)
      PACKAGE_LINUX=1
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

# 1. 检查基础打包依赖（按平台分支：Windows 需 wslpath+zip，Linux 需 tar）
if [[ ${PACKAGE_LINUX} -eq 1 ]]; then
  command -v tar >/dev/null 2>&1 || fail 'tar command not found'
else
  command -v wslpath >/dev/null 2>&1 || fail 'wslpath command not found'
  command -v zip >/dev/null 2>&1 || fail 'zip command not found (run: sudo apt-get install -y zip)'
fi
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

# 4. 定位 Release 产物（Windows: 镜像树 MSVC 产物；Linux: WSL 本地 Clang 产物）
if [[ ${PACKAGE_LINUX} -eq 1 ]]; then
  RELEASE_ARTIFACTS_DIR="${ROOT_DIR}/build-wsl-clang-release/devpiano_artefacts/Release"
  RELEASE_BIN="${RELEASE_ARTIFACTS_DIR}/DevPiano"

  if [[ ! -f "${RELEASE_BIN}" ]]; then
    fail "Release executable not found at:
  ${RELEASE_BIN}
Please ensure Linux Release build has succeeded:
  ./scripts/dev.sh wsl-build --release"
  fi

  log "Found Linux x64 Release executable: ${RELEASE_BIN}"
else
  WIN_MIRROR_DIR_WSL="$(wslpath -u "${WIN_MIRROR_DIR_VALUE}")"
  RELEASE_ARTIFACTS_DIR="${WIN_MIRROR_DIR_WSL}/build-win-msvc-release/devpiano_artefacts/Release"
  RELEASE_BIN="${RELEASE_ARTIFACTS_DIR}/DevPiano.exe"

  if [[ ! -f "${RELEASE_BIN}" ]]; then
    fail "Release executable not found at:
  ${RELEASE_BIN}
Please ensure Windows Release build has succeeded:
  ./scripts/dev.sh win-build --release"
  fi

  log "Found Windows x64 Release executable: ${RELEASE_BIN}"
fi

# 5. 确定目标输出目录
if [[ -n "${DIST_DIR_INPUT}" ]]; then
  DIST_DIR="${DIST_DIR_INPUT}"
elif [[ ${PACKAGE_LINUX} -eq 1 || ${USE_LOCAL_DIST} -eq 1 ]]; then
  DIST_DIR="${ROOT_DIR}/dist/${TAG}"
else
  DIST_DIR="${WIN_MIRROR_DIR_WSL}/dist/${TAG}"
fi

log "Preparing distribution directory: ${DIST_DIR}"
mkdir -p "${DIST_DIR}"

# 6. 复制产物
cp -f "${RELEASE_BIN}" "${DIST_DIR}/"
cp -f "${CHANGELOG_FILE}" "${DIST_DIR}/"

# 7. 打包与生成 SHA256 校验和（Linux 额外执行 glibc/libstdc++ 门槛检查）
if [[ ${PACKAGE_LINUX} -eq 1 ]]; then
  # 门槛检查与 release.yml 的 release-linux-x64 同规则，防本地备用产物兼容面缩水
  log "Running glibc/libstdc++ version floor check..."
  "${ROOT_DIR}/scripts/check_linux_glibc_floor.sh" "${RELEASE_BIN}" || \
    fail "glibc/libstdc++ floor check failed — binary is not distributable"

  ARCHIVE_FILENAME="DevPiano-${TAG}-linux-x64.tar.gz"
  SHA_FILENAME="DevPiano-${TAG}-linux-x64.sha256"

  cd "${DIST_DIR}"

  log "Compressing ${ARCHIVE_FILENAME}..."
  rm -f "${ARCHIVE_FILENAME}" "${SHA_FILENAME}"
  tar -czf "${ARCHIVE_FILENAME}" DevPiano CHANGELOG.md

  log "Calculating SHA256 checksum..."
  sha256sum "${ARCHIVE_FILENAME}" > "${SHA_FILENAME}"
else
  ZIP_FILENAME="DevPiano-${TAG}-win-x64.zip"
  SHA_FILENAME="DevPiano-${TAG}-win-x64.sha256"

  cd "${DIST_DIR}"

  log "Compressing ${ZIP_FILENAME}..."
  rm -f "${ZIP_FILENAME}" "${SHA_FILENAME}"
  zip -9 "${ZIP_FILENAME}" DevPiano.exe CHANGELOG.md >/dev/null

  log "Calculating SHA256 checksum..."
  sha256sum "${ZIP_FILENAME}" > "${SHA_FILENAME}"
fi

# 8. 打印打包结果
success "=========================================================="
success " Release Package Created Successfully for ${TAG}"
success "=========================================================="
printf '%bLocation:%b %s\n' "${C_INFO}" "${C_RESET}" "${DIST_DIR}"
printf '%bContents:%b\n' "${C_INFO}" "${C_RESET}"
if [[ ${PACKAGE_LINUX} -eq 1 ]]; then
  ls -lh DevPiano CHANGELOG.md "${ARCHIVE_FILENAME}" "${SHA_FILENAME}"
else
  ls -lh DevPiano.exe CHANGELOG.md "${ZIP_FILENAME}" "${SHA_FILENAME}"
fi

printf '\n%bSHA256 Checksum:%b\n' "${C_INFO}" "${C_RESET}"
cat "${SHA_FILENAME}"

printf '\n%bNext steps:%b\n' "${C_INFO}" "${C_RESET}"
if [[ ${PACKAGE_LINUX} -eq 1 ]]; then
  printf '  1. Perform manual smoke testing on the DevPiano binary (see docs/guides/release-workflow.md)\n'
  printf '  2. Create git tag and push:\n'
  printf '     git tag -a %s -m "Release %s"\n' "${TAG}" "${TAG}"
  printf '     git push origin main && git push origin %s\n' "${TAG}"
  printf '  3. Create GitHub Release (release.yml uploads both win-x64 zip and linux-x64 tar.gz):\n'
  printf '     gh release create "%s" \\\n' "${TAG}"
  printf '       --title "DevPiano %s" \\\n' "${TAG}"
  printf '       --notes-file "%s/CHANGELOG.md" \\\n' "${ROOT_DIR}"
  printf '       "%s/%s" \\\n' "${DIST_DIR}" "${ARCHIVE_FILENAME}"
  printf '       "%s/%s"\n\n' "${DIST_DIR}" "${SHA_FILENAME}"
else
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
fi
