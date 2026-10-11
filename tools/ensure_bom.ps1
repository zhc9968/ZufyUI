# ============================================================================
# tools/ensure_bom.ps1 -- guarantee a UTF-8 BOM on source files that need one.
#
# WHY THIS EXISTS
#   A .h/.cpp containing non-ASCII text (e.g. Chinese comments) WITHOUT a BOM is
#   decoded by MSVC using the system ANSI code page (GBK on a zh-CN machine),
#   which yields bogus errors such as C2143 / C2259 / C2888 / C4819 / C2001.
#   Editors and AI tools frequently strip the BOM without telling anyone, so run
#   this before every build.
#
# WHY THIS FILE IS ASCII-ONLY (deliberate -- do not add Chinese here)
#   A BOM guard must not itself depend on a BOM. PowerShell 5.1 reads a BOM-less
#   .ps1 as ANSI, so a guard containing Chinese would fail to parse in exactly
#   the situation it exists to repair -- and then it could not repair itself.
#   Keeping this script ASCII makes the repair path immune to that failure.
#   The Chinese explanation for humans lives in HANDOFF.md instead.
#
# BEHAVIOUR
#   * A file receives a BOM only if it BOTH lacks one AND contains non-ASCII.
#   * Pure-ASCII files are left untouched (a BOM would only create diff noise);
#     pass -All to force a BOM on every scanned file.
#   * Files that do not decode as valid UTF-8 are skipped with a warning:
#     adding a BOM to a genuinely GBK-encoded file would only make it worse.
#   * Line endings are never modified. Already-correct files are not rewritten.
#
# USAGE
#   powershell -ExecutionPolicy Bypass -File tools/ensure_bom.ps1          # fix in place
#   powershell -ExecutionPolicy Bypass -File tools/ensure_bom.ps1 -Check   # check only; exit 1 if any missing
# ============================================================================
[CmdletBinding()]
param(
    # Repository root. Empty = parent directory of this script.
    # NOTE: $PSScriptRoot is NOT usable in a param() default on PowerShell 5.1
    # (it evaluates to an empty string), hence the empty default + fix-up below.
    [string]$Root = '',

    # Check only; never modify. Exit code 1 when something needs a BOM (CI use).
    [switch]$Check,

    # Also add a BOM to pure-ASCII files (off by default).
    [switch]$All,

    # Extensions that should carry a BOM.
    #   .ps1 is included because PowerShell 5.1 misreads a BOM-less script that
    #   contains non-ASCII, producing fake syntax errors.
    [string[]]$Include = @('*.h', '*.hpp', '*.cpp', '*.cxx', '*.ps1'),

    # Directory names to skip entirely (build output, editor caches, site copy).
    [string[]]$ExcludeDir = @('x64', '.vs', '.git', 'ghpages', 'Debug', 'Release')
)

$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$utf8Bom = New-Object System.Text.UTF8Encoding($true)             # emits BOM
$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)  # rejects invalid bytes

# ---- collect candidate files ----
$files = @()
foreach ($pat in $Include) {
    $files += Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $pat -ErrorAction SilentlyContinue
}
$files = $files | Where-Object {
    $rel = $_.FullName.Substring($Root.Length).TrimStart('\')
    $parts = $rel -split '\\'
    -not ($parts | Where-Object { $ExcludeDir -contains $_ })
} | Sort-Object FullName -Unique

if (-not $files) {
    Write-Host "ensure_bom: no source files found under $Root"
    exit 0
}

$needBom = @()     # has non-ASCII, lacks BOM  -> gets one (the failure mode we prevent)
$asciiOnly = @()   # pure ASCII, lacks BOM    -> left alone
$notUtf8 = @()     # not valid UTF-8          -> skipped with a warning

foreach ($f in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        continue   # already has a BOM
    }
    try { $text = $utf8Strict.GetString($bytes) }
    catch { $notUtf8 += $f.FullName; continue }

    if (-not $All -and $text -notmatch '[^\x00-\x7F]') { $asciiOnly += $f.FullName; continue }

    $needBom += $f.FullName
    if (-not $Check) {
        [System.IO.File]::WriteAllText($f.FullName, $text, $utf8Bom)
    }
}

# ---- report ----
$relPath = { param($p) $p.Substring($Root.Length).TrimStart('\') }

foreach ($p in $notUtf8) {
    Write-Host ("[skip] not valid UTF-8, no BOM added (convert it first): " + (& $relPath $p)) -ForegroundColor Yellow
}

if ($needBom.Count -eq 0) {
    $extra = if ($asciiOnly.Count) { " ($($asciiOnly.Count) pure-ASCII file(s) need no BOM)" } else { '' }
    Write-Host ("ensure_bom: scanned $($files.Count) file(s); every file with non-ASCII already has a BOM.$extra") -ForegroundColor Green
    exit 0
}

if ($Check) {
    Write-Host ("ensure_bom: $($needBom.Count) file(s) with non-ASCII are missing a BOM:") -ForegroundColor Red
    foreach ($p in $needBom) { Write-Host ("  " + (& $relPath $p)) }
    exit 1
}

Write-Host ("ensure_bom: added a BOM to $($needBom.Count) of $($files.Count) scanned file(s):") -ForegroundColor Cyan
foreach ($p in $needBom) { Write-Host ("  + " + (& $relPath $p)) }
exit 0
