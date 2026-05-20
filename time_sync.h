#ifndef TIME_SYNC_H
#define TIME_SYNC_H

int32_t daySerialFromDate(const String &date);
String dateFromDaySerial(int32_t daySerial);

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

#endif
