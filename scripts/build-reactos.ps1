[CmdletBinding()]
param(
    [string]$ReactOSSourceDir = (Join-Path $PSScriptRoot '..\..\reactos'),
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build\reactos-i386'),
    [string]$RosBEDir = 'C:\RosBE',
    [ValidateSet('i386', 'amd64')]
    [string]$Architecture = 'i386',
    [switch]$InjectTestXrun,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'

$ReactOSSourceDir = (Resolve-Path $ReactOSSourceDir).Path
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$RosBEScript = Join-Path $RosBEDir 'RosBE.ps1'
$CMakeExe = Join-Path $RosBEDir 'bin\cmake.exe'
$ToolchainFile = Join-Path $ReactOSSourceDir 'toolchain-gcc.cmake'
$InjectionFile = Join-Path $RepoRoot 'cmake\InjectReactosDriver.cmake'
$XrunInjectionValue = if ($InjectTestXrun) { 'ON' } else { 'OFF' }

foreach ($RequiredPath in @($RosBEScript, $CMakeExe, $ToolchainFile, $InjectionFile)) {
    if (-not (Test-Path $RequiredPath)) {
        throw "Required file not found: $RequiredPath"
    }
}

& $RosBEScript $Architecture
if ($env:ROS_ARCH -ne $Architecture) {
    throw "RosBE did not select the requested architecture ($Architecture)."
}

$ConfigureArgs = @(
    '-S', $ReactOSSourceDir,
    '-B', $BuildDir,
    '-G', 'Ninja',
    "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=$ToolchainFile",
    "-DCMAKE_PROJECT_INCLUDE:FILEPATH=$InjectionFile",
    "-DVIRTIO_SND_SOURCE_DIR:PATH=$RepoRoot",
    "-DVIRTIO_SND_TEST_XRUN_INJECTION:BOOL=$XrunInjectionValue",
    "-DARCH:STRING=$Architecture",
    '-DENABLE_CCACHE:BOOL=OFF'
)

& $CMakeExe @ConfigureArgs
if ($LASTEXITCODE -ne 0) {
    throw "ReactOS CMake configuration failed with exit code $LASTEXITCODE."
}

if (-not $ConfigureOnly) {
    & $CMakeExe --build $BuildDir --target virtiosnd --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "virtiosnd build failed with exit code $LASTEXITCODE."
    }
}
