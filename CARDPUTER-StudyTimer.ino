#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

#ifndef WIFI_ENABLED
#define WIFI_ENABLED true
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif

#ifndef NTP_SERVER_1
#define NTP_SERVER_1 NTP_SERVER
#endif

#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.google.com"
#endif

#ifndef NTP_SERVER_3
#define NTP_SERVER_3 "time.cloudflare.com"
#endif

#ifndef GMT_OFFSET_SECONDS
#define GMT_OFFSET_SECONDS 32400
#endif

#ifndef DAYLIGHT_OFFSET_SECONDS
#define DAYLIGHT_OFFSET_SECONDS 0
#endif

enum AppState
{
  STATE_READY,
  STATE_CUSTOM_INPUT,
  STATE_RUNNING,
  STATE_PAUSED,
  STATE_DONE,
  STATE_STATS,
  STATE_VOICE_MEMOS,
  STATE_LANGUAGE_SETTINGS,
  STATE_VOLUME_SETTINGS,
  STATE_MODE_SETTINGS,
  STATE_CONFIRM_RESET
};

enum UiLanguage
{
  LANG_JA,
  LANG_EN
};

enum SoundMode
{
  SOUND_QUIET,
  SOUND_NORMAL,
  SOUND_LOUD
};

enum FeatureMode
{
  MODE_SIMPLE,
  MODE_RICH
};

enum SettingsItem
{
  SETTINGS_LANG_JA,
  SETTINGS_LANG_EN,
  SETTINGS_SOUND_QUIET,
  SETTINGS_SOUND_NORMAL,
  SETTINGS_SOUND_LOUD,
  SETTINGS_MODE_SIMPLE,
  SETTINGS_MODE_RICH
};

enum SettingsDirection
{
  SETTINGS_DIRECTION_NONE,
  SETTINGS_DIRECTION_UP,
  SETTINGS_DIRECTION_DOWN,
  SETTINGS_DIRECTION_LEFT,
  SETTINGS_DIRECTION_RIGHT
};

enum TimeStatus
{
  TIME_STATUS_NO_WIFI_CONFIG,
  TIME_STATUS_WIFI_CONNECTING,
  TIME_STATUS_NTP_WAITING,
  TIME_STATUS_SYNCED,
  TIME_STATUS_OFFLINE_DATE,
  TIME_STATUS_NO_DATE
};

struct SettingsItemPosition
{
  SettingsItem item;
  int x;
  int y;
};

struct LogTimeFields
{
  String date;
  String time;
  bool synced;
  bool offlineDate;
};

struct StudyStats
{
  bool sdReady;
  bool timeReady;
  uint16_t todayMinutes;
  uint16_t lastSevenDaysMinutes;
  uint8_t streakDays;
  uint16_t dayMinutes[7];
};

struct VoiceMemoEntry
{
  String path;
  String label;
  uint32_t sizeBytes;
};

struct __attribute__((packed)) WavHeader
{
  char riff[4];
  uint32_t chunkSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t dataSize;
};

