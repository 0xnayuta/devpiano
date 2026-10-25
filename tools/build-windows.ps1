[CmdletBinding()]
param(
    [string]$MirrorDir = $(if ($env:WIN_MIRROR_DIR) { $env:WIN_MIRROR_DIR } else { 'G:\source\projects\devpiano' }),
    [string]$ConfigurePreset = '',
    [string]$BuildPreset = '',
    [switch]$Release,
    [switch]$ClangTidy,
    [switch]$SkipConfigure,
    [switch]$Tests,
    [switch]$CtestVerbose,
    [string]$TestName = '',
    [string]$TestCategory = '',
    [string]$CtestArgsJson = '[]',
    [string]$VsInstallPath = $env:VSINSTALLDIR,
    [string]$VsDevCmdPath = $env:VS_DEVCMD_PATH
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $Tests -and ($CtestVerbose -or $TestName -or $TestCategory -or $CtestArgsJson -ne '[]')) {
    throw 'Test options require -Tests.'
}
if ($Tests -and $ClangTidy) {
    throw '-Tests cannot be combined with -ClangTidy.'
}
if ($TestName -and $TestCategory) {
    throw '-TestName and -TestCategory are mutually exclusive.'
}

$ctestExtraArgs = @()
if ($CtestArgsJson) {
    try {
        $parsedCtestArgs = $CtestArgsJson | ConvertFrom-Json
    } catch {
        throw "Invalid -CtestArgsJson: $($_.Exception.Message)"
    }
    if ($null -ne $parsedCtestArgs) {
        $ctestExtraArgs = @($parsedCtestArgs | ForEach-Object { [string]$_ })
    }
}
if ($Tests -and ($TestName -or $TestCategory) -and $ctestExtraArgs.Count -gt 0) {
    throw 'Extra ctest arguments cannot be combined with -TestName or -TestCategory.'
}

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

Push-Location $MirrorDir
try {
    Enter-DeveloperPowerShell -InstallationPath $installationPath -InstanceId $vsInstance.InstanceId
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
        if ($Tests) {
            $buildTestsMatch = [regex]::Match($cacheText, '(?m)^BUILD_TESTS:BOOL=(ON|OFF)\s*$')
            if (-not $buildTestsMatch.Success -or $buildTestsMatch.Groups[1].Value -ne 'ON') {
                throw "SkipConfigure requested for tests but BUILD_TESTS is not ON in $cmakeCache. Reconfigure without --no-configure."
            }
        }

        Write-Log 'skipping CMake configure; reusing the existing build tree'
    } else {
        $configureArgs = @('--preset', $ConfigurePreset)
        if ($Tests) {
            $configureArgs += '-DBUILD_TESTS=ON'
        }
        Write-Log ("cmake " + ($configureArgs -join ' '))
        & cmake @configureArgs
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed with exit code $LASTEXITCODE"
        }
    }

    if (-not $Tests) {
        Write-Log 'killing any stale DevPiano.exe process'
        Get-Process -Name "DevPiano" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    }

    $buildArgs = @('--build', '--preset', $BuildPreset)
    if ($Tests) {
        $buildArgs += @('--target', 'devpiano_tests')
    }
    Write-Log ("cmake " + ($buildArgs -join ' '))
    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        # 偶发 LNK1163（COMDAT 节选择无效）：WSL 镜像同步让 ninja 重编部分
        # obj，而 MSVC 增量链接的 .ilk 仍按旧 obj 布局合并新 COMDAT。清除
        # .ilk 后全量重链一次可恢复（ADR-001 镜像工作流的已知竞态）。
        Write-Log "build failed (exit $LASTEXITCODE); clearing incremental-link state and retrying once"
        Get-ChildItem -Path $MirrorDir -Recurse -Filter *.ilk -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue
        & cmake @buildArgs
    }
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }

    if ($Tests) {
        $testExitCode = 0
        if ($TestName -or $TestCategory) {
            $artefactsDir = Join-Path $resolvedBuildDir 'devpiano_tests_artefacts'
            $testExe = ''
            if ($presetBuild.BuildType) {
                $testExe = Join-Path $artefactsDir (Join-Path $presetBuild.BuildType 'devpiano_tests.exe')
            }
            if (-not $testExe -or -not (Test-Path -LiteralPath $testExe -PathType Leaf)) {
                $foundExe = Get-ChildItem -LiteralPath $artefactsDir -Filter 'devpiano_tests.exe' -Recurse -File -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                if ($null -ne $foundExe) {
                    $testExe = $foundExe.FullName
                }
            }
            if (-not $testExe -or -not (Test-Path -LiteralPath $testExe -PathType Leaf)) {
                throw "devpiano_tests.exe was not found under $artefactsDir"
            }
            $testArgs = @()
            if ($TestName) {
                $testArgs += @('--name', $TestName)
            } else {
                $testArgs += @('--category', $TestCategory)
            }
            if ($CtestVerbose) {
                $testArgs += '--verbose'
            }
            Write-Log ("running " + $testExe + " " + ($testArgs -join ' '))
            & $testExe @testArgs
            $testExitCode = $LASTEXITCODE
        } else {
            $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
            if ($null -eq $ctestCommand) {
                throw 'ctest was not found in PATH after entering the Visual Studio developer environment.'
            }
            $testArgs = @('--test-dir', $resolvedBuildDir, '--output-on-failure')
            if ($CtestVerbose) {
                $testArgs += '-V'
            }
            if ($ctestExtraArgs.Count -gt 0) {
                $testArgs += $ctestExtraArgs
            }
            Write-Log ("ctest " + ($testArgs -join ' '))
            & ctest @testArgs
            $testExitCode = $LASTEXITCODE
        }
        if ($testExitCode -ne 0) {
            throw "Tests failed with exit code $testExitCode"
        }
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

if ($Tests) {
    Write-Log 'MSVC test run completed successfully'
} else {
    Write-Log 'MSVC validation build completed successfully'
}
