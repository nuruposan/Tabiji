#include "LogStore.h"

LogStore::LogStore(uint32_t magicNumber) : _magicNumber(magicNumber) {
}

LogStore::~LogStore() {
  flush();  // Ensure any buffered logs are written to the file
  end();
}

bool LogStore::begin(int8_t dataSize, int8_t writeCacheDepth, int8_t readCacheDepth) {
  _writeBufferDepth = writeCacheDepth;
  _readBufferDepth = readCacheDepth;
  _dataSize = dataSize;
  _entrySize = (dataSize + CHECKSUM_FIELD_LENGTH);
  _entryCount = 0;
  _readIndexAdjustment = 0;

  _writeBuffer = malloc(_entrySize * _writeBufferDepth);
  _readBuffer = malloc(_dataSize * _readBufferDepth);

  // Check if memory allocation for buffers is successful
  if ((_writeBuffer == nullptr) || (_readBuffer == nullptr)) {  // failed to allocate memory
    Serial.println("[LS.err] Memory allocation failed for log buffers.");
    end();  // Clean up resources

    return false;
  }

  // Mount the LittleFS file system
  if (!LittleFS.begin()) {  // failed to mount LittleFS
    Serial.println("[LS.err] Failed to mount LittleFS.");
    end();  // Clean up resources

    return false;
  }

  // Check if the log file exists and if it needs to be reset based on the header information
  bool needReset = false;
  if (LittleFS.exists(DS_FILENAME)) {  // log file exists

    File file = LittleFS.open(DS_FILENAME, "r");
    if (!file) {
      Serial.println("[LS.err] Failed to open log info file for reading.");
      return false;
    }

    uint32_t readMagicNum = 0;
    uint32_t readEntrySize = 0;
    uint32_t readEntryCount = 0;
    size_t fileSize = file.size();

    file.seek(0, SeekSet);
    file.readBytes((char *)&readMagicNum, sizeof(readMagicNum));
    file.readBytes((char *)&readEntrySize, sizeof(readEntrySize));
    file.readBytes((char *)&readEntryCount, sizeof(readEntryCount));
    file.close();

    needReset = (fileSize < HEADER_LENGTH)         // File is too small
                || (_magicNumber != readMagicNum)  // Magic number mismatch
                || (_entrySize != readEntrySize);  // Entry size mismatch
    uint32_t entryCountFromFileSize = (fileSize - HEADER_LENGTH) / _entrySize;

    // Log file needs to be reset or need recovery due to misalignment
    if (needReset) {  // need to reset the log file
      Serial.println("[LS.warn] Magic number or/and entry size mismatch. Clearing the log file.");
    } else if (entryCountFromFileSize != readEntryCount) {  // entry count mismatch
      Serial.println("[LS.warn] The entry count in the log file does not match the header information.");

      // Calculate the number of entries based on the file size and entry size
      _entryCount = ((fileSize - HEADER_LENGTH) / _entrySize);
    } else {  // Log file is consistent with the header
      // Set the entry count based on the header information
      _entryCount = readEntryCount;
    }
  } else {  // no log file exists
    Serial.println("[LS.info] Log files do not exist. Creating new log store.");
    needReset = true;  // Log file doesn't exist, need to create it
  }

  if (needReset && (reset() == false)) {
    Serial.println("[LS.err] Failed to reset the log store.");
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
  _readIndexAdjustment = 0;
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
  _readIndexAdjustment = 0;

  // Create and open a new log file for writing
  File file = LittleFS.open(DS_FILENAME, FILE_WRITE);
  if (!file) {  // Failed to create a new log file
    Serial.println("[LS.err] Failed to create a new log file.");
    return false;
  }

  // Write the header information to the new log file
  file.write((const uint8_t *)&_magicNumber, sizeof(_magicNumber));
  file.write((const uint8_t *)&_entrySize, sizeof(_entrySize));
  file.write((const uint8_t *)&_entryCount, sizeof(_entryCount));

  // Fill the remaining header space with padding bytes
  uint16_t paddingSize = HEADER_LENGTH - (sizeof(_magicNumber) + sizeof(_entrySize) + sizeof(_entryCount));
  for (uint16_t i = 0; i < paddingSize; ++i) {
    file.write((const uint8_t)0xFF);  // fill remaining bytes
  }
  file.close();

  return true;
}

uint8_t LogStore::checksum(void *data, uint32_t dataSize) {
  // Calculate the checksum by XOR all bytes in the data block.
  uint8_t cs = 0;
  uint8_t *bytes = (uint8_t *)data;
  for (uint32_t i = 0; i < dataSize; ++i) {
    cs ^= bytes[i];  // XOR each byte
  }

  return cs;
}

bool LogStore::append(void *data) {
  // Determine the location in the write buffer where the new entry will be placed.
  uint8_t *entry = (uint8_t *)_writeBuffer + indexToOffset(_entriesBuffered);

  // Copy the data into the write buffer and calculate the checksum.
  memcpy(entry, data, _dataSize);  // Copy the actual data

  // Append the checksum at the tail of the data
  entry[_dataSize] = CHECKSUM_DELIMITER;             // Add the checksum delimiter
  entry[_dataSize + 1] = checksum(data, _dataSize);  // Add the calculated checksum

  // Increment the number of entries currently buffered
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

  // Open the log file for writing and append the buffered entries
  File file = LittleFS.open(DS_FILENAME, "r+");
  if (!file) {
    Serial.println("[LS.err] Failed to open log file for writing.");
    return false;
  }

  uint32_t bytesWritten = 0;
  uint32_t newEntryCount = _entryCount + _entriesBuffered;
  // Update the entry count in the file header
  file.seek(sizeof(_magicNumber) + sizeof(_entrySize), SeekSet);
  bytesWritten = file.write((const uint8_t *)&newEntryCount, sizeof(_entryCount));
  if (bytesWritten != sizeof(_entryCount)) {
    Serial.println("[LS.err] Failed to update log entry count in the file header.");
    file.close();

    return false;
  }

  // Write the buffered entries to the file starting from the calculated write position.
  uint32_t writePos = HEADER_LENGTH + (_entryCount * _entrySize);
  file.seek(writePos, SeekSet);
  for (uint8_t i = 0; i < _entriesBuffered; ++i) {
    // Write the buffered entry to the file
    bytesWritten = file.write((const uint8_t *)(_writeBuffer + indexToOffset(i)), _entrySize);

    // Check if the write was successful
    if (bytesWritten != _entrySize) {  // Failed to write the entire entry
      Serial.println("[LS.err] Failed to write log entry.");
      file.close();

      return false;
    }
  }

  // Flush the file to ensure all buffered data is written to the storage
  file.flush();

  // Update entry count
  _entryCount = newEntryCount;
  _entriesBuffered = 0;

  // Close the file after updating the header and writing all entries
  file.close();

  return true;
}

uint32_t LogStore::entryCount() const {
  return _entryCount;
}

bool LogStore::fetch(uint32_t startIndex) {
  // return false if the requested start index is beyond the current entry count
  if (startIndex >= _entryCount) {
    _readStartIndex = -1;  // Invalid index
    _readIndexAdjustment = 0;
    return false;
  }

  // Reset the read offset adjustment before fetching new entries
  _readStartIndex = startIndex;
  _readBufferPos = 0;

  // Open the log file for reading
  File file = LittleFS.open(DS_FILENAME, "r");
  if (!file) {  // Failed to open
    Serial.println("Failed to open log file for reading");
    _readStartIndex = -1;
    _readIndexAdjustment = 0;

    return false;
  }

  int i = 0;
  uint32_t readPos = readIndexToFilePos(_readStartIndex);
  file.seek(readPos, SeekSet);
  while (i < _readBufferDepth) {
    if ((file.position() + _entrySize) > file.size()) break;  // Stop if no more data is available
    if ((_readStartIndex + i) >= _entryCount) break;          // Don't read beyond the available entries

    file.read((uint8_t *)_readBuffer + indexToOffset(i), _dataSize);
    uint8_t readDelimiter = file.read();  // Read the checksum delimiter
    uint8_t readChecksum = file.read();   // Read the stored checksum from the file

    uint8_t checksum = 0;
    for (uint16_t j = 0; j < _dataSize; ++j) {
      checksum ^= *((uint8_t *)_readBuffer + indexToOffset(i) + j);
    }

    // Verify the checksum and delimiter before accepting the entry
    if ((readDelimiter != CHECKSUM_DELIMITER) || (checksum != readChecksum)) {
      // Skip the whole corrupted entry (one entry unit)
      _readIndexAdjustment += 1;
      readPos = readIndexToFilePos(_readStartIndex);

      file.seek(readPos, SeekSet);

      continue;
    }

    readPos += _entrySize;  // Move to the next entry position
    i++;
  }
  file.close();

  if (i == 0) {  // no entries were successfully read
    _readStartIndex = -1;
    _readIndexAdjustment = 0;

    return false;
  }

  return true;
}

void *LogStore::readFirst(bool flushBuffer) {
  // Flush the write buffer before reading if requested
  if (flushBuffer) flush();

  if (_entryCount == 0) return nullptr;  // No entries to read

  // Load the first set of entries into the read buffer
  _readIndexAdjustment = 0;
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
        if ((j % 16) == 0) Serial.printf("    0x%04X: ", (HEADER_LENGTH + indexToOffset(i, j)));
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
      if ((j % 16) == 0) Serial.printf("    0x%04X: ", indexToOffset(i, j));
      Serial.printf("%02X ", *((uint8_t *)_writeBuffer + indexToOffset(i, j)));
      if (((j % 16) == 15) && (j != (_entrySize - 1))) Serial.println();
    }
    Serial.println();
  }
}

uint32_t LogStore::indexToOffset(uint32_t index, uint32_t shift) const {
  return (index * _entrySize) + shift;
}

uint32_t LogStore::readIndexToFilePos(uint32_t index) const {
  return HEADER_LENGTH + indexToOffset(index + _readIndexAdjustment);
}