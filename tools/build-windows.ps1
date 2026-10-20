[CmdletBinding()]
param(
    [string]$MirrorDir = $(if ($env:WIN_MIRROR_DIR) { $env:WIN_MIRROR_DIR } else { 'G:\source\projects\devpiano' }),
    [string]$ConfigurePreset = '',
    [string]$BuildPreset = '',
    [switch]$Release,
    [switch]$ClangTidy,
    [switch]$SkipConfigure,
    [string]$VsInstallPath = $env:VSINSTALLDIR,
    [string]$VsDevCmdPath = $env:VS_DEVCMD_PATH
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Resolve presets: -Release flag overrides defaults
if ($Release) {
    if (-not $ConfigurePreset) { $ConfigurePreset = 'windows-msvc-release' }
    if (-not $BuildPreset) { $BuildPreset = 'windows-msvc-release' }
} else {
    if (-not $ConfigurePreset) { $ConfigurePreset = 'windows-msvc-debug' }
    if (-not $BuildPreset) { $BuildPreset = 'windows-msvc-debug' }
}

function Write-Log {
    param([string]$Message)
    Write-Host "[build-windows] $Message"
}

function Get-PresetList {
    param(
        [object]$Data,
        [string]$PropertyName
    )
    if ($null -eq $Data) { return @() }
    $property = $Data.PSObject.Properties[$PropertyName]
    if ($null -eq $property) { return @() }
    return @($property.Value)
}

function Get-PresetBuildDirectory {
    param(
        [string]$MirrorDir,
        [string]$ConfigurePresetName,
        [string]$BuildPresetName
    )

    $configurePresets = @{}
    $buildPresets = @{}
    foreach ($fileName in @('CMakePresets.json', 'CMakeUserPresets.json')) {
        $presetFile = Join-Path $MirrorDir $fileName
        if (-not (Test-Path -LiteralPath $presetFile -PathType Leaf)) { continue }
        try {
            $data = Get-Content -LiteralPath $presetFile -Raw | ConvertFrom-Json
        } catch {
            throw "Failed to read CMake preset file: $presetFile"
        }
        foreach ($preset in (Get-PresetList -Data $data -PropertyName 'configurePresets')) {
            if ($null -ne $preset -and $preset.PSObject.Properties['name']) {
                $configurePresets[[string]$preset.name] = $preset
            }
        }
        foreach ($preset in (Get-PresetList -Data $data -PropertyName 'buildPresets')) {
            if ($null -ne $preset -and $preset.PSObject.Properties['name']) {
                $buildPresets[[string]$preset.name] = $preset
            }
        }
    }

    if (-not $configurePresets.ContainsKey($ConfigurePresetName)) {
        throw "Configure preset not found in CMake presets: $ConfigurePresetName"
    }
    if (-not $buildPresets.ContainsKey($BuildPresetName)) {
        throw "Build preset not found in CMake presets: $BuildPresetName"
    }

    $buildPreset = $buildPresets[$BuildPresetName]
    $configureFromBuild = [string]$buildPreset.configurePreset
    if ($configureFromBuild -and $configureFromBuild -ne $ConfigurePresetName) {
        throw "Build preset '$BuildPresetName' uses configure preset '$configureFromBuild', not '$ConfigurePresetName'."
    }

    $configurePreset = $configurePresets[$ConfigurePresetName]
    $binaryDir = [string]$configurePreset.binaryDir
    if (-not $binaryDir) {
        throw "Configure preset '$ConfigurePresetName' has no binaryDir."
    }
    $binaryDir = $binaryDir.Replace('${sourceDir}', $MirrorDir)
    if (-not [System.IO.Path]::IsPathRooted($binaryDir)) {
        $binaryDir = Join-Path $MirrorDir $binaryDir
    }
    $resolvedBuildDir = [System.IO.Path]::GetFullPath($binaryDir).TrimEnd('\')
    $mirrorRoot = [System.IO.Path]::GetFullPath($MirrorDir).TrimEnd('\')
    if ($resolvedBuildDir -eq $mirrorRoot -or
        -not $resolvedBuildDir.StartsWith($mirrorRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Resolved build directory is outside the Windows mirror: $resolvedBuildDir"
    }

    $buildType = ''
    $cacheVariables = $configurePreset.PSObject.Properties['cacheVariables']
    if ($null -ne $cacheVariables -and $null -ne $cacheVariables.Value) {
        $buildTypeProperty = $cacheVariables.Value.PSObject.Properties['CMAKE_BUILD_TYPE']
        if ($null -ne $buildTypeProperty) {
            $buildType = [string]$buildTypeProperty.Value
        }
    }

    return [pscustomobject]@{
        BuildDir  = $resolvedBuildDir
        BuildType = $buildType
    }
}

function Resolve-VsInstance {
    param(
        [string]$ExplicitInstallPath,
        [string]$ExplicitDevCmdPath
    )

    if ($ExplicitInstallPath) {
        $resolvedInstallPath = (Resolve-Path $ExplicitInstallPath).Path
        return [pscustomobject]@{
            InstallationPath = $resolvedInstallPath
            InstanceId       = $null
        }
    }

    if ($ExplicitDevCmdPath) {
        $resolvedDevCmd = (Resolve-Path $ExplicitDevCmdPath).Path
        $installationPath = [System.IO.Path]::GetFullPath((Join-Path $resolvedDevCmd '..\..'))
        return [pscustomobject]@{
            InstallationPath = $installationPath
            InstanceId       = $null
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere.exe not found. Install Visual Studio Installer or add vswhere to the expected location.'
    }

    $json = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -format json
    if ($LASTEXITCODE -ne 0 -or -not $json) {
        throw 'vswhere.exe failed to locate a suitable Visual Studio installation.'
    }

    $instances = $json | ConvertFrom-Json
    $instance = @($instances)[0]
    if (-not $instance) {
        throw 'No Visual Studio installation was returned by vswhere.'
    }

    return [pscustomobject]@{
        InstallationPath = $instance.installationPath
        InstanceId       = $instance.instanceId
    }
}

function Enter-DeveloperPowerShell {
    param(
        [string]$InstallationPath,
        [string]$InstanceId
    )

    $devShellModule = Join-Path $InstallationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path $devShellModule)) {
        throw "Developer PowerShell module not found: $devShellModule"
    }

    Import-Module $devShellModule -Force

    $devCmdArguments = '-arch=x64 -host_arch=x64'

    if ($InstanceId) {
        Write-Log "Entering Developer PowerShell via instance id: $InstanceId"
        Enter-VsDevShell $InstanceId -SkipAutomaticLocation -DevCmdArguments $devCmdArguments | Out-Null
        return
    }

    Write-Log "Entering Developer PowerShell via install path: $InstallationPath"
    Enter-VsDevShell -VsInstallPath $InstallationPath -SkipAutomaticLocation -DevCmdArguments $devCmdArguments | Out-Null
}

$MirrorDir = [System.IO.Path]::GetFullPath($MirrorDir)
if (-not (Test-Path $MirrorDir)) {
    throw "MirrorDir does not exist: $MirrorDir"
}

$vsInstance = Resolve-VsInstance -ExplicitInstallPath $VsInstallPath -ExplicitDevCmdPath $VsDevCmdPath
$installationPath = [System.IO.Path]::GetFullPath($vsInstance.InstallationPath)

Write-Log "mirror: $MirrorDir"
Write-Log "VS installation: $installationPath"
Write-Log "configure preset: $ConfigurePreset"
Write-Log "build preset: $BuildPreset"

Enter-DeveloperPowerShell -InstallationPath $installationPath -InstanceId $vsInstance.InstanceId

Push-Location $MirrorDir
try {
    $presetBuild = Get-PresetBuildDirectory -MirrorDir $MirrorDir `
        -ConfigurePresetName $ConfigurePreset -BuildPresetName $BuildPreset
    $resolvedBuildDir = [string]$presetBuild.BuildDir
    Write-Log "resolved build directory: $resolvedBuildDir"

    if ($SkipConfigure) {
        $buildNinja = Join-Path $resolvedBuildDir 'build.ninja'
        $cmakeCache = Join-Path $resolvedBuildDir 'CMakeCache.txt'
        if (-not (Test-Path -LiteralPath $buildNinja -PathType Leaf)) {
            throw "SkipConfigure requested but build.ninja was not found: $buildNinja"
        }
        if (-not (Test-Path -LiteralPath $cmakeCache -PathType Leaf)) {
            throw "SkipConfigure requested but CMakeCache.txt was not found: $cmakeCache"
        }

        $cacheText = Get-Content -LiteralPath $cmakeCache -Raw
        if ($cacheText -notmatch '(?m)^CMAKE_GENERATOR:INTERNAL=Ninja\s*$') {
            throw "SkipConfigure requested but the cached generator is not Ninja: $cmakeCache"
        }
        $homeMatch = [regex]::Match($cacheText, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$')
        if ($homeMatch.Success) {
            $cacheHome = [System.IO.Path]::GetFullPath($homeMatch.Groups[1].Value.Trim()).TrimEnd('\')
            $mirrorRoot = [System.IO.Path]::GetFullPath($MirrorDir).TrimEnd('\')
            if (-not [string]::Equals($cacheHome, $mirrorRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "SkipConfigure requested but CMakeCache.txt belongs to another source tree: $cacheHome"
            }
        }
        if ($presetBuild.BuildType) {
            $buildTypeMatch = [regex]::Match(
                $cacheText,
                '(?m)^CMAKE_BUILD_TYPE:STRING=(.+)$'
            )
            if ($buildTypeMatch.Success -and
                $buildTypeMatch.Groups[1].Value.Trim() -ne $presetBuild.BuildType) {
                throw "SkipConfigure requested but CMAKE_BUILD_TYPE is '$($buildTypeMatch.Groups[1].Value.Trim())', expected '$($presetBuild.BuildType)'."
            }
        }

        Write-Log 'skipping CMake configure; reusing the existing build tree'
    } else {
        Write-Log "cmake --preset $ConfigurePreset"
        & cmake --preset $ConfigurePreset
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed with exit code $LASTEXITCODE"
        }
    }

    Write-Log 'killing any stale DevPiano.exe process'
    Get-Process -Name "DevPiano" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

    Write-Log "cmake --build --preset $BuildPreset"
    & cmake --build --preset $BuildPreset
    if ($LASTEXITCODE -ne 0) {
        # 偶发 LNK1163（COMDAT 节选择无效）：WSL 镜像同步让 ninja 重编部分
        # obj，而 MSVC 增量链接的 .ilk 仍按旧 obj 布局合并新 COMDAT。清除
        # .ilk 后全量重链一次可恢复（ADR-001 镜像工作流的已知竞态）。
        Write-Log "build failed (exit $LASTEXITCODE); clearing incremental-link state and retrying once"
        Get-ChildItem -Path $MirrorDir -Recurse -Filter *.ilk -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue
        & cmake --build --preset $BuildPreset
    }
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }

    if ($ClangTidy) {
        Write-Log "cmake --build --preset $BuildPreset --target clang-tidy"
        & cmake --build --preset $BuildPreset --target clang-tidy 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[build-windows] clang-tidy returned warnings (non-blocking)" -ForegroundColor Yellow
        } else {
            Write-Log "clang-tidy: clean (0 diagnostics)"
        }
    }
} finally {
    Pop-Location
}

Write-Log 'MSVC validation build completed successfully'
