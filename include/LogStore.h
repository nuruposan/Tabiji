#pragma once

#include <Arduino.h>
#include <LittleFS.h>

class LogStore {
 public:
  static const uint32_t HEADER_LENGTH = 128;                // Size of the header in bytes
  static const uint32_t MAGIC_NUMBER_DEFAULT = 0x12345678;  // Default magic number for validating log file integrity
  static const uint8_t BUFFER_DEPTH_DEFAULT = 8;            // Default cache depth (entries) for read/write buffers
  static const uint32_t DUMP_ENTRY_LIMIT_DEFAULT = 4;       // Limit the number of entries to dump from log file
  static const uint8_t CHECKSUM_FIELD_LENGTH = 2;           // Length of the checksum field in bytes
  static const char CHECKSUM_DELIMITER = '*';               // Delimiter character for separating checksum field

  const char *DS_FILENAME = "/data.bin";  // File name for storing log entries

 private:
  // Log file metadata
  const uint32_t _magicNumber = MAGIC_NUMBER_DEFAULT;  // Magic number for validating log file integrity
  uint32_t _dataSize = 0;                              // Size of the actual data in each log entry (excluding checksum)
  uint32_t _entrySize = 0;   // Size of each log entry in bytes on storage (including checksum)
  uint32_t _entryCount = 0;  // Current number of log entries

  // Write buffer and related variables
  void *_writeBuffer = nullptr;                      // Buffer for writing log entries
  uint8_t _writeBufferDepth = BUFFER_DEPTH_DEFAULT;  // Cache size for buffering log entries
  uint8_t _entriesBuffered = 0;                      // Number of log entries currently buffered for writing

  // Read buffer and related variables
  void *_readBuffer = nullptr;                      // Buffer for reading log entries
  uint8_t _readBufferDepth = BUFFER_DEPTH_DEFAULT;  // Cache size for reading log entries
  int32_t _readStartIndex = -1;                     // Start index for reading log entries
  int8_t _readBufferPos = 0;                        // Current position in the read buffer
  uint32_t _readOffsetAdjustment = 0;               // Bytes skipped while resynchronizing corrupted entries

  // Callback functions for log store events
  void (*_onLogStoreBegin)() = nullptr;  // Callback function for log store begin event
  void (*_onLogStoreReset)() = nullptr;  // Callback function for log store reset event
  void (*_onLogStoreFull)() = nullptr;   // Callback function for log store full event
  void (*_onEntryAppend)() = nullptr;    // Callback function for log store append event
  void (*_onEntryFlush)() = nullptr;     // Callback function for log store flush event

  void dumpWriteBuffer();
  uint32_t indexToOffset(uint32_t index) const;
  uint32_t readIndexToOffset(uint32_t index) const;
  bool fetch(uint32_t startIndex);

 public:
  LogStore(uint32_t magicNumber = MAGIC_NUMBER_DEFAULT);
  ~LogStore();

  bool begin(uint8_t entrySize,                       //
      uint8_t writeCacheSize = BUFFER_DEPTH_DEFAULT,  //
      uint8_t readCacheSize = BUFFER_DEPTH_DEFAULT);
  void end();
  void dump(bool headerDump = true,  //
      uint32_t entriesToDump = DUMP_ENTRY_LIMIT_DEFAULT);
  bool append(void *data);
  bool reset();
  void *readFirst(bool flushBuffer = true);
  void *readNext();
  bool flush();
  uint32_t entryCount() const;
};
