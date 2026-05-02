[CmdletBinding()]
param(
    [string]$SourceDir,
    [string]$MirrorDir = $(if ($env:WIN_MIRROR_DIR) { $env:WIN_MIRROR_DIR } else { 'G:\source\projects\devpiano' }),
    [switch]$CheckOnly
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
Write-Log "preserve mirror build dirs: build-win-msvc, build-win-msvc-release"

New-Item -ItemType Directory -Force -Path $MirrorDir | Out-Null

$excludeDirPaths = @(
    # IDE / editor local state — must be excluded on BOTH sides
    # Source side: avoid scanning WSL-side editor dirs
    # Mirror side: avoid treating Windows-local IDE files as "extra" to delete
    (Join-Path $SourceDir '.git'),
    (Join-Path $SourceDir '.vs'),
    (Join-Path $SourceDir '.idea'),
    (Join-Path $SourceDir '.vscode'),
    (Join-Path $SourceDir 'build'),
    (Join-Path $SourceDir 'out'),
    (Join-Path $SourceDir 'bin'),
    (Join-Path $SourceDir 'obj'),
    (Join-Path $SourceDir '.cache'),
    (Join-Path $SourceDir '__pycache__'),
    (Join-Path $SourceDir 'CMakeFiles'),
    # Mirror-local build output
    (Join-Path $MirrorDir 'build-win-msvc'),
    (Join-Path $MirrorDir 'build-win-msvc-release'),
    # Mirror-local IDE / editor state (VS, JetBrains, etc.)
    (Join-Path $MirrorDir '.vs'),
    (Join-Path $MirrorDir '.idea'),
    (Join-Path $MirrorDir '.vscode')
)

Get-ChildItem -Path $SourceDir -Directory -Filter 'build-*' -ErrorAction SilentlyContinue |
    ForEach-Object { $excludeDirPaths += $_.FullName }

$excludeFiles = @(
    'CMakeCache.txt',
    'compile_commands.json',
    # IDE / editor state files
    '*.suo', '*.user', '*.userosscache', '*.rsuser',
    '*.vcxproj.user', '*.VC.db', '*.VC.opendb', '*.opendb', '*.opensdf', '*.sdf', '*.ipch',
    # SQLite WAL / journal files (Browse.VC.db, CodeChunks.db, SemanticSymbols.db, etc.)
    '*.db-shm', '*.db-wal', '*.db-journal', '*.db-corrupt',
    # Build artifacts
    '*.obj', '*.iobj', '*.pch', '*.pdb', '*.ilk', '*.idb', '*.exp', '*.lib', '*.dll', '*.exe',
    '*.a', '*.so', '*.dylib', '*.app', '*.out', '*.o', '*.lo', '*.la',
    '*.cache', '*.tmp', '*.log', '*.binlog'
)

$roboArgs = @(
    $SourceDir,
    $MirrorDir,
    '/MIR',
    '/FFT',
    '/Z',
    '/R:2',
    '/W:1',
    '/NFL',
    '/NDL',
    '/NP',
    '/XD'
) + $excludeDirPaths + @('/XF') + $excludeFiles

if ($CheckOnly) {
    $roboArgs += '/L'
    Write-Log ('[CHECK ONLY] listing changes — no files will be copied or deleted')
    Write-Log ('robocopy args: ' + ($roboArgs -join ' '))
} else {
    Write-Log ('robocopy args: ' + ($roboArgs -join ' '))
}

& robocopy @roboArgs
$exitCode = $LASTEXITCODE

if ($exitCode -ge 8) {
    throw "robocopy failed with exit code $exitCode"
}

Write-Log "robocopy completed with exit code $exitCode"
