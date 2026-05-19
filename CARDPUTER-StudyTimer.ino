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
  STATE_SETTINGS,
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

enum SettingsItem
{
  SETTINGS_LANG_JA,
  SETTINGS_LANG_EN,
  SETTINGS_SOUND_QUIET,
  SETTINGS_SOUND_NORMAL,
  SETTINGS_SOUND_LOUD
};

enum SettingsDirection
{
  SETTINGS_DIRECTION_NONE,
  SETTINGS_DIRECTION_UP,
  SETTINGS_DIRECTION_DOWN,
  SETTINGS_DIRECTION_LEFT,
  SETTINGS_DIRECTION_RIGHT
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
};

const uint32_t DEFAULT_TIMER_SECONDS = 25UL * 60UL;
const uint16_t MIN_TIMER_MINUTES = 1;
const uint16_t MAX_TIMER_MINUTES = 99;
const uint16_t PRESET_MINUTES[] = {5, 10, 15, 25, 45};
const char *PREF_NAMESPACE = "study_timer";
const char *PREF_LAST_MINUTES = "last_minutes";
const char *PREF_LANGUAGE = "language";
const char *PREF_SOUND_MODE = "sound_mode";
const char *LOG_FILENAME = "/study_log.csv";
const uint8_t SD_SPI_SCK_PIN = 40;
const uint8_t SD_SPI_MOSI_PIN = 14;
const uint8_t SD_SPI_MISO_PIN = 39;
const uint8_t SD_SPI_CS_PIN = 12;
const uint32_t WIFI_RETRY_INTERVAL_MS = 30000;
const uint32_t TIME_SYNC_CHECK_INTERVAL_MS = 2000;
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
const SettingsItemPosition SETTINGS_ITEM_POSITIONS[] = {
    {SETTINGS_LANG_JA, 82, 50},
    {SETTINGS_LANG_EN, 176, 50},
    {SETTINGS_SOUND_QUIET, 56, 86},
    {SETTINGS_SOUND_NORMAL, 166, 86},
    {SETTINGS_SOUND_LOUD, 120, 108},
};

AppState appState = STATE_READY;
AppState stateBeforeConfirm = STATE_READY;
UiLanguage currentLanguage = LANG_JA;
SoundMode currentSoundMode = SOUND_NORMAL;
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
uint32_t lastWifiRetryMs = 0;
uint32_t lastTimeSyncCheckMs = 0;
uint32_t lastInteractionMs = 0;
uint32_t lastSegmentBlinkMs = 0;
uint8_t currentBrightness = BRIGHTNESS_NORMAL;
bool segmentBlinkOn = true;
bool needsRedraw = true;

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

void saveLastMinutes()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUShort(PREF_LAST_MINUTES, selectedMinutes());
  preferences.end();
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
  }

  if (timeSynced || now - lastTimeSyncCheckMs < TIME_SYNC_CHECK_INTERVAL_MS)
  {
    return;
  }

  lastTimeSyncCheckMs = now;
  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 5))
  {
    timeSynced = true;
  }
}

LogTimeFields currentLogTimeFields()
{
  LogTimeFields fields = {"", "", false};
  if (!timeSynced)
  {
    return fields;
  }

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 5))
  {
    timeSynced = false;
    return fields;
  }

  char dateBuffer[11];
  char timeBuffer[9];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeInfo);
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);
  fields.date = dateBuffer;
  fields.time = timeBuffer;
  fields.synced = true;
  return fields;
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
  file.print(logTime.synced ? "synced" : "unsynced");
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
  setState(STATE_SETTINGS);
}

void setSoundMode(SoundMode mode)
{
  currentSoundMode = mode;
  saveSoundMode();
  beep(1000, 35);
  setState(STATE_SETTINGS);
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
  }
}

const SettingsItemPosition *currentSettingsItemPosition()
{
  for (const auto &position : SETTINGS_ITEM_POSITIONS)
  {
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

void moveSettingsSelection(SettingsDirection direction)
{
  const SettingsItemPosition *current = currentSettingsItemPosition();
  if (direction == SETTINGS_DIRECTION_NONE || current == nullptr)
  {
    return;
  }

  bool found = false;
  SettingsItem nextItem = selectedSettingsItem;
  int bestScore = 0;
  for (const auto &candidate : SETTINGS_ITEM_POSITIONS)
  {
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
  }
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
  if (appState == STATE_RUNNING || appState == STATE_DONE)
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
    drawCenteredLabel(tr("START  1-5  0  S", "開始  1-5  0  S"), 102, MUTED_COLOR);
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
    drawCenteredLabel(lastLogSaved ? tr("SAVED", "保存") : "LOG OFF", 98, MUTED_COLOR);
    break;

  case STATE_SETTINGS:
    drawCenteredLabel(tr("SETTINGS", "設定"), 12, TEXT_COLOR);
    drawLabelAt(settingsLabel(SETTINGS_LANG_JA, "1 Japanese", "1 日本語"), 34, 42,
                settingsItemColor(SETTINGS_LANG_JA, currentLanguage == LANG_JA));
    drawLabelAt(settingsLabel(SETTINGS_LANG_EN, "2 English", "2 English"), 132, 42,
                settingsItemColor(SETTINGS_LANG_EN, currentLanguage == LANG_EN));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_QUIET, "3 Quiet", "3 静音"), 8, 76,
                settingsItemColor(SETTINGS_SOUND_QUIET, currentSoundMode == SOUND_QUIET));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_NORMAL, "4 Normal", "4 普通の音"), 118, 76,
                settingsItemColor(SETTINGS_SOUND_NORMAL, currentSoundMode == SOUND_NORMAL));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_LOUD, "5 Loud", "5 うるさい"), 74, 98,
                settingsItemColor(SETTINGS_SOUND_LOUD, currentSoundMode == SOUND_LOUD));
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    break;

  case STATE_CONFIRM_RESET:
    drawCenteredLabel(tr("RESET?", "リセット?"), 34, TEXT_COLOR);
    drawCenteredLabel(tr("ENTER YES", "ENTER はい"), 70, MUTED_COLOR);
    drawCenteredLabel(tr("DEL NO", "DEL いいえ"), 96, MUTED_COLOR);
    break;
  }

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

  if (appState == STATE_SETTINGS)
  {
    SettingsDirection direction = settingsDirectionFromKeys(keys);
    if (direction != SETTINGS_DIRECTION_NONE)
    {
      moveSettingsSelection(direction);
      return;
    }
  }

  if (keys.del)
  {
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
        setState(STATE_SETTINGS);
        return;
      }
    }

    if (appState == STATE_CUSTOM_INPUT && key >= '0' && key <= '9')
    {
      appendCustomDigit(key);
      return;
    }

    if (appState == STATE_SETTINGS)
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
      if (key == '3')
      {
        selectedSettingsItem = SETTINGS_SOUND_QUIET;
        setSoundMode(SOUND_QUIET);
        return;
      }
      if (key == '4')
      {
        selectedSettingsItem = SETTINGS_SOUND_NORMAL;
        setSoundMode(SOUND_NORMAL);
        return;
      }
      if (key == '5')
      {
        selectedSettingsItem = SETTINGS_SOUND_LOUD;
        setSoundMode(SOUND_LOUD);
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

  case STATE_SETTINGS:
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
