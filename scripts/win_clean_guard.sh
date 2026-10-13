#!/usr/bin/env bash
# Windows 镜像树构建目录清理守卫。
#
# 用途：在删除 Windows 侧 MSVC 构建目录（build-win-msvc / build-win-msvc-release）
# 之前做 fail-closed 校验：只有目标路径身份可证明、且目录内不含构建产物以外的
# 资产（历史证据、归档、未知条目）时才允许删除。
#
# 由 scripts/build_msvc_from_wsl.sh 的 --clean-win-build 路径调用，也可单独运行
# 做人工预检：
#   bash scripts/win_clean_guard.sh /mnt/g/source/projects/devpiano build-win-msvc
#
# 退出码：0 = 允许删除；1 = 拒绝删除（诊断见输出）；2 = 用法错误。

set -euo pipefail

SCRIPT_NAME="win_clean_guard"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  C_RESET='\033[0m'
  C_INFO='\033[1;34m'
  C_WARN='\033[1;33m'
  C_ERROR='\033[1;31m'
else
  C_RESET=''
  C_INFO=''
  C_WARN=''
  C_ERROR=''
fi

info() {
  printf '%b[%s]%b %s\n' "${C_INFO}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

warn() {
  printf '%b[%s WARN]%b %s\n' "${C_WARN}" "${SCRIPT_NAME}" "${C_RESET}" "$*"
}

refuse() {
  printf '%b[%s REFUSE]%b %s\n' "${C_ERROR}" "${SCRIPT_NAME}" "${C_RESET}" "$*" >&2
}

usage() {
  cat <<'EOF'
Usage: scripts/win_clean_guard.sh <wsl-mirror-path> <build-dir-name> [--allow-non-build-assets]

Arguments:
  <wsl-mirror-path>   Windows 镜像根目录的 WSL 路径（如 /mnt/g/source/projects/devpiano）
  <build-dir-name>   待清理构建目录名，仅允许 build-win-msvc / build-win-msvc-release
  --allow-non-build-assets
                     显式允许删除含非构建资产的构建目录（需人工二次确认）

Exit codes:
  0  allowed to delete
  1  refused
  2  usage error
EOF
}

# 只允许清理预设产出的构建根名；新增预设构建目录时在此登记。
is_allowed_build_dir_name() {
  case "$1" in
    build-win-msvc|build-win-msvc-release) return 0 ;;
    *) return 1 ;;
  esac
}

# 已知的 CMake/Ninja/MSVC/JUCE 构建产物名字。
# 未命中者一律视为"非构建资产"（历史证据、归档、未知条目），需要显式放行。
is_known_build_entry() {
  case "$1" in
    # CMake / Ninja 状态
    .ninja_deps|.ninja_log|.ninja_*|CMakeCache.txt|CMakeFiles|cmake_install.cmake|CTestTestfile.cmake|Testing)
      return 0 ;;
    compile_commands.json|build.ninja|CMakeUserPresets.json|install_manifest.txt|DartConfiguration.tcl)
      return 0 ;;
    # CMake 子构建目录与 IDE 状态
    .cmake|_deps|vcpkg_installed|.vs|.cache|out|bin|obj|ipch|x64|x86|Debug|Release|RelWithDebInfo|MinSizeRel)
      return 0 ;;
    # JUCE / 同步
    submodules|juce_*)
      return 0 ;;
    # 目标产物目录
    *_artefacts)
      return 0 ;;
    # 编译/链接产物
    *.lib|*.dll|*.exe|*.pdb|*.ilk|*.obj|*.iobj|*.tlog|*.recipe|*.bin|*.a|*.so|*.dylib|*.exp|*.ipdb|*.dwo|*.pgc|*.pgd)
      return 0 ;;
    # 构建期文本状态
    *.log|*.json|*.txt|*.cmake|*.ninja|*.vcxproj|*.sln|*.filters|*.user|*.db|*.cache|*.tmp|*.trace|*.suo)
      return 0 ;;
    *)
      return 1 ;;
  esac
}

mirror_arg=""
build_name=""
allow_non_build=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --allow-non-build-assets)
      allow_non_build=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      printf '%s ERROR: unknown option: %s\n' "${SCRIPT_NAME}" "$1" >&2
      usage >&2
      exit 2
      ;;
    *)
      if [[ -z "${mirror_arg}" ]]; then
        mirror_arg="$1"
      elif [[ -z "${build_name}" ]]; then
        build_name="$1"
      else
        printf '%s ERROR: unexpected argument: %s\n' "${SCRIPT_NAME}" "$1" >&2
        usage >&2
        exit 2
      fi
      shift
      ;;
  esac
