#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# Linux Release 产物 glibc / libstdc++ 版本门槛检查（Phase 25-C）
#
# 背景（2026-08 查证）：Linux 产物的运行兼容面由构建环境的 glibc /
# libstdc++ 版本决定——glibc 只向后兼容，滚动发行版（Arch/CachyOS，
# glibc 2.44）构建的产物仅 Arch 系可运行。正式 Linux 产物在
# ubuntu-24.04 runner（glibc 2.39 / GCC 13 libstdc++）构建，支持矩阵为
# glibc >= 2.39 的常见桌面发行版。
#
# 用法：
#   ./scripts/check_linux_glibc_floor.sh [path-to-DevPiano]
# 默认检查 build-wsl-clang-release 产物；非零退出 = 门槛不达标。
# =============================================================================

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BINARY="${1:-${ROOT_DIR}/build-wsl-clang-release/devpiano_artefacts/Release/DevPiano}"

# 构建底线 = ubuntu-24.04 runner 的 glibc / libstdc++（GCC 13）
GLIBC_FLOOR="2.39"
GLIBCXX_FLOOR="3.4.32"

# 目标支持矩阵（发行版 → glibc 版本，2026-08 查证）
declare -A DISTRO_GLIBC=(
    ["Ubuntu 24.04 LTS"]="2.39"
    ["Debian 13"]="2.41"
    ["Fedora 41"]="2.40"
    ["Arch / CachyOS / Manjaro"]="2.44"
)

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

# 点分版本比较：ver_ge <a> <b> → a >= b
ver_ge() {
    local -a a=() b=()
    IFS='.' read -r -a a <<< "$1"
    IFS='.' read -r -a b <<< "$2"
    for i in 0 1 2; do
        local ai="${a[$i]:-0}" bi="${b[$i]:-0}"
        if ((ai > bi)); then
            return 0
        fi
        if ((ai < bi)); then
            return 1
        fi
    done
    return 0
}

[[ -f "${BINARY}" ]] || fail "binary not found: ${BINARY}"
command -v readelf >/dev/null 2>&1 || fail "readelf not found (install binutils)"

MAX_GLIBC="$(readelf --version-info "${BINARY}" | grep -o 'GLIBC_[0-9.]*' | sort -V | uniq | tail -1 || true)"
MAX_GLIBCXX="$(readelf --version-info "${BINARY}" | grep -o 'GLIBCXX_[0-9.]*' | sort -V | uniq | tail -1 || true)"
[[ -n "${MAX_GLIBC}" ]] || fail "no GLIBC_ version symbols found in ${BINARY}"

GLIBC_NUM="${MAX_GLIBC#GLIBC_}"
GLIBCXX_NUM="${MAX_GLIBCXX#GLIBCXX_}"

echo "=== Linux 分发兼容性门槛检查 ==="
echo "二进制:       ${BINARY}"
echo "最大 GLIBC:   ${MAX_GLIBC}   (构建底线 ${GLIBC_FLOOR})"
echo "最大 GLIBCXX: ${MAX_GLIBCXX} (构建底线 ${GLIBCXX_FLOOR})"
echo ""
echo "支持矩阵（需 glibc >= ${GLIBC_FLOOR}）:"
for distro in "Ubuntu 24.04 LTS" "Debian 13" "Fedora 41" "Arch / CachyOS / Manjaro"; do
    ver="${DISTRO_GLIBC[$distro]}"
    if ver_ge "${ver}" "${GLIBC_NUM}"; then
        printf '  [OK]   %-26s glibc %-5s\n' "${distro}" "${ver}"
    else
        printf '  [FAIL] %-26s glibc %-5s（产物需求 %s 超出）\n' "${distro}" "${ver}" "${MAX_GLIBC}"
    fi
done
echo ""

failures=0
if ! ver_ge "${GLIBC_FLOOR}" "${GLIBC_NUM}"; then
    printf 'FAIL: 最大 GLIBC 需求 %s 超出构建底线 %s\n' "${MAX_GLIBC}" "${GLIBC_FLOOR}" >&2
    failures=1
fi
if ! ver_ge "${GLIBCXX_FLOOR}" "${GLIBCXX_NUM}"; then
    printf 'FAIL: 最大 GLIBCXX 需求 %s 超出构建底线 %s\n' "${MAX_GLIBCXX}" "${GLIBCXX_FLOOR}" >&2
    failures=1
fi

if ((failures != 0)); then
    printf '结论: 门槛不达标——请改用保守构建环境（ubuntu-24.04 runner / 容器）构建 Release 产物。\n' >&2
    exit 1
fi

printf '结论: 门槛达标——产物可运行于全部支持矩阵发行版（glibc >= %s）。\n' "${GLIBC_FLOOR}"
