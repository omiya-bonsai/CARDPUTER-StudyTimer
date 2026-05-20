#ifndef VOICE_MEMO_H
#define VOICE_MEMO_H

void setState(AppState nextState);
void loadVoiceMemoList();
LogTimeFields currentLogTimeFields();
bool richModeEnabled();

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
    M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
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
  M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
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
    M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
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

#endif
