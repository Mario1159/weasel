#pragma once

#ifndef IN_MODULE_INTERFACE
#include <rfl/json.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/msgpack.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/Result.hpp>
#endif

#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string_view>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstdint>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstddef>
#endif

namespace wsl::serialize
{

namespace detail
{

/**
 * Returns the byte size of the first complete msgpack object in
 * \p data, or 0 if the object is malformed or truncated.
 *
 * The scene serializers stream several msgpack documents into one
 * buffer, so the reader must know where each document ends.
 */
inline std::size_t
msgpack_object_size (const std::uint8_t *data, std::size_t size)
{
  std::size_t pos = 0;
  std::uint64_t remaining = 1;

  auto read_len = [&] (std::size_t n) -> std::uint64_t {
    if (pos + n > size) {
      pos = size + 1;
      return 0;
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < n; ++i) {
      value = (value << 8) | static_cast<std::uint64_t> (data[pos + i]);
    }
    pos += n;
    return value;
  };

  while (remaining > 0) {
    if (pos >= size) {
      return 0;
    }
    const std::uint8_t b = data[pos++];

    if (b <= 0x7f || b >= 0xe0) {
      // positive / negative fixint
    } else if (b == 0xc0 || b == 0xc2 || b == 0xc3) {
      // nil / false / true
    } else if ((b & 0xf0) == 0x80) {
      remaining += static_cast<std::uint64_t> (b & 0x0f) * 2; // fixmap
    } else if ((b & 0xf0) == 0x90) {
      remaining += (b & 0x0f); // fixarray
    } else if ((b & 0xe0) == 0xa0) {
      const std::size_t n = (b & 0x1f); // fixstr
      if (n > size || pos > size - n) {
        return 0;
      }
      pos += n;
    } else {
      std::uint64_t children = 0;
      std::uint64_t bytes = 0;
      switch (b) {
      case 0xc1: // never used
        return 0;
      case 0xc4: // bin 8
      case 0xd9: // str 8
        bytes = read_len (1);
        break;
      case 0xc5: // bin 16
      case 0xda: // str 16
        bytes = read_len (2);
        break;
      case 0xc6: // bin 32
      case 0xdb: // str 32
        bytes = read_len (4);
        break;
      case 0xc7: // ext 8 (length + 1 type byte)
      case 0xc8: // ext 16
      case 0xc9: {
        bytes = read_len (std::size_t{ 1 } << (b - 0xc7)) + 1;
        break;
      }
      case 0xca:
        bytes = 4;
        break; // float 32
      case 0xcb:
        bytes = 8;
        break; // float 64
      case 0xcc:
        bytes = 1;
        break; // uint 8
      case 0xcd:
        bytes = 2;
        break; // uint 16
      case 0xce:
        bytes = 4;
        break; // uint 32
      case 0xcf:
        bytes = 8;
        break; // uint 64
      case 0xd0:
        bytes = 1;
        break; // int 8
      case 0xd1:
        bytes = 2;
        break; // int 16
      case 0xd2:
        bytes = 4;
        break; // int 32
      case 0xd3:
        bytes = 8;
        break; // int 64
      case 0xd4:
        bytes = 2;
        break; // fixext 1
      case 0xd5:
        bytes = 3;
        break; // fixext 2
      case 0xd6:
        bytes = 5;
        break; // fixext 4
      case 0xd7:
        bytes = 9;
        break; // fixext 8
      case 0xd8:
        bytes = 17;
        break; // fixext 16
      case 0xdc:
        children = read_len (2); // array 16
        break;
      case 0xdd:
        children = read_len (4); // array 32
        break;
      case 0xde:
        children = read_len (2) * 2; // map 16
        break;
      case 0xdf:
        children = read_len (4) * 2; // map 32
        break;
      default:
        return 0;
      }
      if (pos > size || bytes > size - pos) {
        return 0;
      }
      pos += static_cast<std::size_t> (bytes);
      --remaining;
      remaining += children;
      continue;
    }
    --remaining;
  }
  return pos;
}

/**
 * Returns the index one past the end of the first complete JSON value
 * starting at \p start, or npos if the value is unterminated.
 */
inline std::size_t
json_value_end (std::string_view s, std::size_t start)
{
  const char open = s[start];
  if (open == '"') {
    std::size_t i = start + 1;
    while (i < s.size ()) {
      if (s[i] == '\\') {
        i += 2;
        continue;
      }
      if (s[i] == '"') {
        return i + 1;
      }
      ++i;
    }
    return std::string_view::npos;
  }
  if (open != '{' && open != '[') {
    // scalar: number, true, false, null
    std::size_t i = start;
    while (i < s.size ()) {
      const char c = s[i];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ','
          || c == '}' || c == ']') {
        break;
      }
      ++i;
    }
    return i;
  }
  int depth = 0;
  bool in_string = false;
  std::size_t i = start;
  while (i < s.size ()) {
    const char c = s[i];
    if (in_string) {
      if (c == '\\') {
        i += 2;
        continue;
      }
      if (c == '"') {
        in_string = false;
      }
      ++i;
      continue;
    }
    if (c == '"') {
      in_string = true;
    } else if (c == '{' || c == '[') {
      ++depth;
    } else if (c == '}' || c == ']') {
      --depth;
      if (depth == 0) {
        return i + 1;
      }
    }
    ++i;
  }
  return std::string_view::npos;
}

} // namespace detail

