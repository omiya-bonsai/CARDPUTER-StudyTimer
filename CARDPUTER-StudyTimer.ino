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

String formatTime(uint32_t seconds)
{
  uint32_t minutes = seconds / 60;
  uint32_t secs = seconds % 60;
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu",
           static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(secs));
  return String(buffer);
}

const char *tr(const char *en, const char *ja)
{
  return currentLanguage == LANG_JA ? ja : en;
}

uint16_t selectedMinutes()
{
  return selectedSeconds / 60;
}

float remainingRatio()
{
  if (selectedSeconds == 0)
  {
    return 0.0f;
  }

  float ratio = static_cast<float>(remainingSeconds) / static_cast<float>(selectedSeconds);
  if (ratio < 0.0f)
  {
    return 0.0f;
  }
  if (ratio > 1.0f)
  {
    return 1.0f;
  }
  return ratio;
}

uint8_t remainingPercent()
{
  return static_cast<uint8_t>(remainingRatio() * 100.0f);
}

int32_t daySerialFromDate(const String &date);
String dateFromDaySerial(int32_t daySerial);

String timerMetaText()
{
  return String(selectedMinutes()) + " min / " + String(remainingPercent()) + "%";
}

int batteryLevelPercent()
{
  int batteryLevel = M5Cardputer.Power.getBatteryLevel();
  if (batteryLevel > 100)
  {
    return 100;
  }
  return batteryLevel;
}

bool isPowerCharging()
{
  return M5Cardputer.Power.isCharging() == 1;
}

bool isBatteryLow()
{
  int batteryLevel = batteryLevelPercent();
  return batteryLevel >= 0 &&
         batteryLevel <= LOW_BATTERY_THRESHOLD_PERCENT &&
         !isPowerCharging();
}

String batteryText()
{
  int batteryLevel = batteryLevelPercent();
  const char *label = tr("BAT", "充電");
  if (batteryLevel < 0)
  {
    return String(label) + " --%";
  }
  return String(label) + " " + String(batteryLevel) + "%";
}

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

String powerStatusText()
{
  int batteryLevel = batteryLevelPercent();
  const char *label = tr("PWR", "充電");
  if (isPowerCharging())
  {
    label = tr("CHG", "充電中");
  }
  else
  {
    label = tr("BAT", "電池");
  }

  if (batteryLevel < 0)
  {
    return String(label) + " --%";
  }
  return String(label) + " " + String(batteryLevel) + "%";
}

String deviceStatusText()
{
  return String(soundModeText()) + " / " + powerStatusText();
}

uint16_t deviceStatusColor()
{
  return isBatteryLow() ? LOW_BATTERY_COLOR : MUTED_COLOR;
}

uint16_t clampTimerMinutes(uint16_t minutes)
{
  if (minutes < MIN_TIMER_MINUTES)
  {
    return MIN_TIMER_MINUTES;
  }
  if (minutes > MAX_TIMER_MINUTES)
  {
    return MAX_TIMER_MINUTES;
  }
  return minutes;
}

void loadLastMinutes()
{
  preferences.begin(PREF_NAMESPACE, true);
  uint16_t minutes = preferences.getUShort(PREF_LAST_MINUTES, DEFAULT_TIMER_SECONDS / 60);
  preferences.end();

  minutes = clampTimerMinutes(minutes);
  selectedSeconds = static_cast<uint32_t>(minutes) * 60UL;
  remainingSeconds = selectedSeconds;
}

void loadLanguage()
{
  preferences.begin(PREF_NAMESPACE, true);
  uint8_t language = preferences.getUChar(PREF_LANGUAGE, LANG_JA);
  preferences.end();

  currentLanguage = language == LANG_EN ? LANG_EN : LANG_JA;
}

void saveLanguage()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUChar(PREF_LANGUAGE, currentLanguage);
  preferences.end();
}

void loadSoundMode()
{
  preferences.begin(PREF_NAMESPACE, true);
  uint8_t mode = preferences.getUChar(PREF_SOUND_MODE, SOUND_NORMAL);
  preferences.end();

  currentSoundMode = mode <= SOUND_LOUD ? static_cast<SoundMode>(mode) : SOUND_NORMAL;
}

void saveSoundMode()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUChar(PREF_SOUND_MODE, currentSoundMode);
  preferences.end();
}

void loadFeatureMode()
{
  preferences.begin(PREF_NAMESPACE, true);
  uint8_t mode = preferences.getUChar(PREF_FEATURE_MODE, MODE_SIMPLE);
  preferences.end();

  currentFeatureMode = mode == MODE_RICH ? MODE_RICH : MODE_SIMPLE;
}

void saveFeatureMode()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUChar(PREF_FEATURE_MODE, currentFeatureMode);
  preferences.end();
}

bool richModeEnabled()
{
  return currentFeatureMode == MODE_RICH;
}

void saveLastMinutes()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUShort(PREF_LAST_MINUTES, selectedMinutes());
  preferences.end();
}

void loadLastSyncDate()
{
  preferences.begin(PREF_NAMESPACE, true);
  lastSyncDate = preferences.getString(PREF_LAST_SYNC_DATE, "");
  lastSyncDaySerial = preferences.getInt(PREF_LAST_SYNC_DAY, -1);
  preferences.end();

  offlineDateAvailable = lastSyncDaySerial >= 0 && lastSyncDate.length() == 10;
  offlineDateBaseMs = millis();
}

void saveLastSyncDate(const String &date, int32_t daySerial)
{
  if (daySerial < 0 || date.length() != 10)
  {
    return;
  }

  preferences.begin(PREF_NAMESPACE, false);
  preferences.putString(PREF_LAST_SYNC_DATE, date);
  preferences.putInt(PREF_LAST_SYNC_DAY, daySerial);
  preferences.end();

  lastSyncDate = date;
  lastSyncDaySerial = daySerial;
  offlineDateAvailable = true;
  offlineDateBaseMs = millis();
}

void initSdLogging()
{
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  sdAvailable = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
}

bool wifiConfigured()
{
  return WIFI_ENABLED && String(WIFI_SSID).length() > 0;
}

void startBackgroundWifi()
{
  if (!wifiConfigured())
  {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiStarted = true;
  lastWifiRetryMs = millis();
}

void updateBackgroundTimeSync()
{
  if (!wifiConfigured())
  {
    timeSynced = false;
    return;
  }

  uint32_t now = millis();

  if (!wifiStarted)
  {
    startBackgroundWifi();
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    ntpConfigured = false;
    timeSynced = false;
    ntpFailCount = 0;
    if (now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS)
    {
      WiFi.disconnect(false);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastWifiRetryMs = now;
    }
    return;
  }

  if (!ntpConfigured)
  {
    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS,
               NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    ntpConfigured = true;
    lastNtpConfigMs = now;
  }

  if (!timeSynced && now - lastNtpConfigMs >= NTP_RECONFIG_INTERVAL_MS)
  {
    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS,
               NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    lastNtpConfigMs = now;
    ntpFailCount = 0;
  }

  if (timeSynced || now - lastTimeSyncCheckMs < NTP_RETRY_INTERVAL_MS)
  {
    return;
  }

  lastTimeSyncCheckMs = now;
  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 5))
  {
    timeSynced = true;
    ntpFailCount = 0;
    char dateBuffer[11];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeInfo);
    saveLastSyncDate(dateBuffer, daySerialFromDate(dateBuffer));
    needsRedraw = true;
    return;
  }

  ntpFailCount++;
  if (ntpFailCount >= 3)
  {
    ntpConfigured = false;
  }
}

