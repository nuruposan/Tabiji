#include "LogStore.h"

LogStore::LogStore(uint32_t magicNumber) : _magicNumber(magicNumber) {
}

LogStore::~LogStore() {
  flush();  // Ensure any buffered logs are written to the file
  end();
}

bool LogStore::begin(uint8_t entrySize, uint8_t writeCacheDepth, uint8_t readCacheDepth) {
  _writeBufferDepth = writeCacheDepth;
  _readBufferDepth = readCacheDepth;
  _entrySize = entrySize;
  _entryCount = 0;

  _writeBuffer = malloc(_entrySize * _writeBufferDepth);
  _readBuffer = malloc(_entrySize * _readBufferDepth);
  bool needReset = false;

  if ((_writeBuffer == nullptr) || (_readBuffer == nullptr)) {
    Serial.println("[LS.err] Memory allocation failed for log buffers.");
    return false;  // Memory allocation failed
  }

  if (!LittleFS.begin()) {
    Serial.println("[LS.err] Failed to mount LittleFS.");
    return false;
  }

  if (LittleFS.exists(DS_FILENAME)) {
    File file = LittleFS.open(DS_FILENAME, "r");
    if (!file) {
      Serial.println("[LS.err] Failed to open log info file for reading.");
      return false;
    }

    uint32_t fileSize = file.size();
    uint32_t magic;
    uint16_t entrySize;
    file.seek(0, SeekSet);
    file.readBytes((char *)&magic, sizeof(magic));
    file.readBytes((char *)&entrySize, sizeof(entrySize));

    needReset = (magic != _magicNumber) || (entrySize != _entrySize)  //
                || (fileSize < HEADER_LENGTH);

    file.close();

    if (needReset) {
      Serial.println("[LS.warn] Magic number or/and entry size mismatch. Clearing log.");
    } else {
      _entryCount = ((fileSize - HEADER_LENGTH) / _entrySize);
    }
  } else {
    Serial.println("[LS.info] Log files do not exist. Creating new log store.");
    needReset = true;  // Log file doesn't exist, need to create it
  }

  if (needReset && (reset() == false)) {
    return false;  // Reset failed
  }

  return true;
}

void LogStore::end() {
  // Ensure any buffered logs are written to the file
  flush();

  // Free allocated buffers and reset state
  if (_writeBuffer) free(_writeBuffer);
  if (_readBuffer) free(_readBuffer);
  _entryCount = 0;
  _entrySize = 0;
  _writeBuffer = nullptr;
  _readBuffer = nullptr;

  // Unmount LittleFS
  LittleFS.end();
}

bool LogStore::reset() {
  // Remove existing log files if they exist
  if (LittleFS.exists(DS_FILENAME)) {
    LittleFS.remove(DS_FILENAME);
  }

  // Reset the entry count to zero
  _entryCount = 0;
  _entriesBuffered = 0;

  File file = LittleFS.open(DS_FILENAME, FILE_WRITE);
  if (!file) {
    Serial.println("[LS.err] Failed to create log file.");
    return false;
  }
  file.seek(0, SeekSet);
  file.write((const uint8_t *)&_magicNumber, sizeof(_magicNumber));
  file.write((const uint8_t *)&_entrySize, sizeof(_entrySize));

  uint16_t paddingSize = HEADER_LENGTH - (sizeof(_magicNumber) + sizeof(_entrySize));
  for (uint16_t i = 0; i < paddingSize; ++i) {
    file.write((const uint8_t)0x00);
  }
  file.close();

  return true;
}

bool LogStore::append(void *data) {
  memcpy((uint8_t *)_writeBuffer + indexToOffset(_entriesBuffered), data, _entrySize);
  _entriesBuffered += 1;

  // If the write buffer is full, flush it to the file
  if (_entriesBuffered >= _writeBufferDepth) {
    return flush();
  }

  return true;
}

bool LogStore::flush() {
  // If there are no logs buffered, there's nothing to flush
  if (_entriesBuffered == 0) return true;

  File file = LittleFS.open(DS_FILENAME, FILE_APPEND);
  if (!file) {
    Serial.println("[LS.err] Failed to open log file for writing.");
    return false;
  }

  // Move to the end of the file and write the buffered logs
  for (uint8_t i = 0; i < _entriesBuffered; ++i) {
    file.write((const uint8_t *)_writeBuffer + indexToOffset(i), _entrySize);
  }
  file.close();

  _entryCount += _entriesBuffered;
  _entriesBuffered = 0;

  return true;
}

uint32_t LogStore::entryCount() const {
  return _entryCount;
}

bool LogStore::fetch(uint32_t startIndex) {
  if (startIndex >= _entryCount) {
    _readStartIndex = -1;  // Invalid index
    return false;
  }

  _readStartIndex = startIndex;
  _readBufferPos = 0;

  // Load the entries into the read buffer
  File file = LittleFS.open(DS_FILENAME, "r");
  if (!file) {
    Serial.println("Failed to open log file for reading");
    _readStartIndex = -1;
    return false;
  }

  uint32_t readPos = HEADER_LENGTH + indexToOffset(_readStartIndex);
  file.seek(readPos, SeekSet);
  for (uint8_t i = 0; i < _readBufferDepth; i++) {
    if ((_readStartIndex + i) >= _entryCount) break;  // Don't read beyond the available entries

    file.read((uint8_t *)_readBuffer + indexToOffset(i), _entrySize);
  }
  file.close();

  return true;
}

