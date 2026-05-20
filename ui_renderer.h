#ifndef UI_RENDERER_H
#define UI_RENDERER_H

StudyStats calculateStudyStats();
TimeStatus currentTimeStatus();
const char *timeStatusLabel();
bool richModeEnabled();

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
  int topStartX = (screenCanvas.width() - (TOP_SEGMENTS * SEGMENT_HORIZONTAL_W) - ((TOP_SEGMENTS - 1) * SEGMENT_GAP)) / 2;
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
  int x = 0, y = 0, w = 0, h = 0;
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
    drawCenteredLabel(richModeEnabled() ? tr("START  1-5  0  S L V", "開始  1-5  0  S L V") : tr("START  1-5  0  S", "開始  1-5  0  S"), 102, MUTED_COLOR);
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
    drawLabelAt(settingsLabel(SETTINGS_LANG_JA, "1 Japanese", "1 日本語"), 62, 44, settingsItemColor(SETTINGS_LANG_JA, currentLanguage == LANG_JA));
    drawLabelAt(settingsLabel(SETTINGS_LANG_EN, "2 English", "2 English"), 62, 72, settingsItemColor(SETTINGS_LANG_EN, currentLanguage == LANG_EN));
    drawCenteredLabel(tr("NEXT: Fn+/", "次: Fn+/"), 96, MUTED_COLOR);
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    break;
  case STATE_VOLUME_SETTINGS:
    drawCenteredLabel(tr("Volume", "音量の設定"), 4, TEXT_COLOR);
    drawCenteredLabel(tr("PREV Fn+;  NEXT Fn+/", "前 Fn+;  次 Fn+/"), 24, MUTED_COLOR);
    drawLabelAt(settingsLabel(SETTINGS_SOUND_QUIET, "1 Quiet", "1 静音"), 62, 44, settingsItemColor(SETTINGS_SOUND_QUIET, currentSoundMode == SOUND_QUIET));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_NORMAL, "2 Normal", "2 普通の音"), 62, 68, settingsItemColor(SETTINGS_SOUND_NORMAL, currentSoundMode == SOUND_NORMAL));
    drawLabelAt(settingsLabel(SETTINGS_SOUND_LOUD, "3 Loud", "3 うるさい"), 62, 96, settingsItemColor(SETTINGS_SOUND_LOUD, currentSoundMode == SOUND_LOUD));
    drawCenteredLabel(tr("DEL BACK", "DEL 戻る"), 120, MUTED_COLOR);
    break;
  case STATE_MODE_SETTINGS:
    drawCenteredLabel(tr("Mode", "モード"), 10, TEXT_COLOR);
    drawCenteredLabel(tr("PREV: Fn+;", "前: Fn+;"), 28, MUTED_COLOR);
    drawLabelAt(settingsLabel(SETTINGS_MODE_SIMPLE, "1 Simple", "1 シンプル"), 62, 52, settingsItemColor(SETTINGS_MODE_SIMPLE, currentFeatureMode == MODE_SIMPLE));
    drawLabelAt(settingsLabel(SETTINGS_MODE_RICH, "2 Rich", "2 リッチ"), 62, 80, settingsItemColor(SETTINGS_MODE_RICH, currentFeatureMode == MODE_RICH));
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

#endif
