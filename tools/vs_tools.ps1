# ============================================================================
# tools/vs_tools.ps1 -- locate Visual Studio / MSBuild without absolute paths.
#
# ASCII-ONLY ON PURPOSE: see the note at the top of ensure_bom.ps1.
#
# Resolution order:
#   1. -VsRoot (optional user override)
#   2. vswhere.exe (ships with the VS Installer)
#   3. %VSINSTALLDIR%
#
# Dot-source this file from other tools:
#   . (Join-Path $PSScriptRoot 'vs_tools.ps1')
# ============================================================================

function Resolve-VsRoot {
    param([string]$VsRoot)

    if ($VsRoot) {
        if (-not (Test-Path -LiteralPath $VsRoot)) { throw "VsRoot not found: $VsRoot" }
        return $VsRoot.TrimEnd('\')
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($found) {
            $root = @($found)[0]
            if ($root) { return $root.Trim().TrimEnd('\') }
        }
    }

    if ($env:VSINSTALLDIR) { return $env:VSINSTALLDIR.TrimEnd('\') }

    throw 'Visual Studio with the C++ toolset was not found. Install the MSVC workload or pass -VsRoot.'
}

function Resolve-MSBuild {
    param([string]$VsRoot)
    $root = Resolve-VsRoot $VsRoot
    $msbuild = Join-Path $root 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found: $msbuild" }
    return $msbuild
}

function Resolve-VcVars {
    param([string]$VsRoot, [string]$Platform = 'x64')
    $root = Resolve-VsRoot $VsRoot
    $name = if ($Platform -eq 'x86' -or $Platform -eq 'Win32') { 'vcvars32.bat' } else { 'vcvars64.bat' }
    $vcvars = Join-Path $root "VC\Auxiliary\Build\$name"
    if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvars not found: $vcvars" }
    return $vcvars
}