void *LogStore::readFirst(bool flushBuffer) {
  // Flush the write buffer before reading if requested
  if (flushBuffer) flush();

  if (_entryCount == 0) return nullptr;  // No entries to read

  // Load the first set of entries into the read buffer
  if (!fetch(0)) {
    return nullptr;  // Failed to fetch the first set of entries
  }

  uint8_t *entryPtr = (uint8_t *)_readBuffer;  // Pointer to the first entry in the read buffer
  _readBufferPos = 1;                          // Set the position to the next entry for subsequent reads

  return _readBuffer;  // Return pointer to the first entry in the read buffer
}

void *LogStore::readNext() {
  // return null if the read start index is invalid
  if (_readStartIndex < 0) return nullptr;  // Invalid read position

  // Fetch the next set of entries into the read buffer if needed
  if (_readBufferPos >= _readBufferDepth) {
    fetch(_readStartIndex + _readBufferPos);  // Load the next set of entries into the read buffer
  }

  // Check if we have reached the end of the available entries
  if ((_readStartIndex + _readBufferPos + 1) >= _entryCount) {
    _readStartIndex = -1;  // Invalidate the read start index
    return nullptr;        // No more entries to read
  }

  // Calculate the pointer to the next entry in the read buffer
  uint8_t *entryPtr = (uint8_t *)_readBuffer + indexToOffset(_readBufferPos);
  _readBufferPos += 1;

  return entryPtr;
}

void LogStore::dump(bool deaderNump, uint32_t entriesToDump) {
  if (LittleFS.exists(DS_FILENAME) == false) {
    return;
  }

  File file = LittleFS.open(DS_FILENAME, FILE_READ);
  if (!file) {
    Serial.println("[LS.err] Failed to open log file for dumping.");
    return;
  }

  uint32_t magic, entryCount, dataBlockSize;
  file.readBytes((char *)&magic, sizeof(magic));
  entryCount = (file.size() - HEADER_LENGTH) / _entrySize;
  dataBlockSize = file.size() - HEADER_LENGTH;

  Serial.printf("<Data store info>\n");
  Serial.printf("- Flash size: %u bytes total,  %u bytes free\n",  //
      LittleFS.totalBytes(), LittleFS.totalBytes() - LittleFS.usedBytes());
  Serial.printf("- Data file: %s (%u bytes)\n", DS_FILENAME, file.size());
  Serial.printf("- Stored entries: %u (%u on disk + %u in buffer)\n",  //
      (entryCount + _entriesBuffered), entryCount, _entriesBuffered);
  if (_entryCount != entryCount) {
    Serial.printf("[LS.warn] Entry count mismatch: expected=%u, actual=%u\n",  //
        _entryCount, entryCount);
  }
  Serial.println();

  Serial.printf("<File header in data file>\n");
  Serial.printf("- Header size: %u bytes\n", HEADER_LENGTH);
  Serial.printf("- Magic number: 0x%08X (%u)\n", magic, magic);
  Serial.printf("- Entry size: %u bytes\n", _entrySize);

  file.seek(0, SeekSet);
  for (uint8_t i = 0; i < HEADER_LENGTH; ++i) {
    if (i % 16 == 0) Serial.printf("    0x%04X: ", i);
    Serial.printf("%02X ", file.read());
    if (i % 16 == 15) Serial.println();
  }
  Serial.println();

  Serial.printf("<Data field in data file>\n", entryCount);
  Serial.printf("- Data field size: %u bytes\n", dataBlockSize);
  Serial.printf("- Entries on disk: %u\n", entryCount);
  Serial.println();
  if (entriesToDump > 0) {
    if (_entrySize <= entriesToDump) {
      file.seek(HEADER_LENGTH, SeekSet);  // Skip the header
    } else {
      file.seek(HEADER_LENGTH + indexToOffset(entryCount - entriesToDump),
          SeekSet);  // Seek to the start of the last entriesToDump entries
    }
    uint32_t entryStart = (entryCount > entriesToDump) ? (entryCount - entriesToDump) : 0;
    for (uint32_t i = entryStart; (i < entryCount) && (i < entryStart + entriesToDump); i++) {
      Serial.printf("entry[%u]: \n", i);
      for (uint8_t j = 0; j < _entrySize; ++j) {
        if ((j % 16) == 0) Serial.printf("    0x%04X: ", (HEADER_LENGTH + indexToOffset(i)) + j);
        Serial.printf("%02X ", file.read());
        if (((j % 16) == 15) && (j != (_entrySize - 1))) Serial.println();
      }
      if (entriesToDump) Serial.println();
    }
    Serial.println();
  }
  file.close();

  Serial.printf("<Write buffer>\n");
  Serial.printf("- Buffer capacity: %u bytes\n", (_entrySize * _writeBufferDepth));
  Serial.printf("- Entries in buffer: %u\n", _entriesBuffered);
  Serial.println();

  for (uint8_t i = 0; i < _entriesBuffered; ++i) {
    Serial.printf("entry[%u]:\n", (_entryCount + i));
    for (uint8_t j = 0; j < _entrySize; ++j) {
      if ((j % 16) == 0) Serial.printf("    0x%04X: ", indexToOffset(i) + j);
      Serial.printf("%02X ", *((uint8_t *)_writeBuffer + indexToOffset(i) + j));
      if (((j % 16) == 15) && (j != (_entrySize - 1))) Serial.println();
    }
    Serial.println();
  }
}

uint32_t LogStore::indexToOffset(uint32_t index) const {
  return index * _entrySize;
}