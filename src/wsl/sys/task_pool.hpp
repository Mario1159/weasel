#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace wsl::sys
{

/**
 * Fixed-size thread pool used to run ECS systems concurrently.
 *
 * Tasks are enqueued with `enqueue`; `dispatch` enqueues a batch and then
 * *fences* (blocks the calling thread) until every task in the batch has
 * completed. The fence is the synchronization primitive the
 * `system_scheduler` relies on to separate dependency levels: a level's tasks
 * are dispatched, the caller waits for the fence, then the next level begins.
 *
 * The pool is created once (owned by `core_systems`) and reused every frame —
 * threads are never created per frame.
 */
class task_pool
{
public:
  explicit task_pool (std::size_t worker_count);

  ~task_pool ();

  task_pool (const task_pool &) = delete;
  task_pool &operator= (const task_pool &) = delete;

  /** Non-blocking: schedule a task to run on a worker thread. */
  void enqueue (std::function<void ()> task);

  /** Enqueue every task in `tasks`, then block until all complete (fence). */
  template <class F>
  void
  dispatch (std::span<F> tasks)
  {
    for (auto &t : tasks) {
      enqueue (std::function<void ()> (t));
    }
    fence ();
  }

  /** Convenience overload accepting a vector of tasks. */
  void dispatch (std::span<std::function<void ()>> tasks);

  /** Number of worker threads. */
  std::size_t
  worker_count () const
  {
    return m_workers.size ();
  }

private:
  void fence ();
  void worker_loop ();

  std::vector<std::thread> m_workers;
  std::vector<std::function<void ()>> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::condition_variable m_fence_cv;
  std::atomic<std::size_t> m_outstanding{ 0 };
  bool m_stop = false;
};

} // namespace wsl::sys