done

if [[ -z "${mirror_arg}" || -z "${build_name}" ]]; then
  usage >&2
  exit 2
fi

command -v realpath >/dev/null 2>&1 || {
  refuse 'realpath not found; cannot prove destination identity'
  exit 1
}

# 1) 构建目录名必须在允许集合内（拒绝把任意目录名拼进删除路径）。
if ! is_allowed_build_dir_name "${build_name}"; then
  refuse "build directory name not allowed: '${build_name}' (expected build-win-msvc / build-win-msvc-release)"
  exit 1
fi

# 2) 镜像根必须先规范化（解析已存在的符号链接组件），再证明身份。
if [[ -z "${mirror_arg}" || "${mirror_arg}" == "/" ]]; then
  refuse "mirror path is empty or filesystem root: '${mirror_arg}'"
  exit 1
fi

real_mirror="$(realpath -m -- "${mirror_arg}")"

if [[ "${real_mirror}" == "/" ]]; then
  refuse 'mirror path resolves to filesystem root'
  exit 1
fi

# 3) 镜像根必须是 /mnt/<drive>/... 下的 drvfs 路径（Windows 可映射），
#    排除盘符根（/mnt/g）、WSL 原生文件系统与 WSL 主源码根。
if [[ ! "${real_mirror}" =~ ^/mnt/[a-zA-Z]/[^/] ]]; then
  refuse "mirror path must live under a Windows-mounted drive (/mnt/<drive>/...): '${real_mirror}'"
  exit 1
fi

if [[ "${real_mirror}" =~ ^/mnt/[a-zA-Z]$ ]]; then
  refuse "mirror path is a drive root: '${real_mirror}'"
  exit 1
fi

# 4) 逐组件而不是字符串前缀地证明父子关系：目标的父目录必须逐字等于镜像根。
target="${real_mirror}/${build_name}"
target_real="$(realpath -m -- "${target}")"
if [[ "$(dirname -- "${target_real}")" != "${real_mirror}" ]]; then
  refuse "resolved target '${target_real}' is not a direct child of mirror root '${real_mirror}'"
  exit 1
fi
if [[ "$(basename -- "${target_real}")" != "${build_name}" ]]; then
  refuse "resolved target name '$(basename -- "${target_real}")' does not match '${build_name}'"
  exit 1
fi

# 5) 目标自身不得是符号链接（避免删除链接指向的其它目录内容）。
if [[ -L "${target}" ]]; then
  refuse "target is a symbolic link: '${target}' (refusing to recurse through it)"
  exit 1
fi

if [[ -e "${target_real}" && ! -d "${target_real}" ]]; then
  refuse "target exists but is not a directory: '${target_real}'"
  exit 1
fi

if [[ ! -d "${target_real}" ]]; then
  info "target does not exist, nothing to clean: ${target_real}"
  info 'guard PASS (no-op)'
  exit 0
fi

# 6) 内容检查：识别构建目录中的非构建资产（历史证据、归档、未知条目）。
non_build_entries=()
all_entries=()
shopt -s nullglob dotglob
for entry in "${target_real}"/*; do
  name="$(basename -- "${entry}")"
  all_entries+=("${name}")
  if ! is_known_build_entry "${name}"; then
    non_build_entries+=("${name}")
  fi
done
shopt -u nullglob dotglob

info "resolved mirror root : ${real_mirror}"
info "resolved target      : ${target_real}"
info "top-level entries    : ${#all_entries[@]}"
if [[ ${#all_entries[@]} -gt 0 ]]; then
  info "delete scope (top-level): ${all_entries[*]}"
fi

if [[ ${#non_build_entries[@]} -gt 0 ]]; then
  warn 'non-build assets detected in the build directory:'
  for name in "${non_build_entries[@]}"; do
    warn "  - ${name}"
  done
  warn 'these are NOT reproducible build outputs (evidence archives, reports, unknown data).'
  if [[ "${allow_non_build}" != "1" ]]; then
    refuse 'refusing to delete: archive/verify the entries above, then re-run with --allow-non-build-assets'
    exit 1
  fi
  warn '--allow-non-build-assets given: proceeding, the entries above WILL BE DELETED.'
fi

info "guard PASS for: ${target_real}"
exit 0
