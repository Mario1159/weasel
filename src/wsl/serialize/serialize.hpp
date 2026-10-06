#pragma once

#include "types.hpp"
#include "adapters.hpp"

#ifndef IN_MODULE_INTERFACE
#include <rfl.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/json.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/AddTagsToVariants.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/msgpack.hpp>
#endif

#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace wsl::serialize
{

template <typename T>
inline std::string
json_write (const T &value, std::string *error = nullptr)
{
  try {
    return rfl::json::write (value);
  } catch (const std::exception &e) {
    if (error)
      *error = e.what ();
    return {};
  }
}

template <typename T>
inline bool
json_read (const std::string &json, T &out, std::string *error = nullptr)
{
  auto res = rfl::json::read<T> (json);
  if (!res) {
    if (error)
      *error = res.error ().what ();
    return false;
  }
  out = std::move (res.value ());
  return true;
}

template <typename T>
inline bool
json_read (std::string_view json, T &out, std::string *error = nullptr)
{
  return json_read (std::string (json), out, error);
}

/**
 * json_write_p / json_read_p: json_write/json_read with rfl processors.
 *
 * Materials pass rfl::AddNamespacedTagsToVariants so the std::variant inside
 * material_parameter is written with stable type tags. reflect-cpp's
 * default index-based variant encoding can drift across a round trip
 * (e.g. an int reads back as a float), silently corrupting parameters.
 */
template <typename... Ps>
inline std::string
json_write_p (const auto &value, std::string *error = nullptr)
{
  try {
    return rfl::json::write<Ps...> (value);
  } catch (const std::exception &e) {
    if (error)
      *error = e.what ();
    return {};
  }
}

template <typename T, typename... Ps>
inline bool
json_read_p (const std::string &json, T &out, std::string *error = nullptr)
{
  auto res = rfl::json::read<T, Ps...> (json);
  if (!res) {
    if (error)
      *error = res.error ().what ();
    return false;
  }
  out = std::move (res.value ());
  return true;
}

template <typename T>
inline std::vector<std::uint8_t>
msgpack_write (const T &value, std::string *error = nullptr)
{
  try {
    auto buf = rfl::msgpack::write (value);
    return { reinterpret_cast<const std::uint8_t *> (buf.data ()),
             reinterpret_cast<const std::uint8_t *> (buf.data ()
                                                     + buf.size ()) };
  } catch (const std::exception &e) {
    if (error)
      *error = e.what ();
    return {};
  }
}

template <typename T>
inline bool
msgpack_read (const std::vector<std::uint8_t> &bytes, T &out,
              std::string *error = nullptr)
{
  auto res = rfl::msgpack::read<T> (bytes.data (), bytes.size ());
  if (!res) {
    if (error)
      *error = res.error ().what ();
    return false;
  }
  out = std::move (res.value ());
  return true;
}

template <typename T>
inline bool
msgpack_read (const std::uint8_t *data, std::size_t size, T &out,
              std::string *error = nullptr)
{
  auto res = rfl::msgpack::read<T> (data, size);
  if (!res) {
    if (error)
      *error = res.error ().what ();
    return false;
  }
  out = std::move (res.value ());
  return true;
}

template <typename T>
inline void
save_field_if_diff (auto &obj, const char *name, const T &field, const T &def)
{
  (void)obj;
  (void)name;
  (void)field;
  (void)def;
}

} // namespace wsl::serialize