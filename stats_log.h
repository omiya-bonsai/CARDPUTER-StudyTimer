#ifndef STATS_LOG_H
#define STATS_LOG_H

String currentDateString();
LogTimeFields currentLogTimeFields();
const char *logSyncStatus(const LogTimeFields &fields);

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

#endif
