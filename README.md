# CARDPUTER StudyTimer

A calm, low-friction study timer for the M5Stack Cardputer.

![CARDPUTER StudyTimer overview](images/overview.jpeg)

The timer is the primary feature. Logging is a passive side effect. The intended daily flow is:

```text
Power on
Previous timer appears
Press Enter
Start studying
```

## Concept

CARDPUTER StudyTimer is designed to feel closer to a simple 100-yen shop timer than a productivity app.

It is intentionally standalone:

- Cardputer only
- No extra UNIT modules
- No MQTT
- No Home Assistant
- No cloud features
- No gamification
- No subject-name input

Wi-Fi and NTP are optional background helpers. The timer remains usable even if Wi-Fi, NTP, or SD logging fails.

## Features

- Fast startup with the previous timer value shown immediately
- Presets: 5, 10, 15, 25, and 45 minutes
- Manual minute input from 1 to 99 minutes
- Large remaining-time display
- Four-edge segmented progress display
- Japanese and English UI
- Sound modes: Quiet, Normal, Loud
- Low-battery visual warning at 20% or less when not charging
- Passive CSV logging to microSD
- Background Wi-Fi/NTP time sync
- No startup wait for Wi-Fi or NTP

## Controls

### Ready

- `Enter`: start the previous timer
- `1` to `5`: start a preset
- `0`: enter a custom minute value
- `S`: open settings
- `L`: open stats
- `V`: open voice memos
- Hold `M`: record a voice memo from any screen

### Custom Input

- Number keys: enter minutes
- `Enter`: start
- `Del`: delete one digit, or return to Ready when empty

### Running / Paused

- `Enter`: pause or resume
- `Del`: open reset confirmation

### Settings

Press `S` to open Language, then use `Fn + /` and `Fn + ;` to switch between Language and Volume.

Language:

- `1`: Japanese
- `2`: English

Volume:

- `1`: Quiet
- `2`: Normal
- `3`: Loud

Controls:

- `Fn + ;`: move left
- `Fn + ,`: move down
- `Fn + .`: move up
- `Fn + /`: move right
- `Enter`: apply selected item
- `Del`: return to Ready

![Settings screen](images/settings.jpeg)

### Stats

Press `L` to show stats from `study_log.csv` on the SD card.

- today's total
- last 7 days total
- streak days
- simple 7-day bars

Synced devices use the exact NTP date. Before NTP sync, stats can still use a saved date as `OFFLINE DATE` when a previous sync exists. Stats still require an available SD card.

### Voice Memos

Hold `M` to record a voice memo from the built-in microphone. Recording does not stop the timer. Voice memos are saved as WAV files under `/voice_memos/` on the SD card.

- Hold `M`: record
- Release `M`: save
- `V`: open voice memo list
- `Fn + ;`: previous memo
- `Fn + /`: next memo
- `Enter`: play or stop
- `Del`: return

Synced devices use timestamp filenames. Unsynced devices use sequential filenames.

## Configuration

Copy `config.example.h` to `config.h` and edit only `config.h`.

```cpp
#define WIFI_ENABLED true
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
#define NTP_SERVER_3 "time.cloudflare.com"
#define GMT_OFFSET_SECONDS 32400
#define DAYLIGHT_OFFSET_SECONDS 0
```

Do not commit real Wi-Fi credentials. `config.h` is intentionally ignored.

If Wi-Fi is not configured, the timer still works. When a previous sync date is available, completed sessions are logged as `offline` with a date. If no date is available, they are logged as `unsynced`.

## CSV Log

Completed sessions are appended to:

```text
/study_log.csv
```

Current CSV format:

```csv
date,time,sync_status,duration_min,completed,input_type
2026-05-19,13:32:52,synced,25,1,preset4
2026-05-19,14:10:12,synced,45,1,custom
2026-05-19,,offline,25,1,preset4
,,unsynced,15,1,preset3
```

Columns:

- `date`: `YYYY-MM-DD`, empty when no date is available
- `time`: `HH:MM:SS`, empty when NTP is not synced
- `sync_status`: `synced`, `offline`, or `unsynced`
- `duration_min`: timer duration in minutes
- `completed`: `1` for completed sessions
- `input_type`: `preset1` to `preset5`, or `custom`

The date and time columns are split so the Cardputer can later aggregate daily totals locally without parsing a combined timestamp string.

## Build

Target board:

```text
m5stack:esp32:m5stack_cardputer
```

Required Arduino libraries include:

- M5Cardputer
- M5Unified
- M5GFX

Example compile command:

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer .
```

## Repository Contents

This public-ready repository intentionally tracks only:

- `CARDPUTER-StudyTimer.ino`
- `README.md`
- `README.ja.md`
- `LICENSE`
- `config.example.h`
- `images/`
- `.gitignore`

Local notes, generated docs, private configuration, and editor files are ignored.

## AI Assistance

This project was developed with assistance from AI tools, including Codex app and GitHub Copilot.

## License

This project is licensed under the MIT License.

Copyright (c) 2026 omiya-bonsai
