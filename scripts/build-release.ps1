[CmdletBinding()]
param(
    [ValidateSet("stable", "development")]
    [string] $BuildChannel = "stable"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..")
)
$vcpkgRoot = if (-not [string]::IsNullOrWhiteSpace($env:DME_VCPKG_ROOT)) {
    [System.IO.Path]::GetFullPath($env:DME_VCPKG_ROOT)
} elseif (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
    [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
} else {
    throw "VCPKG_ROOT is not set. Set it to your existing vcpkg installation, for example: `$env:VCPKG_ROOT = 'C:\vcpkg'"
}
$vcpkgExecutable = Join-Path $vcpkgRoot "vcpkg.exe"
$releaseArchive = Join-Path $repositoryRoot "release\DewralMapEditor-windows-x64.zip"
$sourceArchive = Join-Path $repositoryRoot "release\DewralMapEditor-source.zip"

function Require-Command {
    param(
        [Parameter(Mandatory)]
        [string] $Name,

        [Parameter(Mandatory)]
        [string] $InstallHint
    )

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found. $InstallHint"
    }
}

Write-Host "Dewral Map Editor - Windows release build" -ForegroundColor Cyan
Write-Host "Repository: $repositoryRoot"

Require-Command "cmake.exe" "Install CMake 3.24 or newer and reopen this terminal."

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio was not found. Install the Desktop development with C++ workload."
}

$visualStudioPath = (& $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath | Select-Object -First 1)

# Some VS2026 Insiders installations are not reported by the older vswhere
# shipped on the machine. Fall back to the installed x64 toolchain script.
if (-not $visualStudioPath) {
    $visualStudioRoot = Join-Path ${env:ProgramFiles} "Microsoft Visual Studio"
    $vcVarsFile = Get-ChildItem -LiteralPath $visualStudioRoot `
        -Filter "vcvars64.bat" -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($vcVarsFile) {
        $visualStudioPath = $vcVarsFile.Directory.Parent.Parent.Parent.FullName
    }
}
if (-not $visualStudioPath) {
    throw "The Visual Studio C++ toolchain was not found. Install the Desktop development with C++ workload."
}
$visualStudioPath = ([string]$visualStudioPath).Trim()
Write-Host "MSVC: $visualStudioPath"

$cmakeExecutable = (Get-Command cmake.exe).Source
$bundledCmake = Join-Path $visualStudioPath `
    "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (Test-Path -LiteralPath $bundledCmake) {
    $cmakeExecutable = $bundledCmake
}
Write-Host "CMake: $cmakeExecutable"

$cmakeVersionText = (& $cmakeExecutable --version | Select-Object -First 1)
if ($cmakeVersionText -notmatch "cmake version (\d+)\.(\d+)") {
    throw "The installed CMake version could not be detected."
}
if ([int]$Matches[1] -lt 3 -or
    ([int]$Matches[1] -eq 3 -and [int]$Matches[2] -lt 24)) {
    throw "CMake 3.24 or newer is required. Found: $cmakeVersionText"
}

$configurePreset = "windows-vcpkg"
$buildPreset = "windows-release"
$manifestLog = Join-Path $repositoryRoot "build\vcpkg-windows\vcpkg-manifest-install.log"
if ($visualStudioPath -match "[\\/]18[\\/]") {
    $configurePreset = "windows-vs2026-vcpkg"
    $buildPreset = "windows-vs2026-release"
    $manifestLog = Join-Path $repositoryRoot "build\vcpkg-windows-vs2026\vcpkg-manifest-install.log"
}

if (-not (Test-Path -LiteralPath $vcpkgExecutable)) {
    throw "vcpkg.exe was not found under $vcpkgRoot. Set VCPKG_ROOT to an existing vcpkg installation."
}
if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"))) {
    throw "The vcpkg toolchain was not found under $vcpkgRoot. Set VCPKG_ROOT to an existing vcpkg installation."
}

$binaryCache = Join-Path $repositoryRoot ".cache\vcpkg"
New-Item -ItemType Directory -Force -Path $binaryCache | Out-Null

$env:VCPKG_ROOT = $vcpkgRoot
$env:VCPKG_DISABLE_METRICS = "1"
if (-not $env:VCPKG_BINARY_SOURCES) {
    $env:VCPKG_BINARY_SOURCES = "clear;files,$binaryCache,readwrite"
}

Push-Location $repositoryRoot
try {
    Write-Host "Configuring the project and installing dependencies..."
    & $cmakeExecutable --preset $configurePreset "-DDME_BUILD_CHANNEL=$BuildChannel"
    if ($LASTEXITCODE -ne 0) {
        if (Test-Path -LiteralPath $manifestLog) {
            Write-Host ""
            Write-Host "Last vcpkg messages:" -ForegroundColor Yellow
            Get-Content -LiteralPath $manifestLog -Tail 50 |
                ForEach-Object { Write-Host $_ }
        }
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }

    Write-Host "Building and packaging the Release configuration..."
    & $cmakeExecutable --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) {
        throw "The release build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $releaseArchive)) {
    throw "The build completed, but the release archive was not created."
}
if (-not (Test-Path -LiteralPath $sourceArchive)) {
    throw "The build completed, but the source archive was not created."
}

$archiveSize = [math]::Round((Get-Item -LiteralPath $releaseArchive).Length / 1MB, 2)
$sourceArchiveSize = [math]::Round((Get-Item -LiteralPath $sourceArchive).Length / 1MB, 2)
Write-Host ""
Write-Host "Release packages created successfully." -ForegroundColor Green
Write-Host "Ready-to-run: $releaseArchive ($archiveSize MB)"
Write-Host "Source code:  $sourceArchive ($sourceArchiveSize MB)"
