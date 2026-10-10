[CmdletBinding()]
param(
    [ValidateSet('windows-native', 'windows-minimal')][string]$Preset = 'windows-native',
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build "build/$Preset" --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build or automatic tests failed.' }
    Write-Host "Executable: $Root\build\$Preset\bin\$Configuration\quanta.exe"
} finally {
    Pop-Location
}