const uint32_t DEFAULT_TIMER_SECONDS = 25UL * 60UL;
const uint16_t MIN_TIMER_MINUTES = 1;
const uint16_t MAX_TIMER_MINUTES = 99;
const uint16_t PRESET_MINUTES[] = {5, 10, 15, 25, 45};
const char *PREF_NAMESPACE = "study_timer";
const char *PREF_LAST_MINUTES = "last_minutes";
const char *PREF_LANGUAGE = "language";
const char *PREF_SOUND_MODE = "sound_mode";
const char *PREF_FEATURE_MODE = "feature_mode";
const char *PREF_LAST_SYNC_DATE = "last_sync_date";
const char *PREF_LAST_SYNC_DAY = "last_sync_day";
const char *LOG_FILENAME = "/study_log.csv";
const char *VOICE_MEMO_DIR = "/voice_memos";
const uint8_t SD_SPI_SCK_PIN = 40;
const uint8_t SD_SPI_MOSI_PIN = 14;
const uint8_t SD_SPI_MISO_PIN = 39;
const uint8_t SD_SPI_CS_PIN = 12;
const uint32_t WIFI_RETRY_INTERVAL_MS = 30000;
const uint32_t NTP_RETRY_INTERVAL_MS = 5000;
const uint32_t NTP_RECONFIG_INTERVAL_MS = 30000;
const uint32_t CUSTOM_INPUT_TIMEOUT_MS = 7000;
const uint32_t DONE_RETURN_MS = 3000;
const uint32_t DIM_AFTER_MS = 30000;
const uint32_t DEEP_DIM_AFTER_MS = 120000;
const uint8_t BRIGHTNESS_NORMAL = 96;
const uint8_t BRIGHTNESS_DIM = 40;
const uint8_t BRIGHTNESS_DEEP_DIM = 12;
const uint8_t TOP_SEGMENTS = 15;
const uint8_t SIDE_SEGMENTS = 6;
const uint8_t TOTAL_SEGMENTS = (TOP_SEGMENTS * 2) + (SIDE_SEGMENTS * 2);
const uint32_t SEGMENT_BLINK_MS = 600;
const int SEGMENT_MARGIN_X = 8;
const int SEGMENT_MARGIN_Y = 8;
const int SEGMENT_GAP = 3;
const int SEGMENT_TOP_Y = 7;
const int SEGMENT_BOTTOM_Y = 126;
const int SEGMENT_SIDE_X_LEFT = 8;
const int SEGMENT_SIDE_X_RIGHT = 226;
const int SEGMENT_HORIZONTAL_W = 11;
const int SEGMENT_HORIZONTAL_H = 6;
const int SEGMENT_VERTICAL_W = 6;
const int SEGMENT_VERTICAL_H = 14;
const uint16_t BACKGROUND_COLOR = TFT_BLACK;
const uint16_t TEXT_COLOR = TFT_WHITE;
const uint16_t MUTED_COLOR = 0x8410;
const uint16_t DONE_COLOR = TFT_GREEN;
const uint16_t SEGMENT_OFF_COLOR = 0x18E3;
const uint16_t SEGMENT_DIM_COLOR = 0x4208;
const uint16_t LOW_BATTERY_COLOR = TFT_RED;
const uint8_t LOW_BATTERY_THRESHOLD_PERCENT = 20;
const uint8_t LOW_BATTERY_RELEASE_PERCENT = 23;
const uint8_t BATTERY_DISPLAY_HYSTERESIS_PERCENT = 2;
const uint32_t VOICE_SAMPLE_RATE = 8000;
const uint16_t VOICE_RECORD_SAMPLES = 256;
const uint16_t VOICE_PLAYBACK_SAMPLES = 512;
const uint8_t VOICE_PLAYBACK_BUFFERS = 3;
const uint8_t VOICE_MEMO_MAX_ENTRIES = 50;
const uint8_t VOICE_PLAYBACK_CHANNEL = 0;
const SettingsItemPosition LANGUAGE_SETTINGS_ITEM_POSITIONS[] = {
    {SETTINGS_LANG_JA, 120, 52},
    {SETTINGS_LANG_EN, 120, 80},
};
const SettingsItemPosition VOLUME_SETTINGS_ITEM_POSITIONS[] = {
    {SETTINGS_SOUND_QUIET, 120, 52},
    {SETTINGS_SOUND_NORMAL, 120, 76},
    {SETTINGS_SOUND_LOUD, 120, 100},
};
const SettingsItemPosition MODE_SETTINGS_ITEM_POSITIONS[] = {
    {SETTINGS_MODE_SIMPLE, 120, 52},
    {SETTINGS_MODE_RICH, 120, 80},
};

AppState appState = STATE_READY;
AppState stateBeforeConfirm = STATE_READY;
AppState stateBeforeVoiceMemos = STATE_READY;
UiLanguage currentLanguage = LANG_JA;
SoundMode currentSoundMode = SOUND_NORMAL;
FeatureMode currentFeatureMode = MODE_SIMPLE;
SettingsItem selectedSettingsItem = SETTINGS_LANG_JA;
Preferences preferences;
M5Canvas screenCanvas;

