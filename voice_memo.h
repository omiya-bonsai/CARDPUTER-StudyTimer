#ifndef VOICE_MEMO_H
#define VOICE_MEMO_H

void setState(AppState nextState);
void loadVoiceMemoList();
LogTimeFields currentLogTimeFields();
bool richModeEnabled();
void drawScreen();

struct __attribute__((packed)) WavSubChunkHeader
{
  char identifier[4];
  uint32_t chunkSize;
};

bool speakerRawSelfTest()
{
  static constexpr uint32_t testRate = 17000;
  static constexpr size_t testSamples = 17000; // 1 sec
  static int16_t testBuf[testSamples];
  for (size_t i = 0; i < testSamples; i++)
  {
    float phase = (2.0f * PI * 440.0f * i) / static_cast<float>(testRate);
    testBuf[i] = static_cast<int16_t>(sinf(phase) * 20000.0f);
  }
  bool queued = M5Cardputer.Speaker.playRaw(testBuf, testSamples, testRate, false, 1, 0, true);
  if (!queued)
  {
    return false;
  }
  do
  {
    delay(1);
    M5Cardputer.update();
  } while (M5Cardputer.Speaker.isPlaying());
  return true;
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
    M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
    setVoiceStatus("MIC ERR");
    return;
  }
  auto micCfg = M5Cardputer.Mic.config();
  micCfg.magnification = 32;
  micCfg.over_sampling = 2;
  micCfg.noise_filter_level = 0;
  micCfg.input_channel = m5::input_only_right;
  M5Cardputer.Mic.config(micCfg);

  voiceRecording = true;
  voiceRecordingStartedMs = millis();
  voiceRecordedBytes = 0;
  voiceRecordPeak = 0;
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
  if (voiceRecordedBytes == 0)
  {
    setVoiceStatus("EMPTY");
  }
  else if (voiceRecordPeak < VOICE_SILENCE_THRESHOLD)
  {
    setVoiceStatus("LOW MIC");
  }
  else
  {
    setVoiceStatus("SAVED");
  }
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
    for (uint16_t index = 0; index < VOICE_RECORD_SAMPLES; index++)
    {
      int32_t sample = voiceRecordBuffer[index];
      uint16_t amplitude = static_cast<uint16_t>(sample < 0 ? -sample : sample);
      if (amplitude > voiceRecordPeak)
      {
        voiceRecordPeak = amplitude;
      }
    }
    size_t bytes = sizeof(voiceRecordBuffer);
    size_t written = voiceMemoFile.write(reinterpret_cast<uint8_t *>(voiceRecordBuffer), bytes);
    voiceRecordedBytes += written;
    if (written != bytes)
    {
      setVoiceStatus("SD W ERR", 5000);
      drawScreen();
    }
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
  setVoiceStatus("PLAY OPEN", 5000);
  drawScreen();
  voicePlaybackFile = SD.open(voiceMemoEntries[selectedVoiceMemoIndex].path.c_str(), FILE_READ);
  if (!voicePlaybackFile)
  {
    setVoiceStatus("PLAY ERR");
    return false;
  }

  WavHeader header;
  setVoiceStatus("PLAY HDR", 5000);
  drawScreen();
  if (voicePlaybackFile.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
      memcmp(header.riff, "RIFF", 4) != 0 ||
      memcmp(header.wave, "WAVE", 4) != 0 ||
      header.audioFormat != 1 ||
      header.bitsPerSample < 8 ||
      header.bitsPerSample > 16 ||
      header.channels == 0 ||
      header.channels > 2 ||
      header.sampleRate == 0)
  {
    voicePlaybackFile.close();
    setVoiceStatus("WAV ERR");
    return false;
  }
  voicePlaybackFile.seek(offsetof(WavHeader, audioFormat) + header.fmtSize);
  WavSubChunkHeader chunk;
  if (voicePlaybackFile.read(reinterpret_cast<uint8_t *>(&chunk), sizeof(chunk)) != sizeof(chunk))
  {
    voicePlaybackFile.close();
    setVoiceStatus("WAV ERR");
    return false;
  }
  while (memcmp(chunk.identifier, "data", 4) != 0)
  {
    if (!voicePlaybackFile.seek(chunk.chunkSize, SeekMode::SeekCur) ||
        voicePlaybackFile.read(reinterpret_cast<uint8_t *>(&chunk), sizeof(chunk)) != sizeof(chunk))
    {
      voicePlaybackFile.close();
      setVoiceStatus("WAV ERR");
      return false;
    }
  }
  uint32_t dataLen = chunk.chunkSize;
  if (dataLen == 0)
  {
    voicePlaybackFile.close();
    setVoiceStatus("WAV SIZE");
    return false;
  }

  M5Cardputer.Mic.end();
  M5Cardputer.Speaker.end();
  delay(2);
  M5Cardputer.Speaker.begin();
  voicePlaybackSpeakerReady = true;
  auto spkCfg = M5Cardputer.Speaker.config();
  spkCfg.magnification = 64;
  M5Cardputer.Speaker.config(spkCfg);
  M5Cardputer.Speaker.setVolume(VOICE_PLAYBACK_VOLUME);
  M5Cardputer.Speaker.setAllChannelVolume(255);
  if (!speakerRawSelfTest())
  {
    setVoiceStatus("RAW NG", 5000);
    drawScreen();
    voicePlaybackFile.close();
    return false;
  }
  setVoiceStatus("RAW OK", 5000);
  drawScreen();
  delay(1000);
  M5Cardputer.update();
  setVoiceStatus("PLAY STR", 5000);
  drawScreen();
  bool is16Bit = header.bitsPerSample >= 16;
  bool isStereo = false;
  const uint32_t playbackRate = VOICE_SAMPLE_RATE;
  uint32_t playedBytes = 0;
  uint16_t rawPeak = 0;
  uint16_t rawSpan = 0;
  while (dataLen > 0)
  {
    size_t bytesToRead = dataLen;
    if (bytesToRead > sizeof(voicePlaybackBuffers[0]))
    {
      bytesToRead = sizeof(voicePlaybackBuffers[0]);
    }
    size_t bytesRead = voicePlaybackFile.read(reinterpret_cast<uint8_t *>(voicePlaybackBuffers[0]), bytesToRead);
    if (bytesRead == 0)
    {
      break;
    }
    playedBytes += bytesRead;
    while (M5Cardputer.Speaker.isPlaying())
    {
      delay(1);
      M5Cardputer.update();
    }
    dataLen -= bytesRead;
    bool queued = false;
    if (is16Bit)
    {
      size_t sampleCount = bytesRead >> 1;
      int16_t *samples = reinterpret_cast<int16_t *>(voicePlaybackBuffers[0]);
      int16_t minS = 32767;
      int16_t maxS = -32768;
      for (size_t i = 0; i < sampleCount; i++)
      {
        int16_t s = samples[i];
        if (s < minS)
        {
          minS = s;
        }
        if (s > maxS)
        {
          maxS = s;
        }
        uint16_t a = static_cast<uint16_t>(s < 0 ? -s : s);
        if (a > rawPeak)
        {
          rawPeak = a;
        }
      }
      uint16_t span = static_cast<uint16_t>(maxS - minS);
      if (span > rawSpan)
      {
        rawSpan = span;
      }
      for (size_t i = 0; i < sampleCount; i++)
      {
        int32_t amplified = static_cast<int32_t>(samples[i]) * VOICE_PLAYBACK_GAIN;
        if (amplified > 32767)
        {
          amplified = 32767;
        }
        else if (amplified < -32768)
        {
          amplified = -32768;
        }
        samples[i] = static_cast<int16_t>(amplified);
      }
      queued = M5Cardputer.Speaker.playRaw(reinterpret_cast<const int16_t *>(voicePlaybackBuffers[0]),
                                           sampleCount,
                                           playbackRate,
                                           isStereo,
                                           1,
                                           0,
                                           true);
    }
    else
    {
      queued = M5Cardputer.Speaker.playRaw(reinterpret_cast<const uint8_t *>(voicePlaybackBuffers[0]),
                                           bytesRead,
                                           playbackRate,
                                           isStereo,
                                           1,
                                           0,
                                           true);
    }
    if (!queued)
    {
      setVoiceStatus("SPK ERR", 5000);
      drawScreen();
      break;
    }
    M5Cardputer.update();
  }
  while (M5Cardputer.Speaker.isPlaying())
  {
    delay(1);
    M5Cardputer.update();
  }
  if (playedBytes == 0)
  {
    setVoiceStatus("NO PCM", 5000);
    drawScreen();
  }
  else
  {
    setVoiceStatus((String("B:") + String(playedBytes) + " P:" + String(rawPeak) + " S:" + String(rawSpan)).c_str(), 5000);
    drawScreen();
  }

  stopVoiceMemoPlayback();
  M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
  return true;
}

void updateVoicePlayback()
{
  if (!voicePlaying)
  {
    return;
  }

  if (M5Cardputer.Speaker.isPlaying())
  {
    return;
  }
  stopVoiceMemoPlayback();
  M5Cardputer.Speaker.setVolume(SPEAKER_DEFAULT_VOLUME);
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
