#ifndef TIMER_ENGINE_H
#define TIMER_ENGINE_H

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

#endif