LogTimeFields currentLogTimeFields()
{
  LogTimeFields fields = {"", "", false, false};

  struct tm timeInfo;
  if (timeSynced && getLocalTime(&timeInfo, 5))
  {
    char dateBuffer[11];
    char timeBuffer[9];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeInfo);
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);
    fields.date = dateBuffer;
    fields.time = timeBuffer;
    fields.synced = true;
    return fields;
  }

  timeSynced = false;
  if (offlineDateAvailable)
  {
    uint32_t elapsedDays = (millis() - offlineDateBaseMs) / 86400000UL;
    fields.date = dateFromDaySerial(lastSyncDaySerial + elapsedDays);
    fields.offlineDate = fields.date.length() == 10;
  }
  return fields;
}

String currentDateString()
{
  LogTimeFields fields = currentLogTimeFields();
  return fields.date;
}

const char *logSyncStatus(const LogTimeFields &fields)
{
  if (fields.synced)
  {
    return "synced";
  }
  if (fields.offlineDate)
  {
    return "offline";
  }
  return "unsynced";
}

TimeStatus currentTimeStatus()
{
  if (timeSynced)
  {
    return TIME_STATUS_SYNCED;
  }
  if (wifiConfigured())
  {
    if (!wifiStarted || WiFi.status() != WL_CONNECTED)
    {
      return offlineDateAvailable ? TIME_STATUS_OFFLINE_DATE : TIME_STATUS_WIFI_CONNECTING;
    }
    return offlineDateAvailable ? TIME_STATUS_OFFLINE_DATE : TIME_STATUS_NTP_WAITING;
  }
  return offlineDateAvailable ? TIME_STATUS_OFFLINE_DATE : TIME_STATUS_NO_WIFI_CONFIG;
}

const char *timeStatusLabel()
{
  switch (currentTimeStatus())
  {
  case TIME_STATUS_NO_WIFI_CONFIG:
    return tr("NO WIFI", "WiFi未設定");
  case TIME_STATUS_WIFI_CONNECTING:
    return "WIFI...";
  case TIME_STATUS_NTP_WAITING:
    return "NTP...";
  case TIME_STATUS_SYNCED:
    return tr("SYNCED", "同期済み");
  case TIME_STATUS_OFFLINE_DATE:
    return tr("OFFLINE DATE", "オフライン日付");
  case TIME_STATUS_NO_DATE:
    return tr("NO DATE", "日付なし");
  }
  return "";
}

void setVoiceStatus(const char *text, uint32_t durationMs = 1800)
{
  voiceStatusText = text;
  voiceStatusUntilMs = millis() + durationMs;
  needsRedraw = true;
}

WavHeader makeWavHeader(uint32_t dataSize)
{
  WavHeader header = {
      {'R', 'I', 'F', 'F'},
      dataSize + 36,
      {'W', 'A', 'V', 'E'},
      {'f', 'm', 't', ' '},
      16,
      1,
      1,
      VOICE_SAMPLE_RATE,
      VOICE_SAMPLE_RATE * 2,
      2,
      16,
      {'d', 'a', 't', 'a'},
      dataSize};
  return header;
}

void writeWavHeader(File &file, uint32_t dataSize)
{
  WavHeader header = makeWavHeader(dataSize);
  file.write(reinterpret_cast<uint8_t *>(&header), sizeof(header));
}

bool ensureVoiceMemoDir()
{
  if (!sdAvailable)
  {
    return false;
  }
  if (SD.exists(VOICE_MEMO_DIR))
  {
    return true;
  }
  return SD.mkdir(VOICE_MEMO_DIR);
}

String nextVoiceMemoPath()
{
  LogTimeFields fields = currentLogTimeFields();
  if (fields.synced)
  {
    String timePart = fields.time;
    timePart.replace(":", "");
    String basePath = String(VOICE_MEMO_DIR) + "/" + fields.date + "_" + timePart;
    String path = basePath + ".wav";
    for (uint8_t suffix = 2; SD.exists(path.c_str()) && suffix < 100; suffix++)
    {
      path = basePath + "_" + String(suffix) + ".wav";
    }
    return path;
  }

  for (uint16_t index = 1; index < 1000; index++)
  {
    char name[32];
    snprintf(name, sizeof(name), "/memo_%03u.wav", index);
    String path = String(VOICE_MEMO_DIR) + name;
    if (!SD.exists(path.c_str()))
    {
      return path;
    }
  }
  return String(VOICE_MEMO_DIR) + "/memo.wav";
}

String voiceMemoLabelFromPath(const String &path)
{
  int slash = path.lastIndexOf('/');
  String label = slash >= 0 ? path.substring(slash + 1) : path;
  if (label.endsWith(".wav"))
  {
    label.remove(label.length() - 4);
  }
  if (label.length() > 18)
  {
    label = label.substring(0, 18);
  }
  return label;
}

void stopVoiceMemoPlayback()
{
  if (voicePlaying)
  {
    M5Cardputer.Speaker.stop(VOICE_PLAYBACK_CHANNEL);
  }
  if (voicePlaybackFile)
  {
    voicePlaybackFile.close();
  }
  voicePlaying = false;
  voicePlaybackBytesRemaining = 0;
  needsRedraw = true;
}

