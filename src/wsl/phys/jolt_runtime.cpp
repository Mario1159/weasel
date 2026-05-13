#include "jolt_runtime.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <cstdint>

#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>

namespace wsl
{

namespace
{

std::mutex g_jolt_runtime_mutex;
std::atomic<int> g_jolt_runtime_refcount{ 0 };

void
jolt_trace_impl (const char *in_fmt, ...)
{
  va_list list;
  va_start (list, in_fmt);
  char buffer[1024];
  vsnprintf (buffer, sizeof (buffer), in_fmt, list);
  va_end (list);

  std::cout << buffer << '\n';
}

} // namespace

void
phys::retain_jolt_runtime ()
{
  std::scoped_lock const lock (g_jolt_runtime_mutex);

  if (g_jolt_runtime_refcount++ > 0) {
    return;
  }

  JPH::RegisterDefaultAllocator ();
  JPH::Trace = jolt_trace_impl;

#ifdef JPH_ENABLE_ASSERTS
  JPH::AssertFailed = [] (const char *expr, const char *msg, const char *file,
                          uint32_t line) -> bool {
    std::cerr << file << ":" << line << ": (" << expr << ") "
              << (msg ? msg : "") << std::endl;
    return true;
  };
#endif

  if (JPH::Factory::sInstance == nullptr) {
    JPH::Factory::sInstance = new JPH::Factory ();
  }

  JPH::RegisterTypes ();
}

void
phys::release_jolt_runtime ()
{
  std::scoped_lock const lock (g_jolt_runtime_mutex);

  const int new_count = --g_jolt_runtime_refcount;
  assert (new_count >= 0 && "Jolt runtime released too many times.");
  if (new_count > 0) {
    return;
  }

  JPH::UnregisterTypes ();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
}

} // namespace wsl
