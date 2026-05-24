<#
.SYNOPSIS
    PlatformIO build/upload/monitor helper for Radiowecker-AI-2.0

.DESCRIPTION
    Wraps pio.exe with the correct full path, handles COM-port kill before USB
    upload, and exits with pio's exit code so the caller can detect failures.

.PARAMETER Target
    PIO target: run (default), upload, uploadfs, monitor, clean

.PARAMETER Env
    PIO environment: matouch43 (default, USB), matouch43_ota (OTA)

.EXAMPLE
    # Build only
    .\.claude\tools\build.ps1

.EXAMPLE
    # USB upload (kills running monitor first)
    .\.claude\tools\build.ps1 -Target upload

.EXAMPLE
    # OTA upload
    .\.claude\tools\build.ps1 -Target upload -Env matouch43_ota

.EXAMPLE
    # Serial monitor
    .\.claude\tools\build.ps1 -Target monitor
#>
param(
    [ValidateSet("run","upload","uploadfs","monitor","clean")]
    [string]$Target = "run",

    [ValidateSet("matouch43","matouch43_ota")]
    [string]$Env = "matouch43"
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
    if ($Target -eq "upload" -and $Env -eq "matouch43") {
        Write-Host "[build.ps1] Killing any running pio monitor..." -ForegroundColor DarkYellow
        try { & taskkill /F /IM pio.exe /T 2>$null | Out-Null } catch {}
        Start-Sleep -Milliseconds 500
    }

    # ── Build the pio argument list ───────────────────────────────────────────
    $pioArgs = if ($Target -eq "run") {
        @("run", "-e", $Env)
    } else {
        @("run", "-e", $Env, "-t", $Target)
    }

    Write-Host "[build.ps1] $pio $($pioArgs -join ' ')" -ForegroundColor Cyan
    & $pio @pioArgs
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
