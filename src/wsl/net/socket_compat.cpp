#include "wsl/net/socket_compat.hpp"

#ifdef _WIN32
// WIN32_LEAN_AND_MEAN + NOMINMAX (see editor_context.cpp). Unlike that TU,
// near/far must STAY defined here: FD_ZERO/FD_SET expand through FAR, and
// undefining it breaks them. This TU has no .near()/.far() member uses.
// (winsock2.h pulls in windows.h internally.)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <afunix.h>
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <mutex>

namespace wsl::net
{

void
ensure_socket_runtime ()
{
#ifdef _WIN32
  static std::once_flag flag;
  std::call_once (flag, [] {
    WSADATA data{};
    WSAStartup (MAKEWORD (2, 2), &data);
  });
#endif
}

void
close_socket (socket_handle s)
{
  if (!is_valid_socket (s)) {
    return;
  }
#ifdef _WIN32
  ::closesocket (static_cast<SOCKET> (s));
#else
  ::close (static_cast<int> (s));
#endif
}

long
socket_send (socket_handle s, const char *data, std::size_t len)
{
#ifdef _WIN32
  const int n
      = ::send (static_cast<SOCKET> (s), data, static_cast<int> (len), 0);
#else
  const ssize_t n = ::send (static_cast<int> (s), data, len, 0);
#endif
  if (n < 0) {
    return -1;
  }
  return static_cast<long> (n);
}

long
socket_recv (socket_handle s, char *data, std::size_t len)
{
#ifdef _WIN32
  const int n
      = ::recv (static_cast<SOCKET> (s), data, static_cast<int> (len), 0);
#else
  const ssize_t n = ::recv (static_cast<int> (s), data, len, 0);
#endif
  if (n < 0) {
    return -1;
  }
  return static_cast<long> (n);
}

std::string
socket_error_string ()
{
#ifdef _WIN32
  const DWORD code = WSAGetLastError ();
  char *msg = nullptr;
  const DWORD n = FormatMessageA (
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
          | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR> (&msg), 0, nullptr);
  std::string out = "WSA error " + std::to_string (code);
  if (n != 0 && msg != nullptr) {
    out += ": ";
    out += msg;
    LocalFree (msg);
  }
  while (!out.empty () && (out.back () == '\n' || out.back () == '\r')) {
    out.pop_back ();
  }
  return out;
#else
  return std::strerror (errno);
#endif
}

namespace
{

bool
fill_unix_address (
#ifdef _WIN32
    SOCKADDR_UN &addr,
#else
    sockaddr_un &addr,
#endif
    const std::string &socket_path)
{
  addr.sun_family = AF_UNIX;
  if (socket_path.size () >= sizeof (addr.sun_path)) {
    return false;
  }
  std::snprintf (addr.sun_path, sizeof (addr.sun_path), "%s",
                 socket_path.c_str ());
  return true;
}

} // namespace

socket_handle
socket_open_client (const std::string &socket_path)
{
  ensure_socket_runtime ();
#ifdef _WIN32
  SOCKET s = ::socket (AF_UNIX, SOCK_STREAM, 0);
  if (s == INVALID_SOCKET) {
    return invalid_socket;
  }
  SOCKADDR_UN addr{};
  if (!fill_unix_address (addr, socket_path)) {
    ::closesocket (s);
    return invalid_socket;
  }
  if (::connect (s, reinterpret_cast<SOCKADDR *> (&addr), sizeof (addr)) != 0) {
    ::closesocket (s);
    return invalid_socket;
  }
  return static_cast<socket_handle> (s);
#else
  int fd = ::socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return invalid_socket;
  }
  sockaddr_un addr{};
  if (!fill_unix_address (addr, socket_path)) {
    ::close (fd);
    return invalid_socket;
  }
  if (::connect (fd, reinterpret_cast<struct sockaddr *> (&addr), sizeof (addr))
      < 0) {
    ::close (fd);
    return invalid_socket;
  }
  return static_cast<socket_handle> (fd);
#endif
}

socket_handle
socket_open_server (const std::string &socket_path)
{
  ensure_socket_runtime ();
#ifdef _WIN32
  SOCKET s = ::socket (AF_UNIX, SOCK_STREAM, 0);
  if (s == INVALID_SOCKET) {
    return invalid_socket;
  }
  SOCKADDR_UN addr{};
  if (!fill_unix_address (addr, socket_path)) {
    ::closesocket (s);
    return invalid_socket;
  }
  if (::bind (s, reinterpret_cast<SOCKADDR *> (&addr), sizeof (addr)) != 0
      || ::listen (s, 5) != 0) {
    ::closesocket (s);
    return invalid_socket;
  }
  return static_cast<socket_handle> (s);
#else
  int fd = ::socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return invalid_socket;
  }
  sockaddr_un addr{};
  if (!fill_unix_address (addr, socket_path)) {
    ::close (fd);
    return invalid_socket;
  }
  if (::bind (fd, reinterpret_cast<struct sockaddr *> (&addr), sizeof (addr))
          < 0
      || ::listen (fd, 5) < 0) {
    ::close (fd);
    return invalid_socket;
  }
  return static_cast<socket_handle> (fd);
#endif
}

socket_handle
socket_try_accept (socket_handle server)
{
#ifdef _WIN32
  SOCKET srv = static_cast<SOCKET> (server);
  fd_set fds;
  FD_ZERO (&fds);
  FD_SET (srv, &fds);
  timeval timeout{};
  timeout.tv_usec = 100; // 100 microsecond timeout for non-blocking
  if (::select (0, &fds, nullptr, nullptr, &timeout) <= 0) {
    return invalid_socket;
  }
  SOCKET client = ::accept (srv, nullptr, nullptr);
  if (client == INVALID_SOCKET) {
    return invalid_socket;
  }
  return static_cast<socket_handle> (client);
#else
  int srv = static_cast<int> (server);
  fd_set fds;
  FD_ZERO (&fds);
  FD_SET (srv, &fds);
  timeval timeout{};
  timeout.tv_usec = 100; // 100 microsecond timeout for non-blocking
  if (::select (srv + 1, &fds, nullptr, nullptr, &timeout) <= 0) {
    return invalid_socket;
  }
  int client = ::accept (srv, nullptr, nullptr);
  if (client < 0) {
    return invalid_socket;
  }
  return static_cast<socket_handle> (client);
#endif
}

} // namespace wsl::net
