#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H

bool appendCompletedLog();
void openVoiceMemos();
void closeVoiceMemos();
void stopVoiceMemoPlayback();
bool startVoiceMemoPlayback();
bool moveSettingsSelection(SettingsDirection direction);
bool isSettingsState();
void nextSettingsPage();
void previousSettingsPage();
void openLanguageSettings();
void setLanguage(UiLanguage language);
void setSoundMode(SoundMode mode);
void setFeatureMode(FeatureMode mode);
void applySelectedSettingsItem();
void drawScreen();
bool richModeEnabled();

void beep(uint16_t frequency, uint16_t durationMs)
{
  if (currentSoundMode == SOUND_QUIET)
  {
    return;
  }
  if (currentSoundMode == SOUND_LOUD)
  {
    M5Cardputer.Speaker.setVolume(SPEAKER_LOUD_VOLUME);
    M5Cardputer.Speaker.tone(frequency, durationMs + 60);
    M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
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

bool settingsItemIsInDirection(const SettingsItemPosition &current, const SettingsItemPosition &candidate, SettingsDirection direction)
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

int settingsMoveScore(const SettingsItemPosition &current, const SettingsItemPosition &candidate, SettingsDirection direction)
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
    if (candidate.item == selectedSettingsItem || !settingsItemIsInDirection(*current, candidate, direction))
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
      return SETTINGS_DIRECTION_UP;
    }
    if (key == ',')
    {
      return SETTINGS_DIRECTION_LEFT;
    }
    if (key == '.')
    {
      return SETTINGS_DIRECTION_DOWN;
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
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed())
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
      if (key >= '1' && key <= '9')
      {
        uint16_t minutes = static_cast<uint16_t>(key - '0') * 5;
        applyTimerMinutes(minutes);
        activeTimerSource = "preset" + String(key);
        startTimer();
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

#endif
