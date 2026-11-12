param(
  [switch]$BuildOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$configPath = Join-Path $projectRoot 'src\config.h'
$examplePath = Join-Path $projectRoot 'src\config.example.h'

function Read-RequiredValue([string]$Prompt, [string]$Default = '') {
  if ($Default) { $answer = Read-Host "$Prompt [$Default]" } else { $answer = Read-Host $Prompt }
  if ([string]::IsNullOrWhiteSpace($answer)) { $answer = $Default }
  if ([string]::IsNullOrWhiteSpace($answer)) { throw "$Prompt is required." }
  return $answer.Trim()
}

function ConvertTo-CppString([string]$Value) {
  return $Value.Replace('\', '\\').Replace('"', '\"')
}

$pioCommand = Get-Command pio -ErrorAction SilentlyContinue
if (-not $pioCommand) {
  $platformioExe = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
  if (Test-Path -LiteralPath $platformioExe) { $pioCommand = $platformioExe }
}
if (-not $pioCommand) {
  throw 'PlatformIO CLI was not found. Install PlatformIO or open a PlatformIO terminal, then run this script again.'
}

if (-not (Test-Path -LiteralPath $configPath)) {
  if ($BuildOnly) {
    Copy-Item -LiteralPath $examplePath -Destination $configPath
    Write-Host 'Created src\config.h from the example. It still contains placeholders; live Calendar fetch will not work until you configure it.'
  }
  if (-not $BuildOnly) {
    Write-Host 'Creating a local src\config.h. These values stay on this computer and are ignored by Git.'
  $wifiSsid = Read-RequiredValue 'Wi-Fi name (SSID)'
  $wifiPassword = Read-RequiredValue 'Wi-Fi password'
  $apiBaseUrl = Read-RequiredValue 'Worker base URL, for example https://example.workers.dev'
  $webAppUrl = Read-RequiredValue 'Web app URL, for example https://example.pages.dev'
  $deviceId = Read-RequiredValue 'Device ID' 'DB001'
  $deviceToken = Read-RequiredValue 'Device API token for this device ID'
  $caPath = Read-RequiredValue 'Path to PEM CA certificate file'
  if (-not (Test-Path -LiteralPath $caPath -PathType Leaf)) { throw "CA certificate file not found: $caPath" }
  $caPem = (Get-Content -LiteralPath $caPath -Raw).Trim()
  if ($caPem -notmatch '-----BEGIN CERTIFICATE-----' -or $caPem -notmatch '-----END CERTIFICATE-----') {
    throw 'The CA file must contain a PEM certificate with BEGIN/END CERTIFICATE lines.'
  }

  $configText = @"
#pragma once
#include <pgmspace.h>
#define WIFI_SSID "$(ConvertTo-CppString $wifiSsid)"
#define WIFI_PASSWORD "$(ConvertTo-CppString $wifiPassword)"
#define API_BASE_URL "$(ConvertTo-CppString $apiBaseUrl)"
#define WEB_APP_URL "$(ConvertTo-CppString $webAppUrl)"
#define DEVICE_ID "$(ConvertTo-CppString $deviceId)"
#define DEVICE_API_TOKEN "$(ConvertTo-CppString $deviceToken)"
static const char API_ROOT_CA_PEM[] PROGMEM = R"CERT(
$caPem
)CERT";
constexpr long LOCAL_UTC_OFFSET_SECONDS = 7 * 60 * 60;
"@
  Set-Content -LiteralPath $configPath -Value $configText -Encoding utf8
  Write-Host 'Created src\config.h. It is ignored by Git.'
  }
}

Push-Location $projectRoot
try {
  & $pioCommand run -e nodemcu-32s
  if ($LASTEXITCODE -ne 0) { throw "Firmware build failed with exit code $LASTEXITCODE." }
} finally {
  Pop-Location
}

Write-Host 'Build passed. This only compiles the firmware; it does not upload to a board.'
