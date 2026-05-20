#pragma once

// Copy this file to config.h and edit only config.h.
// Do not commit real Wi-Fi credentials.

// Wi-Fi and NTP settings.
// WIFI_ENABLED: false disables background Wi-Fi/NTP sync entirely.
#define WIFI_ENABLED true
#define WIFI_SSID "Home-IoT-2G"
#define WIFI_PASSWORD "71093734"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
#define NTP_SERVER_3 "time.cloudflare.com"
#define GMT_OFFSET_SECONDS 32400
#define DAYLIGHT_OFFSET_SECONDS 0

// Timer defaults (minutes).
// PRESET_MINUTES are mapped to keys 1..5 in order.
#define TIMER_DEFAULT_MINUTES 25
#define TIMER_MIN_MINUTES 1
#define TIMER_MAX_MINUTES 99
#define TIMER_PRESET_1_MINUTES 5
#define TIMER_PRESET_2_MINUTES 10
#define TIMER_PRESET_3_MINUTES 15
#define TIMER_PRESET_4_MINUTES 25
#define TIMER_PRESET_5_MINUTES 45

// Retry/timeouts (milliseconds).
#define WIFI_RETRY_INTERVAL_MS 30000
#define NTP_RETRY_INTERVAL_MS 5000
#define NTP_RECONFIG_INTERVAL_MS 30000
#define CUSTOM_INPUT_TIMEOUT_MS 7000
#define DONE_RETURN_MS 3000

// Display brightness and idle dimming.
// Brightness range depends on device firmware; keep 0-255.
#define BRIGHTNESS_NORMAL 96
#define BRIGHTNESS_DIM 40
#define BRIGHTNESS_DEEP_DIM 12
#define DIM_AFTER_MS 30000
#define DEEP_DIM_AFTER_MS 120000

// Battery display and warning behavior.
#define LOW_BATTERY_THRESHOLD_PERCENT 20
#define LOW_BATTERY_RELEASE_PERCENT 23
#define BATTERY_DISPLAY_HYSTERESIS_PERCENT 2

// Audio defaults.
#define SPEAKER_DEFAULT_VOLUME 80
#define SPEAKER_LOUD_VOLUME 140

// Voice memo recording/playback settings.
#define VOICE_SAMPLE_RATE 8000
#define VOICE_RECORD_SAMPLES 256
#define VOICE_PLAYBACK_SAMPLES 512
#define VOICE_PLAYBACK_BUFFERS 3
#define VOICE_MEMO_MAX_ENTRIES 50
#define VOICE_PLAYBACK_CHANNEL 0
