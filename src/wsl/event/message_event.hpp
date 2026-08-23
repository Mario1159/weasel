#pragma once

#include <type_traits>

namespace wsl::event
{

/**
 * Marker concept for buffered message payloads.
 *
 * A message event is a plain Weasel data struct (no registration required). It
 * must be trivially copyable so the `message_bus` can type-erase it into a byte
 * buffer and memcpy it back out for readers. Raw SDL types are never posted;
 * the SDL poll boundary translates them into engine message structs first.
 */
template <typename T>
concept message_event
    = std::is_trivially_copyable_v<T> && std::is_copy_constructible_v<T>;

} // namespace wsl::event
