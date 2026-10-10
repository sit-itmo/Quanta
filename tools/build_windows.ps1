[CmdletBinding()]
param(
    [switch]$Gui,
    [ValidateSet('x64','Win32')][string]$Architecture='x64',
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [string]$ImGuiDir='',
    [switch]$NoTests
)
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Flavor=if($Gui){'gui'}else{'cli'}
$Build=Join-Path $Root "out\$Architecture-$Flavor"
$GuiFlag=if($Gui){'ON'}else{'OFF'}
$TestsFlag=if($NoTests){'OFF'}else{'ON'}
$Arguments=@('-S',$Root,'-B',$Build,'-G','Visual Studio 17 2022','-A',$Architecture,
    "-DINV_WITH_GUI=$GuiFlag",'-DINV_STRICT_STATIC=ON',"-DINV_BUILD_TESTS=$TestsFlag")
if($ImGuiDir){$Arguments+=@("-DINV_IMGUI_DIR=$ImGuiDir",'-DINV_FETCH_IMGUI=OFF')}
& cmake @Arguments
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed.'}
& cmake --build $Build --config $Configuration --parallel
if($LASTEXITCODE -ne 0){throw 'Build failed.'}
if(-not $NoTests){
    & ctest --test-dir $Build -C $Configuration --output-on-failure
    if($LASTEXITCODE -ne 0){throw 'Tests failed; see CTest output.'}
}
$Exe=Join-Path $Build "$Configuration\inv.exe"
Write-Host "Executable: $Exe"
Write-Host "Inventory: & `"$Exe`" run --query-file `"$Root\queries\windows\01-inventory.invq`" --redact"
