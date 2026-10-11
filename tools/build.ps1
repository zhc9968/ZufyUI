# ============================================================================
# tools/build.ps1 -- one-shot ZufyUI build (BOM guard runs first).
#
# ASCII-ONLY ON PURPOSE: see the note at the top of ensure_bom.ps1. A build
# entry point that cannot be parsed is useless exactly when it is needed, and
# PowerShell 5.1 misparses a BOM-less script containing non-ASCII.
#
# USAGE
#   powershell -ExecutionPolicy Bypass -File tools/build.ps1
#   ... -Configuration Debug        build Debug|x64
#   ... -SkipDebugger               do not build the companion ZufyUIDebugger
#   ... -SkipTests                  do not run the DSL self-test
#   ... -SkipBomFix                 skip the BOM step
#   ... -CheckBomOnly               only verify BOMs (no writes); exit 1 if missing
#
# PIPELINE
#   1. BOM guard (fix missing UTF-8 BOMs)
#   2. kill processes holding the output files
#   3. build ZufyUI (the demo / smoke test)
#   4. build ZufyUIDebugger (library changes must be verified against it too)
#   5. run the DSL self-test
#
# Exit code: 0 = everything passed; non-zero = the failing step's code.
# ============================================================================
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')] [string]$Configuration = 'Release',
    [string]$Platform = 'x64',

    # Must match the MSBuild path used in AGENTS.md
    [string]$VsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community',

    [string]$DebuggerProject = 'C:\project\ZufyUIDebugger\ZufyUIDebugger.vcxproj',

    [switch]$SkipBomFix,
    [switch]$CheckBomOnly,
    [switch]$SkipDebugger,
    [switch]$SkipTests,

    # Processes killed before building (open file handles break the link step)
    [string[]]$KillProcesses = @('ZufyUI', 'ZufyUIDebugger')
)

$ErrorActionPreference = 'Stop'

# The DSL test writes UTF-8; PowerShell 5.1 would otherwise decode its output as ANSI.
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

$root = Split-Path -Parent $PSScriptRoot
$msbuild = Join-Path $VsRoot 'MSBuild\Current\Bin\MSBuild.exe'
$project = Join-Path $root 'ZufyUI.vcxproj'

function Step([string]$text) { Write-Host "`n=== $text ===" -ForegroundColor Cyan }
function Ok([string]$text) { Write-Host $text -ForegroundColor Green }
function Bad([string]$text) { Write-Host $text -ForegroundColor Red }

# ---------- 1. BOM guard ----------
if ($CheckBomOnly) {
    & (Join-Path $PSScriptRoot 'ensure_bom.ps1') -Root $root -Check
    exit $LASTEXITCODE
}

if (-not $SkipBomFix) {
    Step '1/4 ensure UTF-8 BOM'
    & (Join-Path $PSScriptRoot 'ensure_bom.ps1') -Root $root
    if ($LASTEXITCODE -ne 0) { Bad 'BOM step failed.'; exit $LASTEXITCODE }
}
else {
    Step '1/4 BOM check skipped (-SkipBomFix)'
}

if (-not (Test-Path -LiteralPath $msbuild)) {
    Bad "MSBuild not found: $msbuild`nPass -VsRoot to point at your Visual Studio install."
    exit 2
}
if (-not (Test-Path -LiteralPath $project)) { Bad "Project not found: $project"; exit 2 }

# ---------- 2. kill holders ----------
Step '2/4 stop processes holding output files'
foreach ($name in $KillProcesses) {
    $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
    if ($procs) { $procs | Stop-Process -Force; Write-Host "  stopped $name ($($procs.Count))" }
}
Write-Host '  done (silent when nothing was running)'

$msbArgs = @(
    "/p:Configuration=$Configuration", "/p:Platform=$Platform",
    '/p:DebugSymbols=false', '/p:DebugType=None',
    '/m', '/nologo', '/v:minimal'
)

# ---------- 3. build ----------
Step "3/4 build ZufyUI ($Configuration|$Platform)"
& $msbuild $project @msbArgs
if ($LASTEXITCODE -ne 0) { Bad "ZufyUI build failed (exit $LASTEXITCODE)."; exit $LASTEXITCODE }
Ok "  -> $(Join-Path $root "x64\$Configuration\ZufyUI.exe")"

if (-not $SkipDebugger) {
    if (Test-Path -LiteralPath $DebuggerProject) {
        Step "3b/4 build ZufyUIDebugger ($Configuration|$Platform) -- verify library changes here too"
        & $msbuild $DebuggerProject @msbArgs
        if ($LASTEXITCODE -ne 0) { Bad "ZufyUIDebugger build failed (exit $LASTEXITCODE)."; exit $LASTEXITCODE }
        $dbgExe = Join-Path (Split-Path -Parent $DebuggerProject) "x64\$Configuration\ZufyUIDebugger.exe"
        Ok "  -> $dbgExe"
    }
    else {
        Write-Host "  skipped: $DebuggerProject not found" -ForegroundColor Yellow
    }
}

# ---------- 4. self-test ----------
$testExit = 0
if (-not $SkipTests) {
    Step '4/4 DSL self-test'
    & (Join-Path $PSScriptRoot 'run_dsl_tests.ps1') -VsRoot $VsRoot
    $testExit = $LASTEXITCODE
    if ($testExit -ne 0) { Bad "DSL self-test failed (exit $testExit)." }
}
else {
    Step '4/4 DSL self-test skipped (-SkipTests)'
}

if ($testExit -ne 0) { exit $testExit }
Ok "`nAll steps passed: build + self-test."
exit 0
