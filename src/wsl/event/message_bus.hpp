#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "message_event.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "../comp/component_meta.hpp"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <cstddef>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <cstring>
#endif
// See wsl/math/vector.hpp for why EnTT is skipped inside a module build.
#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <functional>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <mutex>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <unordered_map>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <unordered_set>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <vector>
#endif

namespace wsl::event
{

/**
 * Type-erased event envelope used only as internal staging while a message
 * sits in the per-type reader buffer. It is never posted directly by users;
 * consumers holding a `message_reader<message>` (catch-all) get these.
 */
struct message
{
  entt::id_type type_id{};
  const void *data = nullptr;
};

namespace detail
{

struct erased_buffer
{
  std::vector<std::byte> bytes;
  std::size_t element_size = 0;
  std::uint64_t generation = 0;
};

} // namespace detail

template <typename Event> class message_reader;

/**
 * Thread-safe, pull-only buffered event bus.
 *
 * Unlike the observer `event_hub`, the bus performs no wiring and no broadcast.
 * Producers `post<T>(...)` typed messages; the per-frame `drain` publishes the
 * pending queue into stable per-type buffers; consumers hold a
 * `message_reader<T>` and pull messages during their own update. Each reader
 * keeps its own cursor, so multiple systems each receive every message of a
 * type exactly once.
 */
class message_bus
{
public:
  template <typename Event>
  void
  post (Event event)
  {
    static_assert (message_event<Event>,
                   "Message payload must satisfy wsl::event::message_event");

    const entt::id_type id = comp::stable_type_id<Event> ();

    std::lock_guard<std::mutex> lock (m_mutex);
    detail::erased_buffer &buf = m_pending[id];
    if (buf.element_size == 0) {
      buf.element_size = sizeof (Event);
    }

    const std::size_t offset = buf.bytes.size ();
    buf.bytes.resize (offset + sizeof (Event));
    std::memcpy (buf.bytes.data () + offset, &event, sizeof (Event));
  }

  /**
   * Type-erased variant of :cpp:func:`post` for name-keyed bindings.
   *
   * The element size latches on the first post for a type. A later post whose
   * payload size differs from the latched element size is rejected to protect
   * the buffer from silent corruption.
   *
   * :param type_id: Stable message type identifier.
   * :param data: Payload bytes to copy.
   * :param size: Payload size in bytes.
   * :return: ``true`` if the message was queued, ``false`` when the size does
   *   not match a previously latched element size.
   */
  bool
  post_erased (entt::id_type type_id, const void *data, std::size_t size)
  {
    if (data == nullptr || size == 0) {
      return false;
    }

    std::lock_guard<std::mutex> lock (m_mutex);
    detail::erased_buffer &buf = m_pending[type_id];
    if (buf.element_size == 0) {
      buf.element_size = size;
    }

    if (buf.element_size != size) {
      return false;
    }

    const std::size_t offset = buf.bytes.size ();
    buf.bytes.resize (offset + size);
    std::memcpy (buf.bytes.data () + offset, data, size);
    return true;
  }

  /** Returns a typed reader bound to this bus. */
  template <typename Event>
  message_reader<Event>
  reader ()
  {
    return message_reader<Event>{ *this };
  }

  /** Subscribes a typed callback, delivered once per published message. */
  template <typename Event>
  void
  subscribe (std::function<void (const Event &)> cb)
  {
    const entt::id_type id = comp::stable_type_id<Event> ();
    std::lock_guard<std::mutex> lock (m_mutex);
    m_typed_subs[id].push_back ([cb = std::move (cb)] (const message &m) {
      Event value;
      std::memcpy (&value, m.data, sizeof (Event));
      cb (value);
    });
  }

  /** Subscribes a catch-all callback that receives every published message. */
  void
  subscribe (std::function<void (const message &)> cb)
  {
    std::lock_guard<std::mutex> lock (m_mutex);
    m_catch_all_subs.push_back (std::move (cb));
  }

