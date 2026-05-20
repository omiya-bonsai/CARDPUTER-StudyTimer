#ifndef POWER_STATUS_H
#define POWER_STATUS_H

int batteryLevelPercent()
{
  int batteryLevel = M5Cardputer.Power.getBatteryLevel();
  if (batteryLevel < 0)
  {
    return -1;
  }
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

void updateBatteryStatus()
{
  int rawBatteryLevel = batteryLevelPercent();
  bool charging = isPowerCharging();

  int previousDisplayed = displayedBatteryLevel;
  bool previousBatteryLow = batteryLowLatched;

  if (rawBatteryLevel < 0)
  {
    displayedBatteryLevel = -1;
    batteryLowLatched = false;
  }
  else
  {
    if (displayedBatteryLevel < 0 ||
        abs(rawBatteryLevel - displayedBatteryLevel) >= BATTERY_DISPLAY_HYSTERESIS_PERCENT)
    {
      displayedBatteryLevel = rawBatteryLevel;
    }

    if (charging)
    {
      batteryLowLatched = false;
    }
    else if (batteryLowLatched)
    {
      if (displayedBatteryLevel >= LOW_BATTERY_RELEASE_PERCENT)
      {
        batteryLowLatched = false;
      }
    }
    else if (displayedBatteryLevel <= LOW_BATTERY_THRESHOLD_PERCENT)
    {
      batteryLowLatched = true;
    }
  }

  if (previousDisplayed != displayedBatteryLevel || previousBatteryLow != batteryLowLatched)
  {
    needsRedraw = true;
  }
}

bool isBatteryLow()
{
  return batteryLowLatched;
}

String batteryText()
{
  int batteryLevel = displayedBatteryLevel;
  const char *label = tr("BAT", "充電");
  if (batteryLevel < 0)
  {
    return String(label) + " --%";
  }
  return String(label) + " " + String(batteryLevel) + "%";
}

String powerStatusText()
{
  int batteryLevel = displayedBatteryLevel;
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

#endif
