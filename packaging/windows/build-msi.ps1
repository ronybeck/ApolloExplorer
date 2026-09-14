<#
.SYNOPSIS
    Builds a native MSI installer for ApolloExplorer (Windows 11), using the
    WiX Toolset v4/v5.

.DESCRIPTION
    Builds ApolloExplorer.exe, acp.exe and ash.exe with Qt6/MinGW, stages the
    runtime (via windeployqt), then packages the staged folder into an MSI
    with ApolloExplorer.wxs. The MSI also adds the install folder to the
    machine PATH so acp and ash are usable from any command prompt.

.PARAMETER Version
    Product version to embed in the MSI (also used in the output filename).
    If omitted, it is extracted from VERSION_STRING in protocolTypes.h at the
    repository root.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File packaging\windows\build-msi.ps1 -Version 1.4.0

.NOTES
    Prerequisites:
      - Qt 6 with the MinGW kit (matches the layout used by build_windows_client.ps1)
      - .NET SDK (for the 'wix' dotnet tool: dotnet tool install --global wix)
#>
param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Resolve-Path (Join-Path $ScriptDir "..\..")
$StageDir  = Join-Path $ScriptDir "stage\ApolloExplorer-Windows"
$DistDir   = Join-Path $ScriptDir "dist"

if (-not $Version) {
    $VersionHeader = Join-Path $RepoRoot "protocolTypes.h"
    $HeaderContent = Get-Content $VersionHeader -Raw
    if ($HeaderContent -match '#define\s+VERSION_STRING\s+"([^"]+)"') {
        $Version = $Matches[1]
    } else {
        Write-Error "Could not extract VERSION_STRING from $VersionHeader"
        exit 1
    }
}

Write-Host "########## ApolloExplorer MSI builder (version $Version) ##########"

Write-Host "==> 0. Checking prerequisites"

$UserPath = Resolve-Path ~
$QT6InstallPath = Join-Path $UserPath "Qt6"
$QT6CommandPath = Join-Path $QT6InstallPath "6.11.1\mingw_64\bin"
$QT6ToolsPath   = Join-Path $QT6InstallPath "Tools\mingw1310_64\bin"
if (Test-Path $QT6CommandPath) { $env:Path += ";$QT6CommandPath" }
if (Test-Path $QT6ToolsPath)   { $env:Path += ";$QT6ToolsPath" }

foreach ($tool in @("qmake", "mingw32-make", "windeployqt")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "'$tool' not found on PATH. Install Qt6 (MinGW kit) first - see build_windows_client.ps1 at the repository root."
        exit 1
    }
}

if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    if (Get-Command dotnet -ErrorAction SilentlyContinue) {
        Write-Host "* WiX CLI not found, installing 'wix' dotnet tool"
        dotnet tool install --global wix | Out-Null
        $env:Path += ";$(Join-Path $env:USERPROFILE '.dotnet\tools')"
    }
    if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
        Write-Error "WiX CLI ('wix') not found and could not be installed automatically. Install the .NET SDK, then run: dotnet tool install --global wix"
        exit 1
    }
}

Write-Host "==> 1. Clean previous build output"
Remove-Item -Recurse -Force $StageDir -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $DistDir -ErrorAction SilentlyContinue
Push-Location $RepoRoot
mingw32-make.exe distclean *>$null
Remove-Item -Recurse -Force ".\acp\release\", ".\ApolloExplorerPC\release\", ".\ash\release\", ".\AmigaIconReader\release\" -ErrorAction SilentlyContinue

Write-Host "==> 2. Configuring project (qmake, release)"
qmake -recursive -config release

Write-Host "==> 3. Building (mingw32-make)"
mingw32-make.exe -j8

$AppExe = Join-Path $RepoRoot "ApolloExplorerPC\release\ApolloExplorer.exe"
$AcpExe = Join-Path $RepoRoot "acp\release\acp.exe"
$AshExe = Join-Path $RepoRoot "ash\release\ash.exe"
if (-not (Test-Path $AppExe)) { Write-Error "Expected build output missing: $AppExe"; exit 1 }
if (-not (Test-Path $AcpExe)) { Write-Error "Expected build output missing: $AcpExe"; exit 1 }
if (-not (Test-Path $AshExe)) { Write-Error "Expected build output missing: $AshExe"; exit 1 }

Write-Host "==> 4. Staging application"
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
Copy-Item $AppExe $StageDir
Copy-Item $AcpExe $StageDir
Copy-Item $AshExe $StageDir
windeployqt.exe --release (Join-Path $StageDir "ApolloExplorer.exe")

Write-Host "==> 5. Clean intermediate build output"
mingw32-make.exe distclean *>$null
Remove-Item -Recurse -Force ".\acp\release\", ".\ApolloExplorerPC\release\", ".\ash\release\", ".\AmigaIconReader\release\" -ErrorAction SilentlyContinue
Pop-Location

Write-Host "==> 6. Building MSI (wix)"
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$MsiFile = Join-Path $DistDir "ApolloExplorer-$Version.msi"
$WxsFile = Join-Path $ScriptDir "ApolloExplorer.wxs"

wix build $WxsFile -arch x64 -d "SourceDir=$StageDir" -d "AppVersion=$Version" -out $MsiFile

Write-Host ""
Write-Host "Done: $MsiFile"
