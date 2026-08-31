#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <wsl/ai/a2a/agent_card.hpp>
#include <wsl/ai/a2a/transport.hpp>

namespace wsl::ai::a2a
{

/**
 * A2A server.
 *
 * Hosts an agent card and dispatches incoming requests to the provided
 * handler. The server uses libcurl's socket action API for non-blocking
 * HTTP I/O.
 *
 * Example::
 *
 *   a2a_server server (card, handler);
 *   server.serve (8080);      // blocking
 *   // or
 *   server.serve_async (8080); // background thread
 *   // ...
 *   server.stop ();
 */
class a2a_server
{
public:
  /**
   * Creates a server with the given agent card and request handler.
   *
   * :param card: The agent card served at ``/.well-known/agent-card.json``.
   * :param handler: The handler that processes A2A requests.
   */
  a2a_server (agent_card card, std::shared_ptr<request_handler> handler);

  ~a2a_server ();

  a2a_server (const a2a_server &) = delete;
  a2a_server &operator= (const a2a_server &) = delete;

  /**
   * Start serving on the given port. Blocks until ``stop()`` is called.
   *
   * :param port: TCP port to listen on.
   */
  void serve (uint16_t port);

  /**
   * Start serving in a background thread.
   *
   * :param port: TCP port to listen on.
   */
  void serve_async (uint16_t port);

  /** Signal the server to stop. Thread-safe. */
  void stop ();

  /** Returns ``true`` while the server is running. */
  bool is_running () const;

private:
  agent_card m_card;
  std::shared_ptr<request_handler> m_handler;
  std::atomic<bool> m_running{ false };
  std::thread m_thread;
};

} // namespace wsl::ai::a2a