uint32_t selectedSeconds = DEFAULT_TIMER_SECONDS;
uint32_t remainingSeconds = DEFAULT_TIMER_SECONDS;
uint32_t lastTickMs = 0;
uint32_t doneSinceMs = 0;
uint32_t customInputUpdatedMs = 0;
uint16_t customInputMinutes = 0;
String activeTimerSource = "enter";
bool sdAvailable = false;
bool lastLogSaved = false;
bool wifiStarted = false;
bool ntpConfigured = false;
bool timeSynced = false;
bool offlineDateAvailable = false;
uint32_t lastWifiRetryMs = 0;
uint32_t lastTimeSyncCheckMs = 0;
uint32_t lastNtpConfigMs = 0;
uint32_t offlineDateBaseMs = 0;
uint8_t ntpFailCount = 0;
int32_t lastSyncDaySerial = -1;
String lastSyncDate = "";
uint32_t lastInteractionMs = 0;
uint32_t lastSegmentBlinkMs = 0;
uint8_t currentBrightness = BRIGHTNESS_NORMAL;
bool segmentBlinkOn = true;
bool needsRedraw = true;
bool voiceMemoKeyDown = false;
bool voiceRecording = false;
bool voicePlaying = false;
bool voicePlaybackSpeakerReady = false;
int displayedBatteryLevel = -1;
bool batteryLowLatched = false;
uint32_t voiceRecordingStartedMs = 0;
uint32_t voiceRecordedBytes = 0;
uint32_t voicePlaybackBytesRemaining = 0;
uint8_t voicePlaybackBufferIndex = 0;
uint8_t selectedVoiceMemoIndex = 0;
uint8_t voiceMemoCount = 0;
String voiceStatusText = "";
uint32_t voiceStatusUntilMs = 0;
File voiceMemoFile;
File voicePlaybackFile;
VoiceMemoEntry voiceMemoEntries[VOICE_MEMO_MAX_ENTRIES];
int16_t voiceRecordBuffer[VOICE_RECORD_SAMPLES];
int16_t voicePlaybackBuffers[VOICE_PLAYBACK_BUFFERS][VOICE_PLAYBACK_SAMPLES];

const char *tr(const char *en, const char *ja)
{
  return currentLanguage == LANG_JA ? ja : en;
}

int32_t daySerialFromDate(const String &date);
String dateFromDaySerial(int32_t daySerial);
const char *soundModeText();

#include "timer_engine.h"
#include "power_status.h"

const char *soundModeText()
{
  switch (currentSoundMode)
  {
  case SOUND_QUIET:
    return tr("Quiet", "静音");

  case SOUND_LOUD:
    return tr("Loud", "うるさい");

  case SOUND_NORMAL:
  default:
    return tr("Normal", "普通の音");
  }
}

#include "settings_manager.h"
#include "time_sync.h"
#include "voice_memo.h"
#include "stats_log.h"
#include "ui_renderer.h"
#include "app_state_machine.h"

void setup()
{
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextDatum(TL_DATUM);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setBrightness(BRIGHTNESS_NORMAL);
  M5Cardputer.Speaker.setVolume(80);
  screenCanvas.setColorDepth(16);
  screenCanvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  screenCanvas.setTextDatum(TL_DATUM);
  screenCanvas.setTextFont(1);
  lastInteractionMs = millis();

  loadLanguage();
  loadSoundMode();
  loadFeatureMode();
  loadLastSyncDate();
  loadLastMinutes();
  updateBatteryStatus();
  resetToReady();
  drawScreen();
  initSdLogging();
  startBackgroundWifi();
  needsRedraw = true;
}

void loop()
{
  M5Cardputer.update();
  updateBatteryStatus();
  updateVoiceMemoShortcut();
  updateVoiceRecording();
  updateVoicePlayback();
  updateVoiceStatus();
  updateBackgroundTimeSync();
  handleKeyboard();
  updateTimer();
  updateSegmentBlink();
  updatePowerSave();

  if (appState == STATE_CUSTOM_INPUT &&
      millis() - customInputUpdatedMs >= CUSTOM_INPUT_TIMEOUT_MS)
  {
    resetToReady();
  }

  if (appState == STATE_DONE && millis() - doneSinceMs >= DONE_RETURN_MS)
  {
    resetToReady();
  }

  drawScreen();
  delay(10);
}
