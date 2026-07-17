<#
.SYNOPSIS
  Builds libcps.so for one or more Android ABIs and lays them out as jniLibs/<abi>/.

.DESCRIPTION
  For each ABI this script configures a separate CMake build directory, builds the
  shared library (with the embedded classes.dex), and copies libcps.so into
  <OutDir>/jniLibs/<abi>/. The public C-ABI headers are copied to <OutDir>/include/.

  NDK / SDK / JDK are auto-detected from environment variables and common install
  locations; override with the parameters below.

.PARAMETER Abis
  Space-separated list of ABIs. Default: arm64-v8a armeabi-v7a x86 x86_64

.PARAMETER MinApi
  Android platform level (matches CPS_ANDROID_MIN_API). Default: android-21

.PARAMETER OutDir
  Output root. Default: dist/android (relative to the repo root)

.PARAMETER Clean
  Rebuild from scratch (remove the per-ABI build directories first).

.EXAMPLE
  .\scripts\build-android.ps1
  .\scripts\build-android.ps1 -Abis arm64-v8a,armeabi-v7a
#>
[CmdletBinding()]
param(
    [string[]] $Abis = @('arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'),
    [string]   $MinApi = 'android-21',
    [string]   $OutDir,
    [string]   $SdkRoot,
    [string]   $NdkRoot,
    [string]   $JavaHome,
    [string]   $BuildType = 'Release',
    [switch]   $Clean
)

$ErrorActionPreference = 'Stop'

# Repo root = parent of the scripts/ directory.
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

if (-not $OutDir)   { $OutDir   = Join-Path $RepoRoot 'dist\android' }
if (-not $SdkRoot)  { $SdkRoot  = $env:ANDROID_SDK_ROOT; if (-not $SdkRoot) { $SdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' } }
if (-not $NdkRoot)  {
    $NdkRoot = $env:ANDROID_NDK_ROOT; if (-not $NdkRoot) { $NdkRoot = $env:ANDROID_NDK }
    if (-not $NdkRoot -or -not (Test-Path $NdkRoot)) {
        $ndkBase = Join-Path $SdkRoot 'ndk'
        if (Test-Path $ndkBase) {
            $NdkRoot = (Get-ChildItem $ndkBase -Directory | Sort-Object Name | Select-Object -Last 1).FullName
        }
    }
}
if (-not $JavaHome) {
    $JavaHome = $env:JAVA_HOME
    if (-not $JavaHome) {
        $javac = (Get-Command javac -ErrorAction SilentlyContinue).Source
        if ($javac) { $JavaHome = (Resolve-Path (Join-Path (Split-Path $javac) '..')).Path }
    }
}

Write-Host "Repo     : $RepoRoot"
Write-Host "SDK       : $SdkRoot"
Write-Host "NDK       : $NdkRoot"
Write-Host "JAVA_HOME : $JavaHome"
Write-Host "Output    : $OutDir"
Write-Host "ABIs      : $($Abis -join ', ')"
Write-Host ""

foreach ($v in @('SdkRoot', 'NdkRoot', 'JavaHome')) {
    $p = (Get-Variable $v).Value
    if (-not $p -or -not (Test-Path $p)) { throw "Path not found for $v : '$p'. Set ANDROID_SDK_ROOT / NDK / JAVA_HOME or pass the parameter." }
}

$Toolchain = Join-Path $NdkRoot 'build\cmake\android.toolchain.cmake'
if (-not (Test-Path $Toolchain)) { throw "Android toolchain not found: $Toolchain" }

# Pick a generator + make program available on this host.
$Generator = 'MinGW Makefiles'
if (-not (Get-Command mingw32-make -ErrorAction SilentlyContinue) -and (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $Generator = 'Ninja'
}

# Propagate to the CMake dex logic (javac/d8/android.jar discovery).
$env:ANDROID_SDK_ROOT = $SdkRoot
$env:ANDROID_NDK      = $NdkRoot
if ($JavaHome) { $env:JAVA_HOME = $JavaHome }

$BuildRoot  = Join-Path $RepoRoot 'build-android'
$JniLibsOut = Join-Path $OutDir 'jniLibs'
$IncludeOut = Join-Path $OutDir 'include'

if ($Clean -and (Test-Path $BuildRoot)) { Remove-Item $BuildRoot -Recurse -Force }

$failed = @()
foreach ($abi in $Abis) {
    $abiBuild = Join-Path $BuildRoot $abi
    Write-Host "=== $abi ===" -ForegroundColor Cyan

    & cmake -S $RepoRoot -B $abiBuild -G $Generator `
        "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
        "-DANDROID_ABI=$abi" `
        "-DANDROID_PLATFORM=$MinApi" `
        "-DCMAKE_BUILD_TYPE=$BuildType" `
        "-DANDROID_SDK_ROOT=$SdkRoot" `
        "-DCPS_BUILD_EXAMPLES=OFF" | Out-Null
    if ($LASTEXITCODE -ne 0) { $failed += $abi; Write-Host "  CONFIGURE FAILED for $abi" -ForegroundColor Red; continue }

    & cmake --build $abiBuild -j
    if ($LASTEXITCODE -ne 0) { $failed += $abi; Write-Host "  BUILD FAILED for $abi" -ForegroundColor Red; continue }

    $so = Join-Path $abiBuild 'libcps.so'
    if (-not (Test-Path $so)) { $failed += $abi; Write-Host "  libcps.so not produced for $abi" -ForegroundColor Red; continue }

    $dest = Join-Path $JniLibsOut $abi
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    Copy-Item $so (Join-Path $dest 'libcps.so') -Force
    Write-Host "  -> $dest\libcps.so" -ForegroundColor Green
}

# Headers (once).
New-Item -ItemType Directory -Force -Path $IncludeOut | Out-Null
Copy-Item (Join-Path $RepoRoot 'include\cps') $IncludeOut -Recurse -Force

Write-Host ""
Write-Host "Done. Output layout:" -ForegroundColor Green
Write-Host "  $OutDir\jniLibs\<abi>\libcps.so"
Write-Host "  $OutDir\include\cps\*.h,*.hpp"
if ($failed.Count -gt 0) {
    Write-Host ("Failed ABIs: {0}" -f ($failed -join ', ')) -ForegroundColor Red
    exit 1
}
