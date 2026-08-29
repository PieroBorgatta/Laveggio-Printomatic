#include "StoredZip.h"

void StoredZipWriter::write16(uint16_t value) {
  const uint8_t data[2] = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)};
  file_.write(data, sizeof(data));
}

void StoredZipWriter::write32(uint32_t value) {
  const uint8_t data[4] = {
    static_cast<uint8_t>(value),
    static_cast<uint8_t>(value >> 8),
    static_cast<uint8_t>(value >> 16),
    static_cast<uint8_t>(value >> 24)
  };
  file_.write(data, sizeof(data));
}

uint32_t StoredZipWriter::crc32(const uint8_t *data, size_t size) {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

bool StoredZipWriter::add(const String &name, const String &content) {
  if (!file_ || name.isEmpty() || name.length() > 65535 || content.length() > 0xFFFFFFFFU) return false;
  Entry entry{name, crc32(reinterpret_cast<const uint8_t *>(content.c_str()), content.length()),
              static_cast<uint32_t>(content.length()), static_cast<uint32_t>(file_.position())};
  write32(0x04034B50U);
  write16(20);
  write16(0);
  write16(0);
  write16(0);
  write16(0);
  write32(entry.crc32);
  write32(entry.size);
  write32(entry.size);
  write16(name.length());
  write16(0);
  file_.write(reinterpret_cast<const uint8_t *>(name.c_str()), name.length());
  file_.write(reinterpret_cast<const uint8_t *>(content.c_str()), content.length());
  entries_.push_back(entry);
  return true;
}

bool StoredZipWriter::finish() {
  if (!file_) return false;
  const uint32_t centralOffset = file_.position();
  for (const Entry &entry : entries_) {
    write32(0x02014B50U);
    write16(20);
    write16(20);
    write16(0);
    write16(0);
    write16(0);
    write16(0);
    write32(entry.crc32);
    write32(entry.size);
    write32(entry.size);
    write16(entry.name.length());
    write16(0);
    write16(0);
    write16(0);
    write16(0);
    write32(0);
    write32(entry.offset);
    file_.write(reinterpret_cast<const uint8_t *>(entry.name.c_str()), entry.name.length());
  }
  const uint32_t centralSize = static_cast<uint32_t>(file_.position()) - centralOffset;
  write32(0x06054B50U);
  write16(0);
  write16(0);
  write16(entries_.size());
  write16(entries_.size());
  write32(centralSize);
  write32(centralOffset);
  write16(0);
  file_.flush();
  return true;
}
