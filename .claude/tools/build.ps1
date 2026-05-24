<#
.SYNOPSIS
    PlatformIO build/upload/monitor helper for Radiowecker-AI-2.0

.DESCRIPTION
    Wraps pio.exe with the correct full path, handles COM-port kill before USB
    upload, and exits with pio's exit code so the caller can detect failures.

    The script can be launched even when the system-wide PowerShell execution
    policy is "Restricted", by invoking it as:
        powershell -ExecutionPolicy Bypass -File .\.claude\tools\build.ps1 ...

.PARAMETER Target
    PIO target: run (default), upload, uploadfs, monitor, clean

.PARAMETER Environment
    PIO environment: matouch43 (default, USB), matouch43_ota (OTA)
    (Renamed from -Env to avoid shadowing the built-in $Env: provider.)

.PARAMETER LogFile
    Optional path to also write pio output to (in addition to the console).
    Useful for scripted/CI use and for grep-friendly inspection afterwards.
    Example: -LogFile build_out.txt

.EXAMPLE
    # Build only (output to console)
    .\.claude\tools\build.ps1

.EXAMPLE
    # Build, also tee to build_out.txt
    .\.claude\tools\build.ps1 -LogFile build_out.txt

.EXAMPLE
    # USB upload (kills running monitor first)
    .\.claude\tools\build.ps1 -Target upload

.EXAMPLE
    # OTA upload to radiowecker2.local
    .\.claude\tools\build.ps1 -Target upload -Environment matouch43_ota

.EXAMPLE
    # Serial monitor
    .\.claude\tools\build.ps1 -Target monitor
#>
param(
    [ValidateSet("run","upload","uploadfs","monitor","clean")]
    [string]$Target = "run",

    [ValidateSet("matouch43","matouch43_ota")]
    [string]$Environment = "matouch43",

    [string]$LogFile = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Locate pio.exe ────────────────────────────────────────────────────────────
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
if (-not (Test-Path $pio)) {
    Write-Error "pio.exe not found at: $pio"
    exit 1
}

# ── Project root = two levels up from this script ────────────────────────────
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root

try {
    # ── Kill monitor before USB upload (frees COM port) ───────────────────────
    # OTA uploads do not touch the COM port, so skip the kill there.
    if ($Target -eq "upload" -and $Environment -eq "matouch43") {
        Write-Host "[build.ps1] Killing any running pio monitor..." -ForegroundColor DarkYellow
        try {
            & taskkill /F /IM pio.exe /T *> $null
        } catch { }
        Start-Sleep -Milliseconds 500
    }

    # ── Build the pio argument list ───────────────────────────────────────────
    $pioArgs = if ($Target -eq "run") {
        @("run", "-e", $Environment)
    } else {
        @("run", "-e", $Environment, "-t", $Target)
    }

    Write-Host "[build.ps1] env=$Environment target=$Target" -ForegroundColor Cyan
    Write-Host "[build.ps1] $pio $($pioArgs -join ' ')" -ForegroundColor Cyan
    if ($LogFile) {
        Write-Host "[build.ps1] logging output to: $LogFile" -ForegroundColor DarkCyan
    }

    # ── Run pio, optionally teeing stdout+stderr to a log file ────────────────
    if ($LogFile) {
        # 2>&1 merges stderr into the success stream so we capture both.
        # Manual tee (not Tee-Object) so we can force UTF-8 — Tee-Object in
        # PowerShell 5.1 has no -Encoding parameter and writes UTF-16, which
        # breaks downstream grep/regex tooling.
        if (Test-Path $LogFile) { Remove-Item -Force $LogFile }
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        $sw = [System.IO.StreamWriter]::new((Resolve-Path -LiteralPath (Split-Path $LogFile -Parent | ForEach-Object { if ($_) { $_ } else { '.' } })).Path + '\' + (Split-Path $LogFile -Leaf), $false, $utf8NoBom)
        try {
            & $pio @pioArgs 2>&1 | ForEach-Object {
                $line = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.ToString() } else { [string]$_ }
                Write-Host $line
                $sw.WriteLine($line)
            }
        } finally {
            $sw.Flush(); $sw.Dispose()
        }
    } else {
        & $pio @pioArgs
    }
    $exitCode = $LASTEXITCODE

    # ── Summary ───────────────────────────────────────────────────────────────
    if ($exitCode -eq 0) {
        Write-Host "[build.ps1] SUCCESS (exit 0)" -ForegroundColor Green
    } else {
        Write-Host "[build.ps1] FAILED (exit $exitCode)" -ForegroundColor Red
    }

    exit $exitCode
}
finally {
    Pop-Location
}
