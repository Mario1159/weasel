#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace wsl::ai::acp
{

/**
 * JSON-RPC 2.0 client over stdio (stdin/stdout).
 *
 * Manages an agent subprocess and provides synchronous request/response
 * plus asynchronous notification handling.
 *
 * Messages are newline-delimited JSON-RPC as required by ACP v1.
 */
class acp_client
{
public:
  using notification_handler = std::function<void (const std::string &method,
                                                   const std::string &params)>;

  acp_client ();
  ~acp_client ();

  acp_client (const acp_client &) = delete;
  acp_client &operator= (const acp_client &) = delete;

  /**
   * Launch an agent subprocess and connect via stdio pipes.
   *
   * :param command: Path to the agent executable.
   * :param args: Command-line arguments.
   * :return: true on success.
   */
  bool launch_agent (const std::string &command,
                     const std::vector<std::string> &args);

  /**
   * Send a JSON-RPC request and block until the response arrives.
   *
   * :param method: The RPC method name.
   * :param params_json: Serialized JSON params (or "{}").
   * :return: The result JSON string, or empty on error.
   */
  std::string send_request (const std::string &method,
                            const std::string &params_json);

  /**
   * Send a JSON-RPC notification (no response expected).
   *
   * :param method: The notification method name.
   * :param params_json: Serialized JSON params.
   */
  void send_notification (const std::string &method,
                          const std::string &params_json);

  /**
   * Set the handler for incoming notifications from the agent.
   */
  void set_notification_handler (notification_handler handler);

  /**
   * Check if the agent process is alive.
   */
  bool is_running () const;

  /**
   * Terminate the agent process and close pipes.
   */
  void terminate ();

private:
  void read_loop ();
  void dispatch_message (const std::string &line);
  void write_line (const std::string &line);

  // Pipe file descriptors
  int m_stdin_fd = -1;
  int m_stdout_fd = -1;

  // Agent process
  int m_agent_pid = -1;

  // Background reader
  std::thread m_read_thread;
  std::atomic<bool> m_running{ false };

  // Write synchronization
  std::mutex m_write_mutex;

  // Notification handler
  notification_handler m_on_notification;

  // Pending request responses
  struct pending_request
  {
    std::string result;
    std::string error;
    bool completed = false;
  };

  std::mutex m_pending_mutex;
  std::condition_variable m_pending_cv;
  std::unordered_map<int64_t, std::shared_ptr<pending_request>> m_pending;

  // Auto-incrementing request ID
  std::atomic<int64_t> m_next_id{ 1 };
};

} // namespace wsl::ai::acp
