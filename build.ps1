# TWIN-MU build helper (Windows): configures with Ninja inside the MSVC x64 environment,
# builds Release and runs the unit tests.
# Ninja is used instead of the Visual Studio generator because MSBuild's FileTracker fails
# on very long folder paths.
param(
    [string]$Config = "Release",
    [switch]$Plugin,
    [switch]$NoTests
)
$ErrorActionPreference = "Stop"

$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio C++ tools not found" }

Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
$env:Path = (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja") + ';' + $env:Path

$root = $PSScriptRoot
# Build tree in a short path: object-file paths must stay under 250 characters.
$build = Join-Path $env:LOCALAPPDATA "TwinMu\build-$($Config.ToLower())"
$pluginFlag = if ($Plugin) { "ON" } else { "OFF" }

$ErrorActionPreference = "Continue" # native tools write warnings to stderr
cmake -S "$root" -B "$build" -G Ninja "-DCMAKE_BUILD_TYPE=$Config" "-DTWINMU_BUILD_PLUGIN=$pluginFlag"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $NoTests) {
    & (Join-Path $build "twinmu_tests.exe")
    exit $LASTEXITCODE
}
