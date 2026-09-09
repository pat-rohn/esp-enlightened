#pragma once

#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <type_traits>

class String
{
public:
  String() = default;
  String(const char *value) : value_(value == nullptr ? "" : value) {}
  String(const std::string &value) : value_(value) {}
  String(char value) : value_(1, value) {}

  template <typename T,
            typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
  String(T value) : value_(std::to_string(static_cast<long long>(value)))
  {
  }

  const char *c_str() const { return value_.c_str(); }

  // requiresRestart() compares stored strings field by field, so the stub
  // needs real equality rather than falling back to pointer comparison.
  bool operator==(const String &other) const { return value_ == other.value_; }
  bool operator!=(const String &other) const { return value_ != other.value_; }

  size_t length() const { return value_.length(); }
  bool isEmpty() const { return value_.empty(); }
  int lastIndexOf(char value) const
  {
    const size_t index = value_.find_last_of(value);
    return index == std::string::npos ? -1 : static_cast<int>(index);
  }

  String substring(size_t from, size_t to) const
  {
    if (from >= value_.length())
    {
      return String();
    }
    return String(value_.substr(from, to - from));
  }

  int toInt() const
  {
    return static_cast<int>(std::strtol(value_.c_str(), nullptr, 10));
  }

  String &operator+=(const String &other)
  {
    value_ += other.value_;
    return *this;
  }

  friend String operator+(const String &left, const String &right)
  {
    return String(left.value_ + right.value_);
  }

  friend String operator+(const char *left, const String &right)
  {
    return String(std::string(left == nullptr ? "" : left) + right.value_);
  }

  friend bool operator==(const String &left, const String &right)
  {
    return left.value_ == right.value_;
  }

  friend bool operator==(const String &left, const char *right)
  {
    return left.value_ == (right == nullptr ? "" : right);
  }

private:
  std::string value_;
};

class HostSerial
{
public:
  template <typename T>
  void print(const T &)
  {
  }

  template <typename T>
  void println(const T &)
  {
  }

  void println()
  {
  }

  void printf(const char *, ...)
  {
  }
};

inline HostSerial Serial;

inline void delay(unsigned long)
{
}
