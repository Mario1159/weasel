#pragma once

// Implementation-only serialization API (reflect-cpp backed).
//
// NEVER include this header (or types.hpp / adapters.hpp) from engine
// headers: engine headers are compiled into C++20 module purviews, and the
// serialization backends must stay implementation details of .cpp files.
// Engine headers forward-declare the writer/reader classes instead.
//
// Consumers: reg/*.cpp, rsc/*.cpp, and any other implementation unit that
// saves or loads engine data.

#include <string>
#include <string_view>
#include <vector>

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <rfl/msgpack.hpp>

#include "adapters.hpp"
#include "types.hpp"

namespace wsl
{
namespace serialize
{

/** Serializes value as JSON under key into the writer. */
template <typename T>
inline void
json_write (json_writer &writer, std::string_view key, const T &value)
{
  writer.attach_json (key, rfl::json::write (value));
}

/**
 * Deserializes value from the JSON subtree under key.
 * :return: false (value untouched) when the key is absent or malformed.
 */
template <typename T>
inline bool
json_read (json_reader &reader, std::string_view key, T &value)
{
  std::string sub;
  if (!reader.extract_json (key, sub)) {
    return false;
  }

  auto result = rfl::json::read<T> (sub);
  if (!result) {
    return false;
  }

  value = std::move (result).value ();
  return true;
}

/** Serializes value as a self-describing msgpack blob into the stream. */
template <typename T>
inline void
msgpack_write (binary_writer &writer, const T &value)
{
  const std::vector<char> bytes = rfl::msgpack::write (value);
  writer.write_blob (bytes.data (), bytes.size ());
}

/**
 * Deserializes value from a msgpack blob in the stream.
 * :return: false (value untouched) when the blob is absent or malformed.
 */
template <typename T>
inline bool
msgpack_read (binary_reader &reader, T &value)
{
  std::string blob;
  if (!reader.read_blob (blob)) {
    return false;
  }

  auto result = rfl::msgpack::read<T> (blob);
  if (!result) {
    return false;
  }

  value = std::move (result).value ();
  return true;
}

} // namespace serialize

} // namespace wsl
