# ============================================================================
# tools/run_tests.ps1 -- compile and run the ZufyUI self-test.
#
# ASCII-ONLY ON PURPOSE: see the note at the top of ensure_bom.ps1.
#
# USAGE
#   powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1
#
# Exit code: 0 = all assertions passed; non-zero = compile or assertion failure.
# The test creates no visible window (one hidden dispatcher for UIThread tests).
# ============================================================================
[CmdletBinding()]
param(
    # Optional: point at a Visual Studio install. Auto-detected via vswhere when omitted.
    [string]$VsRoot = '',
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

# The test prints UTF-8 Chinese; PowerShell 5.1 would otherwise decode it as ANSI.
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'tests\zufyui_tests.cpp'
$outDir = Join-Path $env:TEMP 'zufyui_tests'
$exe = Join-Path $outDir 'zufyui_tests.exe'

if (-not (Test-Path -LiteralPath $src)) { throw "Test source not found: $src" }

. (Join-Path $PSScriptRoot 'vs_tools.ps1')
$vcvars = Resolve-VcVars $VsRoot $Platform

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Get-ChildItem -LiteralPath $outDir -Filter '*.obj' -ErrorAction SilentlyContinue | Remove-Item -Force

# Mirrors ZufyUI.vcxproj: C++20 / ConformanceMode / Unicode character set.
#   /utf-8 makes the EXECUTION character set UTF-8 too; without it MSVC encodes
#   narrow literals using the system ANSI code page and the Chinese messages
#   come out as mojibake in a UTF-8 terminal.
$clArgs = @(
    '/nologo', '/std:c++20', '/EHsc', '/bigobj', '/W3', '/permissive-', '/utf-8',
    '/DUNICODE', '/D_UNICODE', '/DWIN32', '/D_WINDOWS', '/DNDEBUG',
    "/I`"$root`"",
    "/Fo`"$outDir\\`"",
    "/Fe`"$exe`"",
    "`"$src`"",
    # cl does not add the default system libs the way an MSBuild project does.
    # (d2d1/dwrite/dwmapi/imm32/winmm/d3d11/dxgi/dcomp/dxguid/windowsapp come from
    #  the library's own #pragma comment(lib, ...).)
    'user32.lib', 'gdi32.lib', 'ole32.lib', 'oleaut32.lib', 'shell32.lib',
    'shlwapi.lib', 'advapi32.lib', 'comdlg32.lib', 'uiautomationcore.lib'
) -join ' '

Write-Host "[1/2] compile $src" -ForegroundColor Cyan
cmd /c "`"$vcvars`" >nul 2>&1 && cl $clArgs"
if ($LASTEXITCODE -ne 0) {
    Write-Host "compile failed (exit $LASTEXITCODE)" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "[2/2] run $exe" -ForegroundColor Cyan
& $exe
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host 'ZufyUI self-test passed.' -ForegroundColor Green }
else { Write-Host "ZufyUI self-test failed (exit $code)." -ForegroundColor Red }
exit $code
