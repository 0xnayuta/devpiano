[CmdletBinding()]
param(
    [string]$MirrorDir = $(if ($env:WIN_MIRROR_DIR) { $env:WIN_MIRROR_DIR } else { 'G:\source\projects\devpiano' }),
    [string]$ConfigurePreset = 'windows-msvc-debug',
    [string]$BuildPreset = 'windows-msvc-debug',
    [string]$VsInstallPath = $env:VSINSTALLDIR,
    [string]$VsDevCmdPath = $env:VS_DEVCMD_PATH
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Log {
    param([string]$Message)
    Write-Host "[build-windows] $Message"
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
    Write-Log "cmake --preset $ConfigurePreset"
    & cmake --preset $ConfigurePreset
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    Write-Log "cmake --build --preset $BuildPreset"
    & cmake --build --preset $BuildPreset
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

Write-Log 'MSVC validation build completed successfully'
