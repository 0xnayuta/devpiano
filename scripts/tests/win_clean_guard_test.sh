#!/usr/bin/env bash
# 测试 scripts/win_clean_guard.sh 的 fail-closed 行为。
#
# 全部用例都在可抛弃的隔离目录中运行（默认 /mnt/g/tmp 下的一次性目录），
# 不触碰真实的 Windows 镜像构建目录，也不执行任何删除。
#
# 运行：bash scripts/tests/win_clean_guard_test.sh
# 退出码：0 = 全部通过；1 = 存在失败；0（打印 SKIP）= 本机没有 Windows 盘挂载。

set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GUARD="${SCRIPT_DIR}/../win_clean_guard.sh"

if [[ ! -f "${GUARD}" ]]; then
  printf 'FAIL: guard script not found: %s\n' "${GUARD}" >&2
  exit 1
fi

# guard 只接受 /mnt/<drive>/... 下的目标；选一个可写的 Windows 盘挂载做隔离测试。
TEST_MNT_ROOT=""
for candidate in /mnt/g /mnt/d /mnt/c /mnt/e /mnt/f; do
  if [[ -d "${candidate}" && -w "${candidate}" ]]; then
    TEST_MNT_ROOT="${candidate}"
    break
  fi
done

if [[ -z "${TEST_MNT_ROOT}" ]]; then
  printf 'SKIP: no writable /mnt/<drive> mount found (guard is WSL-only; not testable here)\n'
  exit 0
fi

TEST_BASE="${TEST_MNT_ROOT}/tmp"
if [[ ! -d "${TEST_BASE}" || ! -w "${TEST_BASE}" ]]; then
  TEST_BASE="${TEST_MNT_ROOT}"
fi

WORK="$(mktemp -d -p "${TEST_BASE}" devpiano-guard-test.XXXXXX)"
cleanup() { rm -rf -- "${WORK}"; }
trap cleanup EXIT

pass=0
fail=0

run_guard() {
  local expect="$1" desc="$2"
  shift 2
  local out rc
  out="$(bash "${GUARD}" "$@" 2>&1)" && rc=0 || rc=$?
  if [[ "${rc}" == "${expect}" ]]; then
    printf '  PASS  exit=%s  %s\n' "${rc}" "${desc}"
    pass=$((pass + 1))
  else
    printf '  FAIL  exit=%s (expected %s)  %s\n' "${rc}" "${expect}" "${desc}"
    printf '%s\n' "${out}" | sed 's/^/        | /'
    fail=$((fail + 1))
  fi
}

printf '[win_clean_guard_test] workdir: %s\n' "${WORK}"

# --- 用例 1：正常构建目录（仅构建产物）必须放行 -------------------------------
MIRROR_SAFE="${WORK}/mirror-safe"
mkdir -p "${MIRROR_SAFE}/build-win-msvc/CMakeFiles" \
         "${MIRROR_SAFE}/build-win-msvc/devpiano_artefacts" \
         "${MIRROR_SAFE}/build-win-msvc/submodules"
: > "${MIRROR_SAFE}/build-win-msvc/CMakeCache.txt"
: > "${MIRROR_SAFE}/build-win-msvc/build.ninja"
: > "${MIRROR_SAFE}/build-win-msvc/compile_commands.json"
: > "${MIRROR_SAFE}/build-win-msvc/.ninja_deps"
: > "${MIRROR_SAFE}/build-win-msvc/.ninja_log"
: > "${MIRROR_SAFE}/build-win-msvc/cmake_install.cmake"
: > "${MIRROR_SAFE}/build-win-msvc/CTestTestfile.cmake"
: > "${MIRROR_SAFE}/build-win-msvc/install_manifest.txt"
: > "${MIRROR_SAFE}/build-win-msvc/devpiano.exe"
: > "${MIRROR_SAFE}/build-win-msvc/devpiano_binary_data.lib"
run_guard 0 'pure build directory is allowed' "${MIRROR_SAFE}" build-win-msvc

# --- 用例 2：release 构建目录名同样在允许集合内 --------------------------------
mkdir -p "${MIRROR_SAFE}/build-win-msvc-release/CMakeFiles"
run_guard 0 'build-win-msvc-release is allowed' "${MIRROR_SAFE}" build-win-msvc-release

# --- 用例 3：非预期目录名必须拒绝 ---------------------------------------------
mkdir -p "${MIRROR_SAFE}/build-anything"
run_guard 1 'unexpected build dir name is refused' "${MIRROR_SAFE}" build-anything
run_guard 2 'empty build dir name produces usage error' "${MIRROR_SAFE}" ''
run_guard 1 'absolute path as build dir name is refused' "${MIRROR_SAFE}" '/etc'

