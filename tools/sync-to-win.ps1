[CmdletBinding()]
param(
    [string]$SourceDir,
    [string]$MirrorDir = $(if ($env:WIN_MIRROR_DIR) { $env:WIN_MIRROR_DIR } else { 'G:\source\projects\devpiano' }),
    [switch]$CheckOnly,
    [switch]$Full,
    [string]$SubmoduleFingerprint = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Log {
    param([string]$Message)
    Write-Host "[sync-to-win] $Message"
}

if (-not $SourceDir) {
    throw 'SourceDir is required.'
}

$SourceDir = [System.IO.Path]::GetFullPath($SourceDir)
$MirrorDir = [System.IO.Path]::GetFullPath($MirrorDir)

Write-Log "source: $SourceDir"
Write-Log "mirror: $MirrorDir"
Write-Log "preserve mirror build dirs: build-win-msvc, build-win-msvc-release, dist"

New-Item -ItemType Directory -Force -Path $MirrorDir | Out-Null

# 排除清单与仓库 .gitignore 保持一致：此处列出的内容不应进入镜像，
# 且镜像侧同名内容不会被 /MIR 删除。改动 .gitignore 时须同步此处。
# 例外：.gitignore 中 [Dd]ebug/ 等 "Visual Studio 输出目录" 规则是 git 层
# 预防（防违规在源树内直接构建）；构建产物目录已规范化为 build-* 前缀
# （源侧动态排除、镜像侧显式保留），故不在此列出。

# 名字级排除：robocopy 裸名匹配任意层级目录，源与镜像两侧同时生效
# （源侧不复制，镜像侧不删除）。
$excludeDirNames = @(
    '.git',            # git 元数据目录；同时保护镜像侧已有 git 仓库不被 /MIR 删除
    '.vs', '.idea', '.vscode',   # IDE 状态
    '.cache', '.codegraph',      # 本地缓存 / codegraph 索引
    '.omp',                      # 本地工具 / 会话 skill（.gitignore 同步）
    '__pycache__',               # Python 缓存
    'ipch'                       # VS 智能感知缓存
)

# 路径级排除：仅源根——这些目录名可能在 submodules 内部出现，裸名会误伤
$excludeSourceDirs = @(
    (Join-Path $SourceDir 'build'),
    (Join-Path $SourceDir 'out'),
    (Join-Path $SourceDir 'bin'),
    (Join-Path $SourceDir 'obj'),
    (Join-Path $SourceDir 'CMakeFiles')
)

# 动态收集源根 build-* 构建目录（build-wsl-clang、build-asan 等）
Get-ChildItem -Path $SourceDir -Directory -Filter 'build-*' -ErrorAction SilentlyContinue |
    ForEach-Object { $excludeSourceDirs += $_.FullName }

# 路径级排除：仅镜像侧——构建/打包产物必须保留；
# 与源侧排除对称：这些目录若出现在镜像侧，也不被 /MIR 删除
$excludeMirrorDirs = @(
    (Join-Path $MirrorDir 'build-win-msvc'),
    (Join-Path $MirrorDir 'build-win-msvc-release'),
    (Join-Path $MirrorDir 'dist'),
    (Join-Path $MirrorDir 'build'),
    (Join-Path $MirrorDir 'out'),
    (Join-Path $MirrorDir 'bin'),
    (Join-Path $MirrorDir 'obj'),
    (Join-Path $MirrorDir 'CMakeFiles')
)

# 文件级排除：/XF 全局模式匹配，两侧同时生效。
# robocopy 限制：/XF 只保护"extra 文件"不被 /MIR 删除；若整个目录在源中
# 不存在（extra 目录），robocopy 会整体删除该目录，无视其中匹配 /XF 的
# 文件。因此镜像侧构建/打包产物必须放在 /XD 保护的目录（build-win-msvc、
# dist 等）内，散落在 extra 目录中的文件仅靠 /XF 不可靠。
$excludeFiles = @(
    '.mirror_submodules.hash',  # 镜像端子模块同步指纹标记文件
    '.git',            # 子模块 gitlink 指针文件
    'CMakeCache.txt', 'compile_commands.json',
    # IDE 状态文件
    '*.suo', '*.user', '*.userosscache', '*.rsuser', '*.vcxproj.user',
    '*.VC.db', '*.VC.opendb', '*.opendb', '*.opensdf', '*.sdf', '*.cachefile',
    # SQLite WAL / journal（Browse.VC.db、CodeChunks.db 等）
    '*.db-shm', '*.db-wal', '*.db-journal', '*.db-corrupt',
    # 编译产物
    '*.obj', '*.iobj', '*.o', '*.lo', '*.gch', '*.pch',
    '*.lib', '*.dll', '*.exe', '*.a', '*.so', '*.dylib', '*.app', '*.out',
    '*.ilk', '*.idb', '*.exp', '*.pdb', '*.ipdb', '*.pgc', '*.pgd', '*.dwo',
    # 临时与日志
    '*.cache', '*.tmp', '*.log', '*.binlog', '*.pyc'
)

$fingerprintFile = Join-Path $MirrorDir '.mirror_submodules.hash'
$mirrorJuce = Join-Path $MirrorDir 'submodules\JUCE\CMakeLists.txt'
$mirrorJive = Join-Path $MirrorDir 'submodules\JIVE\CMakeLists.txt'
$mirrorInspector = Join-Path $MirrorDir 'submodules\melatonin_inspector\CMakeLists.txt'

$effectiveFingerprint = $SubmoduleFingerprint
if (-not $effectiveFingerprint) {
    $gitmodulesFile = Join-Path $SourceDir '.gitmodules'
    if (Test-Path -LiteralPath $gitmodulesFile -PathType Leaf) {
        $effectiveFingerprint = (Get-FileHash -LiteralPath $gitmodulesFile -Algorithm SHA256).Hash
    }
}

$needsSubmoduleSync = $Full -or
    (-not (Test-Path -LiteralPath $fingerprintFile -PathType Leaf)) -or
    (-not (Test-Path -LiteralPath $mirrorJuce -PathType Leaf)) -or
    (-not (Test-Path -LiteralPath $mirrorJive -PathType Leaf)) -or
    (-not (Test-Path -LiteralPath $mirrorInspector -PathType Leaf))

if (-not $needsSubmoduleSync -and $effectiveFingerprint) {
    $cachedFingerprint = (Get-Content -LiteralPath $fingerprintFile -Raw -ErrorAction SilentlyContinue)
    if ($null -ne $cachedFingerprint) {
        $cachedFingerprint = $cachedFingerprint.Trim()
    }
    if ($cachedFingerprint -ne $effectiveFingerprint) {
        $needsSubmoduleSync = $true
    }
}

if (-not $needsSubmoduleSync) {
    Write-Log 'submodules unchanged (cached) — skipping submodules scan (use -Full for full sync)'
    $excludeSourceDirs += (Join-Path $SourceDir 'submodules')
    $excludeMirrorDirs += (Join-Path $MirrorDir 'submodules')
} else {
    Write-Log 'submodules sync required (full scan)...'
}

$roboArgs = @(
    $SourceDir,
    $MirrorDir,
    '/MIR',
    '/FFT',
    '/MT:16',
    '/R:2',
    '/W:1',
    '/XD'
) + $excludeDirNames + $excludeSourceDirs + $excludeMirrorDirs + @('/XF') + $excludeFiles

if ($CheckOnly) {
    # /L 只列出变更而不执行；不附加 /NFL /NDL /NP，保证文件清单可见
    $roboArgs += '/L'
    Write-Log '[CHECK ONLY] listing changes - no files will be copied or deleted'
} else {
    $roboArgs += @('/NFL', '/NDL', '/NP')
}

Write-Log ('robocopy args: ' + ($roboArgs -join ' '))

& robocopy @roboArgs
$exitCode = $LASTEXITCODE

if ($exitCode -ge 8) {
    throw "robocopy failed with exit code $exitCode"
}
if ($needsSubmoduleSync -and (-not $CheckOnly) -and $effectiveFingerprint) {
    Set-Content -LiteralPath $fingerprintFile -Value $effectiveFingerprint -Force
    Write-Log 'submodule fingerprint updated'
}

Write-Log "robocopy completed with exit code $exitCode"
