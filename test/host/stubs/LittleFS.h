#pragma once

#include "Arduino.h"

class File
{
public:
  explicit operator bool() const { return false; }
  bool isDirectory() const { return false; }
  bool available() const { return false; }
  int read() const { return -1; }
  bool print(const char *) { return true; }
  void close() {}
};

namespace fs
{
  class FS
  {
  public:
    File open(const char *, const char *) { return File(); }
  };
}

class LittleFSClass : public fs::FS
{
public:
  bool begin(bool = false) { return true; }
};

inline LittleFSClass LittleFS;