# --- 用例 4：危险镜像根必须拒绝 -----------------------------------------------
run_guard 1 'mirror "/" is refused' / build-win-msvc
run_guard 1 'drive root /mnt/<d> is refused' "${TEST_MNT_ROOT}" build-win-msvc
run_guard 1 'WSL source tree is refused' /root/repos/devpiano build-win-msvc
run_guard 1 'WSL /tmp is refused' /tmp build-win-msvc

# --- 用例 5：路径逃逸/规范化后仍必须落在镜像根之下 -------------------------------
run_guard 0 'normalized escape back into the same mirror is allowed' \
  "${MIRROR_SAFE}/../mirror-safe" build-win-msvc

# 目标经由符号链接指向镜像根之外的目录时必须拒绝
OUTSIDE="${WORK}/outside-target"
mkdir -p "${OUTSIDE}"
ln -s "${OUTSIDE}" "${MIRROR_SAFE}/build-win-msvc-linked"
run_guard 1 'symlinked build dir name is refused' "${MIRROR_SAFE}" build-win-msvc-linked

mkdir -p "${WORK}/mirror-link"
ln -s "${OUTSIDE}" "${WORK}/mirror-link/build-win-msvc"
run_guard 1 'symlinked target dir is refused' "${WORK}/mirror-link" build-win-msvc

# --- 用例 6：目录内存在非构建资产（历史证据）必须拒绝 ---------------------------
MIRROR_EVIDENCE="${WORK}/mirror-evidence"
mkdir -p "${MIRROR_EVIDENCE}/build-win-msvc/CMakeFiles" \
         "${MIRROR_EVIDENCE}/build-win-msvc/phase39-task1-evidence" \
         "${MIRROR_EVIDENCE}/build-win-msvc/phase38-evidence"
: > "${MIRROR_EVIDENCE}/build-win-msvc/CMakeCache.txt"
: > "${MIRROR_EVIDENCE}/build-win-msvc/evidence-manifest.json"
run_guard 1 'evidence dirs block deletion by default' "${MIRROR_EVIDENCE}" build-win-msvc
run_guard 0 'evidence dirs pass only with --allow-non-build-assets' \
  "${MIRROR_EVIDENCE}" build-win-msvc --allow-non-build-assets

# 未知顶层条目同样按非构建资产处理
MIRROR_UNKNOWN="${WORK}/mirror-unknown"
mkdir -p "${MIRROR_UNKNOWN}/build-win-msvc/mystery-payload"
run_guard 1 'unknown top-level entry blocks deletion' "${MIRROR_UNKNOWN}" build-win-msvc

# 每份证据单独存在，不能依赖另一个未知目录触发拒绝。
for evidence_file in evidence-manifest.json analysis.txt render.log measurements.db \
                     experiment.cache capture.tmp callback.trace .ninja_evidence.log \
                     juce_reference.json CMakeUserPresets.json; do
  mirror="${WORK}/mirror-file-${evidence_file}"
  mkdir -p "${mirror}/build-win-msvc"
  printf 'retained evidence\n' > "${mirror}/build-win-msvc/${evidence_file}"
  run_guard 1 "standalone ${evidence_file} blocks deletion" "${mirror}" build-win-msvc
  if [[ "${evidence_file}" == "evidence-manifest.json" ]]; then
    run_guard 0 'standalone evidence requires explicit override' \
      "${mirror}" build-win-msvc --allow-non-build-assets
  fi
done

# 已知构建文件名不能掩盖目录或符号链接。
MIRROR_WRONG_KIND="${WORK}/mirror-wrong-kind"
mkdir -p "${MIRROR_WRONG_KIND}/build-win-msvc/CMakeCache.txt"
run_guard 1 'directory named CMakeCache.txt blocks deletion' "${MIRROR_WRONG_KIND}" build-win-msvc
MIRROR_ENTRY_LINK="${WORK}/mirror-entry-link"
mkdir -p "${MIRROR_ENTRY_LINK}/build-win-msvc"
ln -s "${OUTSIDE}" "${MIRROR_ENTRY_LINK}/build-win-msvc/devpiano_artefacts"
run_guard 1 'symlinked build entry blocks deletion' "${MIRROR_ENTRY_LINK}" build-win-msvc

# --- 用例 7：目标不存在是 no-op（允许，无删除对象） -----------------------------
run_guard 0 'missing build dir is a no-op' "${WORK}/mirror-absent" build-win-msvc

# --- 用例 8：参数错误返回 2 ---------------------------------------------------
run_guard 2 'missing arguments produce usage error' "${MIRROR_SAFE}"
run_guard 2 'unknown option produces usage error' "${MIRROR_SAFE}" build-win-msvc --nope

printf '[win_clean_guard_test] %d passed, %d failed\n' "${pass}" "${fail}"
[[ "${fail}" == "0" ]]
