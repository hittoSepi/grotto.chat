param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $repoRoot $BuildDir
}

if (-not (Test-Path $resolvedBuildDir)) {
    throw "Build directory not found: $resolvedBuildDir"
}

if (-not $SkipBuild) {
    Write-Host "Building grotto-client..."
    cmake --build $resolvedBuildDir --config $Config --target grotto-client
    if ($LASTEXITCODE -ne 0) {
        throw "grotto-client build failed"
    }

    Write-Host "Running check target..."
    cmake --build $resolvedBuildDir --config $Config --target check
    if ($LASTEXITCODE -ne 0) {
        throw "check target failed"
    }
}

$outputDir = Join-Path $resolvedBuildDir $Config
if (-not (Test-Path $outputDir)) {
    $outputDir = $resolvedBuildDir
}

$clientExe = Join-Path $outputDir "grotto-client.exe"
if (-not (Test-Path $clientExe)) {
    throw "Client binary not found: $clientExe"
}

$qaDir = Join-Path $resolvedBuildDir "qa-error-scenarios"
$null = New-Item -ItemType Directory -Force -Path $qaDir
$null = New-Item -ItemType Directory -Force -Path (Join-Path $qaDir "downloads")

$configTemplate = Join-Path $repoRoot "config\client.toml.example"
$qaConfig = Join-Path $qaDir "client.toml"
$configText = Get-Content $configTemplate -Raw
$configText = $configText -replace '(?m)^auto_reconnect\s*=.*$', 'auto_reconnect = true'
$configText = $configText -replace '(?m)^reconnect_delay_sec\s*=.*$', 'reconnect_delay_sec = 1'
$configText = $configText -replace '(?m)^enabled\s*=.*# Enable link previews$', 'enabled = false           # Enable link previews'
$configText = $configText -replace '(?m)^inline_images\s*=.*$', 'inline_images = false          # Keep QA runs deterministic'
Set-Content -Path $qaConfig -Value $configText -Encoding ascii

$serverExeCandidates = @(
    (Join-Path $repoRoot "..\grotto-chat-server\build\Release\grotto-chat-server.exe"),
    (Join-Path $repoRoot "..\grotto-chat-server\build-codex-check\Release\grotto-chat-server.exe"),
    (Join-Path $repoRoot "..\grotto-chat-server\build\grotto-chat-server.exe")
)
$serverExe = $serverExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

$checklistPath = Join-Path $qaDir "ERROR-QA-CHECKLIST.txt"
$lines = @(
    "Grotto Client Error Scenario QA",
    "",
    "Prepared config: $qaConfig",
    "Client binary:    $clientExe",
    ("Server binary:    " + ($(if ($serverExe) { $serverExe } else { "<not found automatically>" }))),
    "",
    "Recommended launch commands:",
    "  Client:",
    "    & '$clientExe' --config '$qaConfig'",
    "  Server:"
)

if ($serverExe) {
    $lines += "    & '$serverExe'"
} else {
    $lines += "    Build/start the server manually before running the client."
}

$lines += @(
    "",
    "Checklist:",
    "1. Start server, then start client with the QA config above and log in.",
    "2. Join a channel or DM, then stop the server process.",
    "3. Verify the client shows reconnecting state instead of freezing or exiting.",
    "4. While reconnecting, send a message and verify the status bar shows queued outbound work.",
    "5. Start the server again and verify the client re-authenticates and flushes the queued message.",
    "6. Trigger a DM session repair case if available and verify reconnect does not drop the repair request immediately.",
    "7. Join a voice room or start a direct call, then exit the client with /quit.",
    "8. Verify the client process exits promptly without hanging after voice shutdown.",
    "9. Start an upload or download, then exit the client with /quit.",
    "10. Verify the client exits promptly and the next launch starts cleanly without stuck transfers.",
    "11. Run the same exit test from the window close action and from Settings -> Logout.",
    "",
    "Reference doc: docs/error-scenario-qa.md"
)

Set-Content -Path $checklistPath -Value $lines -Encoding ascii

Write-Host ""
Write-Host "Prepared QA workspace: $qaDir"
Write-Host "Checklist written to:  $checklistPath"
Write-Host ""
$lines | ForEach-Object { Write-Host $_ }
