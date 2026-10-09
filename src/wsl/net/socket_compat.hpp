#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace wsl::net
{

// Opaque socket handle: a POSIX fd on unix, a Winsock SOCKET (UINT_PTR) on
// Windows. Sized as uintptr_t so it carries either without truncation.
// Centralizes every platform difference (headers, close/send/recv,
// WSAStartup, error text) so editor client/server stay ifdef-free.
#ifdef _WIN32
using socket_handle = std::uintptr_t;
inline constexpr socket_handle invalid_socket
    = static_cast<socket_handle> (~std::uintptr_t (0));
#else
using socket_handle = int;
inline constexpr socket_handle invalid_socket = -1;
#endif

inline bool
is_valid_socket (socket_handle s)
{
  return s != invalid_socket;
}

// One-time socket runtime setup (WSAStartup on Windows; no-op elsewhere).
// Safe to call repeatedly; invoked by the open helpers below.
void ensure_socket_runtime ();

// Close a socket (closesocket/close); no-op for invalid_socket.
void close_socket (socket_handle s);

// Blocking send/recv wrappers. Return bytes transferred, or -1 on error
// (0 from recv means orderly shutdown). Lengths here are small (lines).
long socket_send (socket_handle s, const char *data, std::size_t len);
long socket_recv (socket_handle s, char *data, std::size_t len);

// Human-readable text for the last socket error on this thread.
std::string socket_error_string ();

// Connect a stream socket to the AF_UNIX socket at path. Returns
// invalid_socket on failure (details via socket_error_string).
socket_handle socket_open_client (const std::string &socket_path);

// Create, bind and listen an AF_UNIX stream socket at path. Returns
// invalid_socket on failure.
socket_handle socket_open_server (const std::string &socket_path);

// Non-blocking accept: polls briefly, then accepts one pending connection.
// Returns invalid_socket when none is pending or on error.
socket_handle socket_try_accept (socket_handle server);

} // namespace wsl::net