struct binary_writer
{
  std::vector<std::uint8_t> bytes;

  template <typename T>
  bool
  write (const T &value, std::string *error = nullptr)
  {
    try {
      auto buf = rfl::msgpack::write (value);
      bytes.insert (bytes.end (), buf.begin (), buf.end ());
      return true;
    } catch (const std::exception &e) {
      if (error)
        *error = e.what ();
      return false;
    }
  }
};

struct binary_reader
{
  const std::uint8_t *data = nullptr;
  std::size_t size = 0;

  explicit binary_reader (const std::vector<std::uint8_t> &buf)
      : data (buf.data ()), size (buf.size ())
  {
  }
  explicit binary_reader (const std::uint8_t *d, std::size_t s)
      : data (d), size (s)
  {
  }

  template <typename T>
  bool
  read (T &out, std::string *error = nullptr)
  {
    std::size_t const n = detail::msgpack_object_size (data, size);
    if (n == 0) {
      if (error)
        *error = "invalid or empty msgpack stream";
      return false;
    }
    auto res = rfl::msgpack::read<T> (data, n);
    data += n;
    size -= n;
    if (!res) {
      if (error)
        *error = res.error ().what ();
      return false;
    }
    out = std::move (res.value ());
    return true;
  }
};

/**
 * Accumulates newline-delimited JSON documents. The scene serializers
 * stream several documents into one writer, so each write appends
 * instead of replacing.
 */
struct json_writer
{
  std::string json;

  template <typename T>
  bool
  write (const T &value, std::string *error = nullptr)
  {
    try {
      std::string const doc = rfl::json::write (value);
      if (!json.empty ()) {
        json += '\n';
      }
      json += doc;
      return true;
    } catch (const std::exception &e) {
      if (error)
        *error = e.what ();
      return false;
    }
  }
};

/** Reads one JSON document at a time from a stream written by json_writer. */
struct json_reader
{
  std::string_view json;

  explicit json_reader (std::string_view j) : json (j) {}

  template <typename T>
  bool
  read (T &out, std::string *error = nullptr)
  {
    std::size_t const start = json.find_first_not_of (" \t\r\n");
    if (start == std::string_view::npos) {
      if (error)
        *error = "no more documents in JSON stream";
      return false;
    }
    std::size_t const end = detail::json_value_end (json, start);
    if (end == std::string_view::npos) {
      if (error)
        *error = "unterminated JSON document";
      return false;
    }
    std::string_view const doc = json.substr (start, end - start);
    json.remove_prefix (end);
    auto res = rfl::json::read<T> (doc);
    if (!res) {
      if (error)
        *error = res.error ().what ();
      return false;
    }
    out = std::move (res.value ());
    return true;
  }
};

} // namespace wsl::serialize

// Now include adapters and component adapters so parser specializations
// are visible wherever serialize.hpp is included. (types.hpp alone does
// NOT pull these in to avoid header cycles with component_registry.hpp.)
#include "adapters.hpp"
