#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

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

#endif