  /**
   * Publishes the pending queue into the per-type reader buffers and delivers
   * any `subscribe` callbacks, then clears the pending queue. Call once per
   * frame, right after the SDL poll loop. Events posted during `drain` roll to
   * the next frame.
   */
  void
  drain (entt::registry & /*registry*/)
  {
    std::unordered_map<entt::id_type, detail::erased_buffer> pending;
    std::vector<std::function<void (const message &)>> catch_all;
    std::unordered_map<entt::id_type,
                       std::vector<std::function<void (const message &)>>>
        typed;

    {
      std::lock_guard<std::mutex> lock (m_mutex);
      pending = std::move (m_pending);
      m_pending.clear ();
      catch_all = m_catch_all_subs;
      typed = m_typed_subs;
    }

    for (auto &[id, buf] : pending) {
      detail::erased_buffer &pub = m_published[id];
      pub.bytes = std::move (buf.bytes);
      pub.element_size = buf.element_size;
      ++pub.generation;
    }

    // Drop published buffers for types that received no message this frame.
    // Without this, a one-shot event (e.g. mouse-button-down) would persist in
    // the published buffer and be re-delivered on every subsequent frame until
    // a newer message of the same type arrives.
    for (auto &[id, pub] : m_published) {
      if (pending.find (id) == pending.end ()) {
        pub.bytes.clear ();
      }
    }

    for (auto &[id, pub] : m_published) {
      if (pub.bytes.empty () || pub.element_size == 0) {
        continue;
      }

      const std::size_t count = pub.bytes.size () / pub.element_size;

      auto tit = typed.find (id);
      if (tit != typed.end ()) {
        for (std::size_t i = 0; i < count; ++i) {
          const message m{ id, pub.bytes.data () + i * pub.element_size };
          for (const auto &cb : tit->second) {
            cb (m);
          }
        }
      }

      for (const auto &cb : catch_all) {
        for (std::size_t i = 0; i < count; ++i) {
          const message m{ id, pub.bytes.data () + i * pub.element_size };
          cb (m);
        }
      }
    }
  }

  /** Internal: returns the published buffer for a type, or nullptr. */
  detail::erased_buffer *
  find_published (entt::id_type type_id)
  {
    auto it = m_published.find (type_id);
    return it == m_published.end () ? nullptr : &it->second;
  }

  /** Internal: returns the published buffer map (catch-all reader iteration).
   */
  const std::unordered_map<entt::id_type, detail::erased_buffer> &
  published_buffers () const
  {
    return m_published;
  }

private:
  std::mutex m_mutex;
  std::unordered_map<entt::id_type, detail::erased_buffer> m_pending;
  std::unordered_map<entt::id_type, detail::erased_buffer> m_published;
  std::unordered_map<entt::id_type,
                     std::vector<std::function<void (const message &)>>>
      m_typed_subs;
  std::vector<std::function<void (const message &)>> m_catch_all_subs;
};

template <typename Event> class message_reader
{
public:
  explicit message_reader (message_bus &bus) : m_bus (bus) {}

  /** Invokes `fn` for every message of `Event` published this frame. */
  template <typename F>
  void
  for_each (F &&fn)
  {
    const entt::id_type id = comp::stable_type_id<Event> ();
    detail::erased_buffer *buf = m_bus.find_published (id);

    if (buf == nullptr || buf->bytes.empty () || buf->element_size == 0) {
      m_generation = buf ? buf->generation : 0;
      m_cursor = 0;
      return;
    }

    if (m_generation != buf->generation) {
      m_cursor = 0;
      m_generation = buf->generation;
    }

    const std::size_t count = buf->bytes.size () / buf->element_size;
    for (std::size_t i = m_cursor; i < count; ++i) {
      Event value;
      std::memcpy (&value, buf->bytes.data () + i * buf->element_size,
                   buf->element_size);
      fn (value);
    }
    m_cursor = count;
  }

private:
  message_bus &m_bus;
  std::uint64_t m_generation = 0;
  std::size_t m_cursor = 0;
};

/** Catch-all reader: iterates every published message of every type. */
template <> class message_reader<message>
{
public:
  explicit message_reader (message_bus &bus) : m_bus (bus) {}

  template <typename F>
  void
  for_each (F &&fn)
  {
    for (const auto &[id, buf] : m_bus.published_buffers ()) {
      if (buf.bytes.empty () || buf.element_size == 0) {
        continue;
      }

      const std::size_t count = buf.bytes.size () / buf.element_size;
      for (std::size_t i = 0; i < count; ++i) {
        const message m{ id, buf.bytes.data () + i * buf.element_size };
        fn (m);
      }
    }
  }

private:
  message_bus &m_bus;
};

} // namespace wsl::event