void loadVoiceMemoList()
{
  voiceMemoCount = 0;
  selectedVoiceMemoIndex = 0;
  if (!ensureVoiceMemoDir())
  {
    return;
  }

  File dir = SD.open(VOICE_MEMO_DIR);
  if (!dir)
  {
    return;
  }

  File entry = dir.openNextFile();
  while (entry && voiceMemoCount < VOICE_MEMO_MAX_ENTRIES)
  {
    if (!entry.isDirectory())
    {
      String entryName = entry.name();
      String path = entryName.startsWith("/") ? entryName : String(VOICE_MEMO_DIR) + "/" + entryName;
      if (path.endsWith(".wav"))
      {
        voiceMemoEntries[voiceMemoCount].path = path;
        voiceMemoEntries[voiceMemoCount].label = voiceMemoLabelFromPath(path);
        voiceMemoEntries[voiceMemoCount].sizeBytes = entry.size();
        voiceMemoCount++;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void openVoiceMemos()
{
  if (appState == STATE_RUNNING || appState == STATE_CUSTOM_INPUT || voiceRecording)
  {
    return;
  }
  stopVoiceMemoPlayback();
  loadVoiceMemoList();
  stateBeforeVoiceMemos = appState;
  setState(STATE_VOICE_MEMOS);
}

void closeVoiceMemos()
{
  stopVoiceMemoPlayback();
  setState(stateBeforeVoiceMemos);
}

void startVoiceRecording()
{
  if (voiceRecording || voicePlaying)
  {
    return;
  }
  if (!ensureVoiceMemoDir())
  {
    setVoiceStatus("NO SD");
    return;
  }

  String path = nextVoiceMemoPath();
  voiceMemoFile = SD.open(path.c_str(), FILE_WRITE);
  if (!voiceMemoFile)
  {
    setVoiceStatus("REC ERR");
    return;
  }

  writeWavHeader(voiceMemoFile, 0);
  M5Cardputer.Speaker.end();
  if (!M5Cardputer.Mic.begin())
  {
    voiceMemoFile.close();
    SD.remove(path.c_str());
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(80);
    setVoiceStatus("MIC ERR");
    return;
  }

  voiceRecording = true;
  voiceRecordingStartedMs = millis();
  voiceRecordedBytes = 0;
  needsRedraw = true;
}

void finishVoiceRecording()
{
  if (!voiceRecording)
  {
    return;
  }

  while (M5Cardputer.Mic.isRecording())
  {
    delay(1);
  }
  M5Cardputer.Mic.end();

  if (voiceMemoFile)
  {
    voiceMemoFile.seek(0);
    writeWavHeader(voiceMemoFile, voiceRecordedBytes);
    voiceMemoFile.close();
  }

  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(80);
  voiceRecording = false;
  setVoiceStatus(voiceRecordedBytes > 0 ? "SAVED" : "EMPTY");
  if (appState == STATE_VOICE_MEMOS)
  {
    loadVoiceMemoList();
  }
}

void updateVoiceRecording()
{
  if (!voiceRecording)
  {
    return;
  }

  if (M5Cardputer.Mic.record(voiceRecordBuffer, VOICE_RECORD_SAMPLES, VOICE_SAMPLE_RATE, false))
  {
    size_t bytes = sizeof(voiceRecordBuffer);
    voiceMemoFile.write(reinterpret_cast<uint8_t *>(voiceRecordBuffer), bytes);
    voiceRecordedBytes += bytes;
  }
}

void updateVoiceMemoShortcut()
{
  if (!richModeEnabled())
  {
    if (voiceRecording)
    {
      finishVoiceRecording();
    }
    voiceMemoKeyDown = false;
    return;
  }

  Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
  bool mPressed = false;
  for (char key : keys.word)
  {
    if (key == 'm' || key == 'M')
    {
      mPressed = true;
      break;
    }
  }

  if (mPressed && !voiceMemoKeyDown)
  {
    voiceMemoKeyDown = true;
    startVoiceRecording();
  }
  else if (!mPressed && voiceMemoKeyDown)
  {
    voiceMemoKeyDown = false;
    finishVoiceRecording();
  }
}

bool startVoiceMemoPlayback()
{
  if (voiceMemoCount == 0 || selectedVoiceMemoIndex >= voiceMemoCount)
  {
    return false;
  }

  stopVoiceMemoPlayback();
  voicePlaybackFile = SD.open(voiceMemoEntries[selectedVoiceMemoIndex].path.c_str(), FILE_READ);
  if (!voicePlaybackFile)
  {
    setVoiceStatus("PLAY ERR");
    return false;
  }

  WavHeader header;
  if (voicePlaybackFile.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
      memcmp(header.riff, "RIFF", 4) != 0 ||
      memcmp(header.wave, "WAVE", 4) != 0 ||
      memcmp(header.data, "data", 4) != 0 ||
      header.audioFormat != 1 ||
      header.channels != 1 ||
      header.bitsPerSample != 16 ||
      header.sampleRate != VOICE_SAMPLE_RATE)
  {
    voicePlaybackFile.close();
    setVoiceStatus("WAV ERR");
    return false;
  }

  M5Cardputer.Mic.end();
  if (!voicePlaybackSpeakerReady)
  {
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(80);
    voicePlaybackSpeakerReady = true;
  }
  voicePlaybackBytesRemaining = header.dataSize;
  voicePlaybackBufferIndex = 0;
  voicePlaying = true;
  needsRedraw = true;
  return true;
}

void updateVoicePlayback()
{
  if (!voicePlaying)
  {
    return;
  }

  if (voicePlaybackBytesRemaining == 0)
  {
    if (!M5Cardputer.Speaker.isPlaying(VOICE_PLAYBACK_CHANNEL))
    {
      stopVoiceMemoPlayback();
    }
    return;
  }

  if (M5Cardputer.Speaker.isPlaying(VOICE_PLAYBACK_CHANNEL) > 1)
  {
    return;
  }

  size_t bufferBytes = sizeof(voicePlaybackBuffers[voicePlaybackBufferIndex]);
  size_t bytesToRead = voicePlaybackBytesRemaining < bufferBytes ? voicePlaybackBytesRemaining : bufferBytes;
  size_t bytesRead = voicePlaybackFile.read(reinterpret_cast<uint8_t *>(voicePlaybackBuffers[voicePlaybackBufferIndex]), bytesToRead);
  if (bytesRead == 0)
  {
    voicePlaybackBytesRemaining = 0;
    return;
  }

  voicePlaybackBytesRemaining -= bytesRead;
  M5Cardputer.Speaker.playRaw(voicePlaybackBuffers[voicePlaybackBufferIndex],
                              bytesRead / sizeof(int16_t),
                              VOICE_SAMPLE_RATE,
                              false,
                              1,
                              VOICE_PLAYBACK_CHANNEL);
  voicePlaybackBufferIndex = (voicePlaybackBufferIndex + 1) % VOICE_PLAYBACK_BUFFERS;
}

void updateVoiceStatus()
{
  if (voiceStatusText.length() > 0 && millis() >= voiceStatusUntilMs)
  {
    voiceStatusText = "";
    needsRedraw = true;
  }
}

String csvField(const String &line, uint8_t fieldIndex)
{
  int start = 0;
  for (uint8_t index = 0; index < fieldIndex; index++)
  {
    start = line.indexOf(',', start);
    if (start < 0)
    {
      return "";
    }
    start++;
  }

  int end = line.indexOf(',', start);
  if (end < 0)
  {
    end = line.length();
  }
  return line.substring(start, end);
}

int32_t daySerialFromDate(const String &date)
{
  if (date.length() < 10)
  {
    return -1;
  }

  struct tm dateInfo = {};
  dateInfo.tm_year = date.substring(0, 4).toInt() - 1900;
  dateInfo.tm_mon = date.substring(5, 7).toInt() - 1;
  dateInfo.tm_mday = date.substring(8, 10).toInt();
  dateInfo.tm_hour = 12;

  time_t timestamp = mktime(&dateInfo);
  if (timestamp < 0)
  {
    return -1;
  }
  return timestamp / 86400;
}

String dateFromDaySerial(int32_t daySerial)
{
  if (daySerial < 0)
  {
    return "";
  }

  time_t timestamp = (static_cast<time_t>(daySerial) * 86400) + 43200;
  struct tm *dateInfo = localtime(&timestamp);
  if (dateInfo == nullptr)
  {
    return "";
  }

  char dateBuffer[11];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", dateInfo);
  return String(dateBuffer);
}

StudyStats calculateStudyStats()
{
  StudyStats stats = {sdAvailable, false, 0, 0, 0, {0, 0, 0, 0, 0, 0, 0}};
  if (!sdAvailable || !SD.exists(LOG_FILENAME))
  {
    return stats;
  }

  String today = currentDateString();
  int32_t todaySerial = daySerialFromDate(today);
  if (todaySerial < 0)
  {
    return stats;
  }
  stats.timeReady = true;

  File file = SD.open(LOG_FILENAME, FILE_READ);
  if (!file)
  {
    stats.sdReady = false;
    sdAvailable = false;
    return stats;
  }

  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("date,"))
    {
      continue;
    }

    String completed = csvField(line, 4);
    if (completed != "1")
    {
      continue;
    }

    int32_t logSerial = daySerialFromDate(csvField(line, 0));
    int32_t dayOffset = todaySerial - logSerial;
    if (dayOffset < 0 || dayOffset >= 7)
    {
      continue;
    }

    uint16_t minutes = csvField(line, 3).toInt();
    stats.dayMinutes[6 - dayOffset] += minutes;
    stats.lastSevenDaysMinutes += minutes;
    if (dayOffset == 0)
    {
      stats.todayMinutes += minutes;
    }
  }
  file.close();

  int8_t streakIndex = stats.dayMinutes[6] > 0 ? 6 : 5;
  for (; streakIndex >= 0; streakIndex--)
  {
    if (stats.dayMinutes[streakIndex] == 0)
    {
      break;
    }
    stats.streakDays++;
  }

  return stats;
}

String normalizedInputType()
{
  if (activeTimerSource == "custom")
  {
    return "custom";
  }

  for (uint8_t index = 0; index < sizeof(PRESET_MINUTES) / sizeof(PRESET_MINUTES[0]); index++)
  {
    String presetSource = "preset" + String(index + 1);
    if (activeTimerSource == presetSource || selectedMinutes() == PRESET_MINUTES[index])
    {
      return presetSource;
    }
  }

  return "custom";
}

bool ensureLogHeader()
{
  if (SD.exists(LOG_FILENAME))
  {
    return true;
  }

  File file = SD.open(LOG_FILENAME, FILE_WRITE);
  if (!file)
  {
    return false;
  }
  file.println("date,time,sync_status,duration_min,completed,input_type");
  file.close();
  return true;
}

bool appendCompletedLog()
{
  lastLogSaved = false;
  if (!sdAvailable)
  {
    return false;
  }

  if (!ensureLogHeader())
  {
    sdAvailable = false;
    return false;
  }

  File file = SD.open(LOG_FILENAME, FILE_APPEND);
  if (!file)
  {
    sdAvailable = false;
    return false;
  }

  LogTimeFields logTime = currentLogTimeFields();

  file.print(logTime.date);
  file.print(",");
  file.print(logTime.time);
  file.print(",");
  file.print(logSyncStatus(logTime));
  file.print(",");
  file.print(selectedMinutes());
  file.print(",");
  file.print("1,");
  file.println(normalizedInputType());
  file.close();

  lastLogSaved = true;
  return true;
}

void beep(uint16_t frequency, uint16_t durationMs)
{
  if (currentSoundMode == SOUND_QUIET)
  {
    return;
  }

  if (currentSoundMode == SOUND_LOUD)
  {
    M5Cardputer.Speaker.setVolume(140);
    M5Cardputer.Speaker.tone(frequency, durationMs + 60);
    M5Cardputer.Speaker.setVolume(80);
    return;
  }

  M5Cardputer.Speaker.tone(frequency, durationMs);
}

void setBrightness(uint8_t brightness)
{
  if (currentBrightness == brightness)
  {
    return;
  }
  currentBrightness = brightness;
  M5Cardputer.Display.setBrightness(brightness);
}

void wakeDisplay()
{
  lastInteractionMs = millis();
  setBrightness(BRIGHTNESS_NORMAL);
}

void setState(AppState nextState)
{
  appState = nextState;
  needsRedraw = true;
}

void resetToReady()
{
  remainingSeconds = selectedSeconds;
  customInputMinutes = 0;
  activeTimerSource = "enter";
  setState(STATE_READY);
}

void applyTimerMinutes(uint16_t minutes)
{
  minutes = clampTimerMinutes(minutes);
  selectedSeconds = static_cast<uint32_t>(minutes) * 60UL;
  remainingSeconds = selectedSeconds;
}

void startTimer()
{
  if (remainingSeconds == 0)
  {
    remainingSeconds = selectedSeconds;
  }
  wakeDisplay();
  saveLastMinutes();
  lastTickMs = millis();
  beep(1200, 40);
  setState(STATE_RUNNING);
}

void pauseTimer()
{
  wakeDisplay();
  beep(800, 35);
  setState(STATE_PAUSED);
}

void enterResetConfirm()
{
  stateBeforeConfirm = appState;
  if (appState == STATE_RUNNING)
  {
    setState(STATE_CONFIRM_RESET);
    return;
  }
  setState(STATE_CONFIRM_RESET);
}

void cancelResetConfirm()
{
  if (stateBeforeConfirm == STATE_RUNNING)
  {
    lastTickMs = millis();
  }
  setState(stateBeforeConfirm);
}

void setLanguage(UiLanguage language)
{
  currentLanguage = language;
  saveLanguage();
  beep(1000, 35);
  setState(STATE_LANGUAGE_SETTINGS);
}

void setSoundMode(SoundMode mode)
{
  currentSoundMode = mode;
  saveSoundMode();
  beep(1000, 35);
  setState(STATE_VOLUME_SETTINGS);
}

void setFeatureMode(FeatureMode mode)
{
  currentFeatureMode = mode;
  saveFeatureMode();
  beep(1000, 35);
  setState(STATE_MODE_SETTINGS);
}

void applySelectedSettingsItem()
{
  switch (selectedSettingsItem)
  {
  case SETTINGS_LANG_JA:
    setLanguage(LANG_JA);
    break;

  case SETTINGS_LANG_EN:
    setLanguage(LANG_EN);
    break;

  case SETTINGS_SOUND_QUIET:
    setSoundMode(SOUND_QUIET);
    break;

  case SETTINGS_SOUND_NORMAL:
    setSoundMode(SOUND_NORMAL);
    break;

  case SETTINGS_SOUND_LOUD:
    setSoundMode(SOUND_LOUD);
    break;

  case SETTINGS_MODE_SIMPLE:
    setFeatureMode(MODE_SIMPLE);
    break;

  case SETTINGS_MODE_RICH:
    setFeatureMode(MODE_RICH);
    break;
  }
}

bool isSettingsState()
{
  return appState == STATE_LANGUAGE_SETTINGS ||
         appState == STATE_VOLUME_SETTINGS ||
         appState == STATE_MODE_SETTINGS;
}

void openLanguageSettings()
{
  selectedSettingsItem = currentLanguage == LANG_EN ? SETTINGS_LANG_EN : SETTINGS_LANG_JA;
  setState(STATE_LANGUAGE_SETTINGS);
}

void openVolumeSettings()
{
  switch (currentSoundMode)
  {
  case SOUND_QUIET:
    selectedSettingsItem = SETTINGS_SOUND_QUIET;
    break;

  case SOUND_NORMAL:
    selectedSettingsItem = SETTINGS_SOUND_NORMAL;
    break;

  case SOUND_LOUD:
    selectedSettingsItem = SETTINGS_SOUND_LOUD;
    break;
  }
  setState(STATE_VOLUME_SETTINGS);
}

void openModeSettings()
{
  selectedSettingsItem = currentFeatureMode == MODE_RICH ? SETTINGS_MODE_RICH : SETTINGS_MODE_SIMPLE;
  setState(STATE_MODE_SETTINGS);
}

void nextSettingsPage()
{
  if (appState == STATE_LANGUAGE_SETTINGS)
  {
    openVolumeSettings();
    return;
  }
  if (appState == STATE_VOLUME_SETTINGS)
  {
    openModeSettings();
  }
}

void previousSettingsPage()
{
  if (appState == STATE_MODE_SETTINGS)
  {
    openVolumeSettings();
    return;
  }
  if (appState == STATE_VOLUME_SETTINGS)
  {
    openLanguageSettings();
  }
}

const SettingsItemPosition *currentSettingsItemPosition()
{
  const SettingsItemPosition *positions = LANGUAGE_SETTINGS_ITEM_POSITIONS;
  uint8_t positionCount = sizeof(LANGUAGE_SETTINGS_ITEM_POSITIONS) / sizeof(LANGUAGE_SETTINGS_ITEM_POSITIONS[0]);
  if (appState == STATE_VOLUME_SETTINGS)
  {
    positions = VOLUME_SETTINGS_ITEM_POSITIONS;
    positionCount = sizeof(VOLUME_SETTINGS_ITEM_POSITIONS) / sizeof(VOLUME_SETTINGS_ITEM_POSITIONS[0]);
  }
  if (appState == STATE_MODE_SETTINGS)
  {
    positions = MODE_SETTINGS_ITEM_POSITIONS;
    positionCount = sizeof(MODE_SETTINGS_ITEM_POSITIONS) / sizeof(MODE_SETTINGS_ITEM_POSITIONS[0]);
  }

  for (uint8_t index = 0; index < positionCount; index++)
  {
    const SettingsItemPosition &position = positions[index];
    if (position.item == selectedSettingsItem)
    {
      return &position;
    }
  }
  return nullptr;
}

bool settingsItemIsInDirection(const SettingsItemPosition &current,
                               const SettingsItemPosition &candidate,
                               SettingsDirection direction)
{
  switch (direction)
  {
  case SETTINGS_DIRECTION_LEFT:
    return candidate.x < current.x;

  case SETTINGS_DIRECTION_RIGHT:
    return candidate.x > current.x;

  case SETTINGS_DIRECTION_UP:
    return candidate.y < current.y;

  case SETTINGS_DIRECTION_DOWN:
    return candidate.y > current.y;

  case SETTINGS_DIRECTION_NONE:
    return false;
  }

  return false;
}

int settingsMoveScore(const SettingsItemPosition &current,
                      const SettingsItemPosition &candidate,
                      SettingsDirection direction)
{
  int dx = candidate.x - current.x;
  int dy = candidate.y - current.y;
  if (dx < 0)
  {
    dx = -dx;
  }
  if (dy < 0)
  {
    dy = -dy;
  }

  if (direction == SETTINGS_DIRECTION_LEFT || direction == SETTINGS_DIRECTION_RIGHT)
  {
    return dx * dx + dy * dy * 10;
  }
  return dy * dy + dx * dx * 4;
}

bool moveSettingsSelection(SettingsDirection direction)
{
  const SettingsItemPosition *current = currentSettingsItemPosition();
  if (direction == SETTINGS_DIRECTION_NONE || current == nullptr)
  {
    return false;
  }

  const SettingsItemPosition *positions = LANGUAGE_SETTINGS_ITEM_POSITIONS;
  uint8_t positionCount = sizeof(LANGUAGE_SETTINGS_ITEM_POSITIONS) / sizeof(LANGUAGE_SETTINGS_ITEM_POSITIONS[0]);
  if (appState == STATE_VOLUME_SETTINGS)
  {
    positions = VOLUME_SETTINGS_ITEM_POSITIONS;
    positionCount = sizeof(VOLUME_SETTINGS_ITEM_POSITIONS) / sizeof(VOLUME_SETTINGS_ITEM_POSITIONS[0]);
  }
  if (appState == STATE_MODE_SETTINGS)
  {
    positions = MODE_SETTINGS_ITEM_POSITIONS;
    positionCount = sizeof(MODE_SETTINGS_ITEM_POSITIONS) / sizeof(MODE_SETTINGS_ITEM_POSITIONS[0]);
  }

  bool found = false;
  SettingsItem nextItem = selectedSettingsItem;
  int bestScore = 0;
  for (uint8_t index = 0; index < positionCount; index++)
  {
    const SettingsItemPosition &candidate = positions[index];
    if (candidate.item == selectedSettingsItem ||
        !settingsItemIsInDirection(*current, candidate, direction))
    {
      continue;
    }

    int score = settingsMoveScore(*current, candidate, direction);
    if (!found || score < bestScore)
    {
      found = true;
      bestScore = score;
      nextItem = candidate.item;
    }
  }

  if (found && nextItem != selectedSettingsItem)
  {
    selectedSettingsItem = nextItem;
    needsRedraw = true;
    return true;
  }

  return false;
}

void enterCustomInput()
{
  customInputMinutes = 0;
  customInputUpdatedMs = millis();
  setState(STATE_CUSTOM_INPUT);
}

void startPreset(uint8_t presetIndex)
{
  applyTimerMinutes(PRESET_MINUTES[presetIndex]);
  activeTimerSource = "preset" + String(presetIndex + 1);
  startTimer();
}

void appendCustomDigit(char digit)
{
  uint16_t nextValue = customInputMinutes * 10 + (digit - '0');
  if (nextValue > MAX_TIMER_MINUTES)
  {
    nextValue = MAX_TIMER_MINUTES;
  }
  customInputMinutes = nextValue;
  customInputUpdatedMs = millis();
  needsRedraw = true;
}

void deleteCustomDigit()
{
  customInputMinutes /= 10;
  customInputUpdatedMs = millis();
  needsRedraw = true;
}

void startCustomTimer()
{
  if (customInputMinutes < MIN_TIMER_MINUTES)
  {
    return;
  }
  applyTimerMinutes(customInputMinutes);
  activeTimerSource = "custom";
  customInputMinutes = 0;
  startTimer();
}

void finishTimer()
{
  wakeDisplay();
  remainingSeconds = 0;
  doneSinceMs = millis();
  appendCompletedLog();
  beep(1600, 120);
  setState(STATE_DONE);
}

void updateTimer()
{
  if (appState != STATE_RUNNING)
  {
    return;
  }

  uint32_t now = millis();
  while (now - lastTickMs >= 1000 && appState == STATE_RUNNING)
  {
    lastTickMs += 1000;
    if (remainingSeconds > 0)
    {
      remainingSeconds--;
      needsRedraw = true;
    }
    if (remainingSeconds == 0)
    {
      finishTimer();
    }
  }
}

void updatePowerSave()
{
  if (appState == STATE_RUNNING || appState == STATE_DONE || voiceRecording)
  {
    setBrightness(BRIGHTNESS_NORMAL);
    return;
  }

  uint32_t idleMs = millis() - lastInteractionMs;
  if (idleMs >= DEEP_DIM_AFTER_MS)
  {
    setBrightness(BRIGHTNESS_DEEP_DIM);
  }
  else if (idleMs >= DIM_AFTER_MS)
  {
    setBrightness(BRIGHTNESS_DIM);
  }
  else
  {
    setBrightness(BRIGHTNESS_NORMAL);
  }
}

void updateSegmentBlink()
{
  if (appState != STATE_RUNNING)
  {
    segmentBlinkOn = true;
    return;
  }

  uint32_t now = millis();
  if (now - lastSegmentBlinkMs >= SEGMENT_BLINK_MS)
  {
    lastSegmentBlinkMs = now;
    segmentBlinkOn = !segmentBlinkOn;
    needsRedraw = true;
  }
}

void drawCenteredText(const String &text, int y, int textSize, uint16_t color)
{
  screenCanvas.setTextSize(textSize);
  screenCanvas.setTextColor(color, BACKGROUND_COLOR);
  int textWidth = screenCanvas.textWidth(text);
  int x = (screenCanvas.width() - textWidth) / 2;
  screenCanvas.setCursor(max(0, x), y);
  screenCanvas.print(text);
}

void drawCenteredLabel(const char *text, int y, uint16_t color)
{
  screenCanvas.setFont(&fonts::lgfxJapanGothic_16);
  screenCanvas.setTextSize(1);
  screenCanvas.setTextColor(color, BACKGROUND_COLOR);
  int textWidth = screenCanvas.textWidth(text);
  int x = (screenCanvas.width() - textWidth) / 2;
  screenCanvas.setCursor(max(0, x), y);
  screenCanvas.print(text);
  screenCanvas.setTextFont(1);
}

void drawLabelAt(const String &text, int x, int y, uint16_t color)
{
  screenCanvas.setFont(&fonts::lgfxJapanGothic_16);
  screenCanvas.setTextSize(1);
  screenCanvas.setTextColor(color, BACKGROUND_COLOR);
  screenCanvas.setCursor(x, y);
  screenCanvas.print(text);
  screenCanvas.setTextFont(1);
}

void drawTimerMeta(int y, uint16_t color)
{
  drawCenteredLabel(timerMetaText().c_str(), y, color);
}

void drawBatteryInfo(int y, uint16_t color)
{
  drawCenteredLabel(batteryText().c_str(), y, color);
}

void drawDeviceStatus(int y, uint16_t color)
{
  drawCenteredLabel(deviceStatusText().c_str(), y, color);
}

uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
  return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

uint8_t blendChannel(uint8_t from, uint8_t to, float amount)
{
  return from + (to - from) * amount;
}

uint16_t blendRgb(uint8_t r1, uint8_t g1, uint8_t b1,
                  uint8_t r2, uint8_t g2, uint8_t b2,
                  float amount)
{
  return rgb565(blendChannel(r1, r2, amount),
                blendChannel(g1, g2, amount),
                blendChannel(b1, b2, amount));
}

uint16_t segmentColorForIndex(uint8_t index, bool dimmed)
{
  if (dimmed)
  {
    return SEGMENT_DIM_COLOR;
  }

  float position = static_cast<float>(index) / static_cast<float>(TOTAL_SEGMENTS - 1);
  if (position < 0.25f)
  {
    return blendRgb(0, 120, 255, 0, 220, 160, position / 0.25f);
  }
  if (position < 0.45f)
  {
    return blendRgb(0, 220, 160, 40, 220, 40, (position - 0.25f) / 0.20f);
  }
  if (position < 0.62f)
  {
    return blendRgb(40, 220, 40, 255, 230, 0, (position - 0.45f) / 0.17f);
  }
  if (position < 0.80f)
  {
    return blendRgb(255, 230, 0, 255, 120, 0, (position - 0.62f) / 0.18f);
  }
  return blendRgb(255, 120, 0, 255, 30, 30, (position - 0.80f) / 0.20f);
}

uint8_t segmentDrawIndex(uint8_t progressIndex)
{
  uint8_t topCenter = TOP_SEGMENTS / 2;

  if (progressIndex <= TOP_SEGMENTS - topCenter - 1)
  {
    return topCenter + progressIndex;
  }

  progressIndex -= TOP_SEGMENTS - topCenter;
  if (progressIndex < SIDE_SEGMENTS)
  {
    return TOP_SEGMENTS + progressIndex;
  }

  progressIndex -= SIDE_SEGMENTS;
  if (progressIndex < TOP_SEGMENTS)
  {
    return TOP_SEGMENTS + SIDE_SEGMENTS + progressIndex;
  }

  progressIndex -= TOP_SEGMENTS;
  if (progressIndex < SIDE_SEGMENTS)
  {
    return TOP_SEGMENTS + SIDE_SEGMENTS + TOP_SEGMENTS + progressIndex;
  }

  progressIndex -= SIDE_SEGMENTS;
  return progressIndex;
}

void segmentBounds(uint8_t index, int &x, int &y, int &w, int &h)
{
  w = SEGMENT_HORIZONTAL_W;
  h = SEGMENT_HORIZONTAL_H;

  int horizontalStep = SEGMENT_HORIZONTAL_W + SEGMENT_GAP;
  int topStartX = (screenCanvas.width() - (TOP_SEGMENTS * SEGMENT_HORIZONTAL_W) -
                   ((TOP_SEGMENTS - 1) * SEGMENT_GAP)) /
                  2;
  int verticalStep = SEGMENT_VERTICAL_H + SEGMENT_GAP;
  int sideStartY = SEGMENT_MARGIN_Y + SEGMENT_HORIZONTAL_H + SEGMENT_GAP;

  if (index < TOP_SEGMENTS)
  {
    x = topStartX + index * horizontalStep;
    y = SEGMENT_TOP_Y;
    return;
  }

  if (index < TOP_SEGMENTS + SIDE_SEGMENTS)
  {
    uint8_t sideIndex = index - TOP_SEGMENTS;
    x = SEGMENT_SIDE_X_RIGHT;
    y = sideStartY + sideIndex * verticalStep;
    w = SEGMENT_VERTICAL_W;
    h = SEGMENT_VERTICAL_H;
    return;
  }

  if (index < TOP_SEGMENTS + SIDE_SEGMENTS + TOP_SEGMENTS)
  {
    uint8_t bottomIndex = index - TOP_SEGMENTS - SIDE_SEGMENTS;
    x = topStartX + (TOP_SEGMENTS - 1 - bottomIndex) * horizontalStep;
    y = SEGMENT_BOTTOM_Y;
    return;
  }

  uint8_t sideIndex = index - TOP_SEGMENTS - SIDE_SEGMENTS - TOP_SEGMENTS;
  x = SEGMENT_SIDE_X_LEFT;
  y = sideStartY + (SIDE_SEGMENTS - 1 - sideIndex) * verticalStep;
  w = SEGMENT_VERTICAL_W;
  h = SEGMENT_VERTICAL_H;
}

void drawSegment(uint8_t index, bool lit, uint16_t color)
{
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  segmentBounds(index, x, y, w, h);
  screenCanvas.fillRoundRect(x, y, w, h, 2, lit ? color : SEGMENT_OFF_COLOR);
}

void drawSegments(float ratio, bool dimmed)
{
  uint8_t elapsedSegments = floorf((1.0f - ratio) * TOTAL_SEGMENTS);
  if (elapsedSegments > TOTAL_SEGMENTS)
  {
    elapsedSegments = TOTAL_SEGMENTS;
  }

  bool blinkEnabled = appState == STATE_RUNNING && elapsedSegments < TOTAL_SEGMENTS;
  for (uint8_t progressIndex = 0; progressIndex < TOTAL_SEGMENTS; progressIndex++)
  {
    uint8_t drawIndex = segmentDrawIndex(progressIndex);
    bool lit = progressIndex >= elapsedSegments;
    bool blinkingNext = blinkEnabled && progressIndex == elapsedSegments;
    if (blinkingNext && !segmentBlinkOn)
    {
      lit = false;
    }
    drawSegment(drawIndex, lit, segmentColorForIndex(progressIndex, dimmed));
  }
}

String settingsLabel(SettingsItem item, const char *en, const char *ja)
{
  String label = selectedSettingsItem == item ? ">" : " ";
  label += tr(en, ja);
  return label;
}

uint16_t settingsItemColor(SettingsItem item, bool active)
{
  if (selectedSettingsItem == item || active)
  {
    return TEXT_COLOR;
  }
  return MUTED_COLOR;
}

void drawStatsBars(const StudyStats &stats)
{
  uint16_t maxMinutes = 1;
  for (uint8_t index = 0; index < 7; index++)
  {
    if (stats.dayMinutes[index] > maxMinutes)
    {
      maxMinutes = stats.dayMinutes[index];
    }
  }

  const int barWidth = 20;
  const int barGap = 6;
  const int baseX = 30;
  const int baseY = 112;
  const int maxHeight = 32;
  for (uint8_t index = 0; index < 7; index++)
  {
    int barHeight = (stats.dayMinutes[index] * maxHeight) / maxMinutes;
    if (stats.dayMinutes[index] > 0 && barHeight < 2)
    {
      barHeight = 2;
    }
    int x = baseX + index * (barWidth + barGap);
    screenCanvas.drawRect(x, baseY - maxHeight, barWidth, maxHeight, MUTED_COLOR);
    screenCanvas.fillRect(x + 2, baseY - barHeight, barWidth - 4, barHeight, TEXT_COLOR);
  }
}

void drawStatsScreen()
{
  StudyStats stats = calculateStudyStats();
  drawCenteredLabel(tr("LOG", "記録"), 4, TEXT_COLOR);

  if (!stats.sdReady)
  {
    drawCenteredLabel("NO LOG", 58, MUTED_COLOR);
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    return;
  }

  if (!stats.timeReady)
  {
    TimeStatus status = currentTimeStatus();
    const char *statusLabel = status == TIME_STATUS_SYNCED ? tr("NO DATE", "日付なし") : timeStatusLabel();
    drawCenteredLabel(statusLabel, 48, MUTED_COLOR);
    if (status != TIME_STATUS_SYNCED)
    {
      drawCenteredLabel(tr("SYNC NEEDED", "同期が必要"), 76, MUTED_COLOR);
    }
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    return;
  }

  if (currentTimeStatus() == TIME_STATUS_OFFLINE_DATE)
  {
    drawCenteredLabel(timeStatusLabel(), 12, MUTED_COLOR);
  }
  drawLabelAt(tr("TODAY", "今日"), 12, 28, MUTED_COLOR);
  drawLabelAt(String(stats.todayMinutes) + "m", 88, 28, TEXT_COLOR);
  drawLabelAt(tr("7 DAYS", "7日間"), 12, 50, MUTED_COLOR);
  drawLabelAt(String(stats.lastSevenDaysMinutes) + "m", 88, 50, TEXT_COLOR);
  drawLabelAt(tr("STREAK", "連続"), 150, 28, MUTED_COLOR);
  drawLabelAt(String(stats.streakDays) + "d", 150, 50, TEXT_COLOR);
  drawStatsBars(stats);
  drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
}

void drawVoiceMemoScreen()
{
  drawCenteredLabel(tr("VOICE MEMO", "ボイスメモ"), 4, TEXT_COLOR);

  if (!sdAvailable)
  {
    drawCenteredLabel("NO SD", 58, MUTED_COLOR);
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    return;
  }

  if (voiceMemoCount == 0)
  {
    drawCenteredLabel(tr("NO MEMO", "メモなし"), 58, MUTED_COLOR);
    drawCenteredLabel(tr("M HOLD REC", "M長押し録音"), 88, MUTED_COLOR);
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    return;
  }

  String countLine = String(selectedVoiceMemoIndex + 1) + "/" + String(voiceMemoCount);
  drawCenteredLabel(countLine.c_str(), 28, MUTED_COLOR);
  drawCenteredLabel(voiceMemoEntries[selectedVoiceMemoIndex].label.c_str(), 52, TEXT_COLOR);
  uint32_t audioBytes = voiceMemoEntries[selectedVoiceMemoIndex].sizeBytes > sizeof(WavHeader)
                            ? voiceMemoEntries[selectedVoiceMemoIndex].sizeBytes - sizeof(WavHeader)
                            : 0;
  uint32_t seconds = audioBytes / (VOICE_SAMPLE_RATE * 2);
  if (audioBytes > 0 && seconds == 0)
  {
    seconds = 1;
  }
  drawCenteredLabel((String(seconds) + " sec").c_str(), 76, MUTED_COLOR);
  drawCenteredLabel(voicePlaying ? tr("ENTER STOP", "ENTER 停止") : tr("ENTER PLAY", "ENTER 再生"), 100, MUTED_COLOR);
  drawCenteredLabel(tr("Fn+; / Fn+/  DEL", "Fn+; / Fn+/  DEL"), 120, MUTED_COLOR);
}

void drawVoiceOverlay()
{
  if (voiceRecording)
  {
    screenCanvas.fillRect(196, 0, 44, 16, BACKGROUND_COLOR);
    drawLabelAt("REC", 202, 0, DONE_COLOR);
    return;
  }

  if (voiceStatusText.length() > 0 && millis() < voiceStatusUntilMs)
  {
    screenCanvas.fillRect(174, 0, 66, 16, BACKGROUND_COLOR);
    drawLabelAt(voiceStatusText, 176, 0, MUTED_COLOR);
  }
}

void drawScreen()
{
  if (!needsRedraw)
  {
    return;
  }

  needsRedraw = false;
  screenCanvas.fillScreen(BACKGROUND_COLOR);

  switch (appState)
  {
  case STATE_READY:
    drawSegments(1.0f, false);
    drawCenteredText(formatTime(remainingSeconds), 34, 5, TEXT_COLOR);
    drawDeviceStatus(78, deviceStatusColor());
    drawCenteredLabel(richModeEnabled() ? tr("START  1-5  0  S L V", "開始  1-5  0  S L V")
                                        : tr("START  1-5  0  S", "開始  1-5  0  S"),
                      102, MUTED_COLOR);
    if (!sdAvailable)
    {
      drawCenteredText("LOG OFF", 126, 1, MUTED_COLOR);
    }
    break;

  case STATE_CUSTOM_INPUT:
    drawCenteredLabel(tr("MIN", "分"), 20, MUTED_COLOR);
    if (customInputMinutes == 0)
    {
      drawCenteredText("_", 52, 4, TEXT_COLOR);
    }
    else
    {
      drawCenteredText(String(customInputMinutes), 52, 4, TEXT_COLOR);
    }
    drawCenteredLabel(tr("ENTER", "開始"), 108, MUTED_COLOR);
    break;

  case STATE_RUNNING:
    drawSegments(remainingRatio(), false);
    drawCenteredText(formatTime(remainingSeconds), 34, 5, TEXT_COLOR);
    drawDeviceStatus(78, deviceStatusColor());
    drawTimerMeta(102, MUTED_COLOR);
    break;

  case STATE_PAUSED:
    drawSegments(remainingRatio(), true);
    drawCenteredText(formatTime(remainingSeconds), 34, 5, MUTED_COLOR);
    drawTimerMeta(88, MUTED_COLOR);
    drawCenteredLabel(tr("PAUSE", "一時停止"), 110, TEXT_COLOR);
    break;

  case STATE_DONE:
    drawSegments(0.0f, false);
    drawCenteredLabel(tr("DONE", "完了"), 42, DONE_COLOR);
    if (lastLogSaved)
    {
      StudyStats stats = calculateStudyStats();
      drawCenteredLabel(tr("SAVED", "保存"), 88, MUTED_COLOR);
      if (stats.timeReady)
      {
        String todayLine = String(tr("Today ", "今日 ")) + String(stats.todayMinutes) + "m";
        drawCenteredLabel(todayLine.c_str(), 108, MUTED_COLOR);
      }
    }
    else
    {
      drawCenteredLabel("LOG OFF", 98, MUTED_COLOR);
    }
    break;

  case STATE_STATS:
    drawStatsScreen();
    break;

  case STATE_VOICE_MEMOS:
    drawVoiceMemoScreen();
    break;

  case STATE_LANGUAGE_SETTINGS:
    drawCenteredLabel(tr("Language", "言語の設定"), 10, TEXT_COLOR);
    drawLabelAt(settingsLabel(SETTINGS_LANG_JA, "1 Japanese", "1 日本語"), 62, 44,
                settingsItemColor(SETTINGS_LANG_JA, currentLanguage == LANG_JA));
    drawLabelAt(settingsLabel(SETTINGS_LANG_EN, "2 English", "2 English"), 62, 72,
                settingsItemColor(SETTINGS_LANG_EN, currentLanguage == LANG_EN));
    drawCenteredLabel(tr("NEXT: Fn+/", "次: Fn+/"), 96, MUTED_COLOR);
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    break;

  case STATE_VOLUME_SETTINGS:
    drawCenteredLabel(tr("Volume", "音量の設定"), 4, TEXT_COLOR);
    drawCenteredLabel(tr("PREV Fn+;  NEXT Fn+/", "前 Fn+;  次 Fn+/"), 24, MUTED_COLOR);
    drawLabelAt(settingsLabel(SETTINGS_SOUND_QUIET, "1 Quiet", "1 静音"), 62, 44,
                settingsItemColor(SETTINGS_SOUND_QUIET, currentSoundMode == SOUND_QUIET));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_NORMAL, "2 Normal", "2 普通の音"), 62, 68,
                settingsItemColor(SETTINGS_SOUND_NORMAL, currentSoundMode == SOUND_NORMAL));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_LOUD, "3 Loud", "3 うるさい"), 62, 96,
                settingsItemColor(SETTINGS_SOUND_LOUD, currentSoundMode == SOUND_LOUD));
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    break;

  case STATE_MODE_SETTINGS:
    drawCenteredLabel(tr("Mode", "モード"), 10, TEXT_COLOR);
    drawCenteredLabel(tr("PREV: Fn+;", "前: Fn+;"), 28, MUTED_COLOR);
    drawLabelAt(settingsLabel(SETTINGS_MODE_SIMPLE, "1 Simple", "1 シンプル"), 62, 52,
                settingsItemColor(SETTINGS_MODE_SIMPLE, currentFeatureMode == MODE_SIMPLE));
    drawLabelAt(settingsLabel(SETTINGS_MODE_RICH, "2 Rich", "2 リッチ"), 62, 80,
                settingsItemColor(SETTINGS_MODE_RICH, currentFeatureMode == MODE_RICH));
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    break;

  case STATE_CONFIRM_RESET:
    drawCenteredLabel(tr("RESET?", "リセット?"), 34, TEXT_COLOR);
    drawCenteredLabel(tr("ENTER YES", "ENTER はい"), 70, MUTED_COLOR);
    drawCenteredLabel(tr("DEL NO", "DEL いいえ"), 96, MUTED_COLOR);
    break;
  }

  drawVoiceOverlay();
  screenCanvas.pushSprite(&M5Cardputer.Display, 0, 0);
}

