module;
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

module wsl.core;

namespace wsl::sys
{

task_pool::task_pool (std::size_t worker_count)
{
  if (worker_count == 0) {
    worker_count = 1;
  }
  m_workers.reserve (worker_count);
  m_stop = false;
  for (std::size_t i = 0; i < worker_count; ++i) {
    m_workers.emplace_back ([this] { worker_loop (); });
  }
  wsl::log::sys ()->debug ("task_pool: started {} worker thread(s)",
                           m_workers.size ());
}

task_pool::~task_pool ()
{
  {
    std::unique_lock<std::mutex> lock (m_mutex);
    m_stop = true;
  }
  m_cv.notify_all ();
  for (std::thread &t : m_workers) {
    if (t.joinable ()) {
      t.join ();
    }
  }
}

void
task_pool::enqueue (std::function<void ()> task)
{
  {
    std::unique_lock<std::mutex> lock (m_mutex);
    m_queue.push_back (std::move (task));
    ++m_outstanding;
  }
  m_cv.notify_one ();
}

void
task_pool::dispatch (std::span<std::function<void ()>> tasks)
{
  for (auto &t : tasks) {
    enqueue (std::move (t));
  }
  fence ();
}

void
task_pool::fence ()
{
  std::unique_lock<std::mutex> lock (m_mutex);
  m_fence_cv.wait (lock, [this] { return m_outstanding.load () == 0; });
}

void
task_pool::worker_loop ()
{
  for (;;) {
    std::function<void ()> task;
    {
      std::unique_lock<std::mutex> lock (m_mutex);
      m_cv.wait (lock, [this] { return m_stop || !m_queue.empty (); });
      if (m_stop && m_queue.empty ()) {
        return;
      }
      task = std::move (m_queue.front ());
      m_queue.erase (m_queue.begin ());
    }

    try {
      task ();
    } catch (const std::exception &e) {
      wsl::log::sys ()->error ("task_pool: worker task threw: {}", e.what ());
    } catch (...) {
      wsl::log::sys ()->error ("task_pool: worker task threw (unknown)");
    }

    {
      std::unique_lock<std::mutex> lock (m_mutex);
      if (--m_outstanding == 0) {
        m_fence_cv.notify_all ();
      }
    }
  }
}

} // namespace wsl::sys
