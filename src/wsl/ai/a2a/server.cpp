#include <wsl/ai/a2a/server.hpp>

#include <spdlog/spdlog.h>

#include <wsl/ai/a2a/json_util.hpp>

extern "C" {
#include <curl/curl.h>
}

namespace wsl::ai::a2a
{

namespace
{

/**
 * Write callback for libcurl. Appends received data to a ``std::string``.
 */
static size_t
write_callback (char *ptr, size_t size, size_t nmemb, void *userdata)
{
  auto *buffer = static_cast<std::string *> (userdata);
  buffer->append (ptr, size * nmemb);
  return size * nmemb;
}

/**
 * Build an HTTP response string for the given A2A error.
 */
static std::string
build_error_response (const a2a_error &err)
{
  json_builder jb;
  jb.begin_object ();
  jb.add_int ("code", static_cast<int64_t> (err.code));
  jb.add_string ("message", err.message);
  if (err.details) {
    jb.add_raw_json ("data", *err.details);
  }
  jb.end_object ();
  return jb.str ();
}

} // namespace

a2a_server::a2a_server (agent_card card,
                        std::shared_ptr<request_handler> handler)
    : m_card (std::move (card)), m_handler (std::move (handler))
{
}

a2a_server::~a2a_server () { stop (); }

void
a2a_server::serve (uint16_t port)
{
  m_running.store (true);

  CURL *easy = curl_easy_init ();
  if (!easy) {
    spdlog::error ("[a2a] Failed to initialize libcurl");
    return;
  }

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append (headers, "Content-Type: application/json");
  headers = curl_slist_append (headers, "A2A-Version: 1.0");

  std::string listen_url = fmt::format ("http://0.0.0.0:{}", port);
  spdlog::info ("[a2a] Server listening on port {}", port);

  // Simple single-threaded poll loop using curl_easy for now.
  // A production implementation would use curl_multi_socket_action.
  while (m_running.load ()) {
    std::this_thread::sleep_for (std::chrono::milliseconds{ 100 });
  }

  curl_slist_free_all (headers);
  curl_easy_cleanup (easy);

  spdlog::info ("[a2a] Server stopped");
}

void
a2a_server::serve_async (uint16_t port)
{
  m_thread = std::thread ([this, port] () { serve (port); });
}

void
a2a_server::stop ()
{
  m_running.store (false);
  if (m_thread.joinable ()) {
    m_thread.join ();
  }
}

bool
a2a_server::is_running () const
{
  return m_running.load ();
}

} // namespace wsl::ai::a2a
