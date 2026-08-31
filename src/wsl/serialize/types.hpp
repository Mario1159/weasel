#pragma once

// Implementation-only serialization primitives.
//
// This header (and serialize.hpp, which extends it) must NEVER be included
// from engine headers: they are compiled into module interface purviews, and
// the serialization backends (yyjson/reflect-cpp) must remain implementation
// details of .cpp files. Engine headers only forward-declare the classes
// below for function-pointer signatures.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <yyjson.h>

namespace wsl
{
namespace serialize
{

/** Length-prefixed little-endian binary stream writer. */
class binary_writer
{
public:
  /** Appends raw bytes without any framing. */
  void
  write_bytes (const void *data, std::size_t size)
  {
    m_buffer.append (static_cast<const char *> (data), size);
  }

  /** Appends a trivially copyable value (arithmetic types, enums, PODs). */
  template <typename T>
    requires std::is_trivially_copyable_v<T>
  void
  write (const T &value)
  {
    write_bytes (&value, sizeof (T));
  }

  /** Appends a u64 length followed by the string bytes. */
  void
  write_string (std::string_view str)
  {
    const std::uint64_t size = str.size ();
    write (size);
    write_bytes (str.data (), str.size ());
  }

  /** Appends a u64 length followed by the payload bytes. */
  void
  write_blob (const void *data, std::size_t size)
  {
    const std::uint64_t n = size;
    write (n);
    write_bytes (data, size);
  }

  /** Appends a u64 length followed by the payload bytes. */
  void
  write_blob (const std::string &bytes)
  {
    write_blob (bytes.data (), bytes.size ());
  }

  /** Returns the accumulated buffer. */
  const std::string &
  buffer () const
  {
    return m_buffer;
  }

  /** Resets the buffer. */
  void
  clear ()
  {
    m_buffer.clear ();
  }

private:
  std::string m_buffer;
};

/** Reader for streams produced by binary_writer. */
class binary_reader
{
public:
  binary_reader (const char *data, std::size_t size)
      : m_data (data), m_size (size)
  {
  }

  explicit binary_reader (const std::string &buffer)
      : binary_reader (buffer.data (), buffer.size ())
  {
  }

  /** Reads raw bytes; returns false when the stream is exhausted. */
  bool
  read_bytes (void *out, std::size_t size)
  {
    if (m_offset + size > m_size) {
      return false;
    }
    std::memcpy (out, m_data + m_offset, size);
    m_offset += size;
    return true;
  }

  /** Reads a trivially copyable value. */
  template <typename T>
    requires std::is_trivially_copyable_v<T>
  bool
  read (T &value)
  {
    return read_bytes (&value, sizeof (T));
  }

  /** Reads a u64 length followed by the string bytes. */
  bool
  read_string (std::string &out)
  {
    std::uint64_t size = 0;
    if (!read (size)) {
      return false;
    }
    if (m_offset + size > m_size) {
      return false;
    }
    out.assign (m_data + m_offset, static_cast<std::size_t> (size));
    m_offset += size;
    return true;
  }

  /** Alias of read_string. */
  bool
  read_blob (std::string &out)
  {
    return read_string (out);
  }

  /** Returns true when every byte has been consumed. */
  bool
  at_end () const
  {
    return m_offset == m_size;
  }

private:
  const char *m_data = nullptr;
  std::size_t m_size = 0;
  std::size_t m_offset = 0;
};

/**
 * JSON document writer with object/array composition.
 *
 * Values serialized through reflect-cpp arrive as JSON strings and are
 * parsed + grafted into the document (see serialize.hpp).
 */
class json_writer
{
public:
  json_writer ();
  ~json_writer ();

  json_writer (const json_writer &) = delete;
  json_writer &operator= (const json_writer &) = delete;

  /** Starts a named child object of the current node. */
  void begin_object (std::string_view key);

  /** Starts an anonymous child object of the current array. */
  void begin_element_object ();

  /** Ends the current object. */
  void end_object ();

  /** Starts a named child array of the current node. */
  void begin_array (std::string_view key);

  /** Ends the current array. */
  void end_array ();

  /** Writes a u64 scalar into the current object. */
  void write_u64 (std::string_view key, std::uint64_t value);

  /** Writes a double scalar into the current object. */
  void write_double (std::string_view key, double value);

  /** Writes a boolean scalar into the current object. */
  void write_bool (std::string_view key, bool value);
  /** Writes a string scalar into the current object. */
  void write_string (std::string_view key, std::string_view value);

  /** Appends a u64 element to the current array. */
  void append_u64 (std::uint64_t value);

  /**
   * Parses a JSON string and grafts it into the current object under key.
   * :param key: Object key for the grafted value.
   * :param json: A complete JSON document (e.g. from rfl::json::write).
   */
  void attach_json (std::string_view key, const std::string &json);

  /**
   * Parses a JSON string and appends it as the next element of the current
   * array.
   */
  void append_element_json (const std::string &json);

  /** Returns the document serialized to a JSON string. */
  std::string to_string () const;

private:
  yyjson_mut_val *current ();
  yyjson_mut_val *make_key (std::string_view key);
  yyjson_mut_val *graft_string (const std::string &json);

  yyjson_mut_doc *m_doc = nullptr;
  std::vector<yyjson_mut_val *> m_stack;
};

/**
 * JSON document reader with navigation.
 *
 * Subtrees are handed to reflect-cpp as re-serialized JSON strings
 * (see serialize.hpp).
 */
class json_reader
{
public:
  /** Parses the document; check valid() afterwards. */
  explicit json_reader (const std::string &json);
  ~json_reader ();

  json_reader (const json_reader &) = delete;
  json_reader &operator= (const json_reader &) = delete;

  /** Returns true when the document was parsed successfully. */
  bool valid () const;

  /** Enters the object stored under key; returns false when absent. */
  bool enter_object (std::string_view key);

  /** Enters the array stored under key; returns false when absent. */
  bool enter_array (std::string_view key);

  /** Enters element index of the current array. */
  bool enter_element (std::size_t index);
  /** Returns the number of elements of the array under key (0 if absent). */
  std::size_t array_size (std::string_view key) const;

  /** Reads the u64 element at index of the current array. */
  bool element_u64 (std::size_t index, std::uint64_t &out) const;

  /** Leaves the current node. */
  void leave ();

  /** Reads a u64 scalar; returns false when absent. */
  bool read_u64 (std::string_view key, std::uint64_t &out) const;

  /** Reads a double scalar; returns false when absent. */
  bool read_double (std::string_view key, double &out) const;

  /** Reads a boolean scalar; returns false when absent. */
  bool read_bool (std::string_view key, bool &out) const;

  /** Reads a string scalar; returns false when absent. */
  bool read_string (std::string_view key, std::string &out) const;

  /**
   * Extracts the subtree under key re-serialized as a JSON string.
   * Returns false when the key is absent.
   */
  bool extract_json (std::string_view key, std::string &out) const;

private:
  yyjson_val *current () const;
  yyjson_val *find (std::string_view key) const;

  yyjson_doc *m_doc = nullptr;
  std::vector<yyjson_val *> m_stack;
};

} // namespace serialize

} // namespace wsl
