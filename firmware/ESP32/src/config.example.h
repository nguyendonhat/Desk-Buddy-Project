#pragma once

// Copy this file to config.h and enter local values. config.h is git-ignored.
#define WIFI_SSID "replace-with-wifi-name"
#define WIFI_PASSWORD "replace-with-wifi-password"
#define API_BASE_URL "https://your-worker.example.workers.dev"
#define DEVICE_ID "DB001"
#define DEVICE_API_TOKEN "replace-with-the-matching-worker-device-token"

// Paste the PEM root CA that validates API_BASE_URL. Keep HTTPS certificate
// validation enabled; do not replace it with setInsecure().
static const char API_ROOT_CA_PEM[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
replace-with-the-root-ca-pem
-----END CERTIFICATE-----
)CERT";

// Vietnam time is UTC+7. Set this to the calendar's local offset if changed.
constexpr long LOCAL_UTC_OFFSET_SECONDS = 7 * 60 * 60;