SettingsDirection settingsDirectionFromKeys(const Keyboard_Class::KeysState &keys)
{
  if (!keys.fn)
  {
    return SETTINGS_DIRECTION_NONE;
  }

  for (char key : keys.word)
  {
    if (key == ';')
    {
      return SETTINGS_DIRECTION_LEFT;
    }
    if (key == ',')
    {
      return SETTINGS_DIRECTION_DOWN;
    }
    if (key == '.')
    {
      return SETTINGS_DIRECTION_UP;
    }
    if (key == '/')
    {
      return SETTINGS_DIRECTION_RIGHT;
    }
  }

  return SETTINGS_DIRECTION_NONE;
}

void handleKeyboard()
{
  if (!M5Cardputer.Keyboard.isChange())
  {
    return;
  }

  if (!M5Cardputer.Keyboard.isPressed())
  {
    return;
  }

  wakeDisplay();
  Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

  if (appState == STATE_VOICE_MEMOS)
  {
    SettingsDirection direction = settingsDirectionFromKeys(keys);
    if (direction == SETTINGS_DIRECTION_LEFT && selectedVoiceMemoIndex > 0)
    {
      stopVoiceMemoPlayback();
      selectedVoiceMemoIndex--;
      needsRedraw = true;
      return;
    }
    if (direction == SETTINGS_DIRECTION_RIGHT && selectedVoiceMemoIndex + 1 < voiceMemoCount)
    {
      stopVoiceMemoPlayback();
      selectedVoiceMemoIndex++;
      needsRedraw = true;
      return;
    }
  }

  if (isSettingsState())
  {
    SettingsDirection direction = settingsDirectionFromKeys(keys);
    if (direction != SETTINGS_DIRECTION_NONE)
    {
      if (direction == SETTINGS_DIRECTION_RIGHT)
      {
        nextSettingsPage();
        return;
      }
      if (direction == SETTINGS_DIRECTION_LEFT)
      {
        previousSettingsPage();
        return;
      }
      moveSettingsSelection(direction);
      return;
    }
  }

  if (keys.del)
  {
    if (appState == STATE_VOICE_MEMOS)
    {
      closeVoiceMemos();
      return;
    }
    if (appState == STATE_CONFIRM_RESET)
    {
      cancelResetConfirm();
      return;
    }
    if (appState == STATE_RUNNING || appState == STATE_PAUSED)
    {
      enterResetConfirm();
      return;
    }
    if (appState == STATE_CUSTOM_INPUT && customInputMinutes > 0)
    {
      deleteCustomDigit();
      return;
    }
    resetToReady();
    return;
  }

  for (char key : keys.word)
  {
    if (richModeEnabled() && (key == 'v' || key == 'V') && appState != STATE_VOICE_MEMOS)
    {
      openVoiceMemos();
      return;
    }

    if (appState == STATE_READY)
    {
      if (key >= '1' && key <= '5')
      {
        startPreset(key - '1');
        return;
      }
      if (key == '0')
      {
        enterCustomInput();
        return;
      }
      if (key == 's' || key == 'S')
      {
        openLanguageSettings();
        return;
      }
      if (richModeEnabled() && (key == 'l' || key == 'L'))
      {
        setState(STATE_STATS);
        return;
      }
    }

    if (appState == STATE_CUSTOM_INPUT && key >= '0' && key <= '9')
    {
      appendCustomDigit(key);
      return;
    }

    if (appState == STATE_LANGUAGE_SETTINGS)
    {
      if (key == '1')
      {
        selectedSettingsItem = SETTINGS_LANG_JA;
        setLanguage(LANG_JA);
        return;
      }
      if (key == '2')
      {
        selectedSettingsItem = SETTINGS_LANG_EN;
        setLanguage(LANG_EN);
        return;
      }
    }

    if (appState == STATE_VOLUME_SETTINGS)
    {
      if (key == '1')
      {
        selectedSettingsItem = SETTINGS_SOUND_QUIET;
        setSoundMode(SOUND_QUIET);
        return;
      }
      if (key == '2')
      {
        selectedSettingsItem = SETTINGS_SOUND_NORMAL;
        setSoundMode(SOUND_NORMAL);
        return;
      }
      if (key == '3')
      {
        selectedSettingsItem = SETTINGS_SOUND_LOUD;
        setSoundMode(SOUND_LOUD);
        return;
      }
    }

    if (appState == STATE_MODE_SETTINGS)
    {
      if (key == '1')
      {
        selectedSettingsItem = SETTINGS_MODE_SIMPLE;
        setFeatureMode(MODE_SIMPLE);
        return;
      }
      if (key == '2')
      {
        selectedSettingsItem = SETTINGS_MODE_RICH;
        setFeatureMode(MODE_RICH);
        return;
      }
    }
  }

  if (!keys.enter)
  {
    if (appState == STATE_DONE)
    {
      resetToReady();
    }
    return;
  }

  switch (appState)
  {
  case STATE_READY:
    startTimer();
    break;

  case STATE_CUSTOM_INPUT:
    startCustomTimer();
    break;

  case STATE_RUNNING:
    pauseTimer();
    break;

  case STATE_PAUSED:
    startTimer();
    break;

  case STATE_DONE:
    resetToReady();
    break;

  case STATE_STATS:
    break;

  case STATE_VOICE_MEMOS:
    if (voicePlaying)
    {
      stopVoiceMemoPlayback();
    }
    else
    {
      startVoiceMemoPlayback();
    }
    break;

  case STATE_LANGUAGE_SETTINGS:
  case STATE_VOLUME_SETTINGS:
  case STATE_MODE_SETTINGS:
    applySelectedSettingsItem();
    break;

  case STATE_CONFIRM_RESET:
    resetToReady();
    break;
  }
}

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
  resetToReady();
  drawScreen();
  initSdLogging();
  startBackgroundWifi();
  needsRedraw = true;
}

void loop()
{
  M5Cardputer.update();
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
