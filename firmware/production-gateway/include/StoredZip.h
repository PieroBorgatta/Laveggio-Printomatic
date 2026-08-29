#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

class StoredZipWriter {
 public:
  explicit StoredZipWriter(File &file) : file_(file) {}
  bool add(const String &name, const String &content);
  bool finish();

 private:
  struct Entry {
    String name;
    uint32_t crc32;
    uint32_t size;
    uint32_t offset;
  };

  File &file_;
  std::vector<Entry> entries_;
  void write16(uint16_t value);
  void write32(uint32_t value);
  static uint32_t crc32(const uint8_t *data, size_t size);
};
