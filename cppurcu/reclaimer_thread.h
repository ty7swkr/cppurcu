/*
 * reclaimer_thread.h
 *
 * Created on: 2025. 10. 26.
 * Author: tys
 */

#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <algorithm>
#include <condition_variable>
#include <unordered_set>
#include <vector>

namespace cppurcu
{
/**
 * Best-effort reclamation for shared_ptr
 *
 * A worker thread scans ptrs_ on notification or when reclaim_interval_ expires.
 * If reclaim_interval_ == 0μs, it scans only on notification.
 * Entries are removed only when shared_ptr::use_count() == 1.(unique())
 * Attempts to destroy all tracked objects before the thread exits,
 * but cannot guarantee completion if shared_ptrs are still referenced elsewhere.
 */
class reclaimer_thread
{
public:
  reclaimer_thread(bool wait_until_execution = true,
                   std::chrono::microseconds reclaim_interval = std::chrono::microseconds{10000})
  : reclaim_interval_(reclaim_interval), stop_(false) { init(wait_until_execution); }

  reclaimer_thread(std::chrono::microseconds reclaim_interval,
                   bool wait_until_execution = true)
  : reclaim_interval_(reclaim_interval), stop_(false) { init(wait_until_execution); }

  reclaimer_thread(const reclaimer_thread &) = delete;
  reclaimer_thread(reclaimer_thread &&) = delete;
  reclaimer_thread &operator=(const reclaimer_thread &) = delete;
  reclaimer_thread &operator=(reclaimer_thread &&) = delete;

  virtual ~reclaimer_thread()
  {
    stop_.store(true, std::memory_order_release);
    {
      std::lock_guard<std::mutex> guard(lock_);
      notified_ = true;
      cond_.notify_all();
    }

    if (worker_.joinable())
      worker_.join();
  }

  template<typename T>
  void push(std::shared_ptr<T> &&ptr)
  {
    if (!ptr)
      return;

    std::lock_guard<std::mutex> guard(lock_);
    if (ptrs_.insert(std::move(ptr)).second == false)
      return;

    if (notified_ == true)
      return;

    notified_ = true;
    cond_.notify_one();
  }

  std::thread::id
  thread_id() const
  {
    return thread_id_.load(std::memory_order_acquire);
  }

protected:
  virtual void worker_loop()
  {
    std::vector<std::shared_ptr<const void>> unique_ptrs;

    while (stop_.load(std::memory_order_acquire) == false)
    {
      {
        std::unique_lock<std::mutex> guard(lock_);

        // Prevent spurious wakeups and signal loss
        auto pred = [this]()
        {
          if (notified_ == false)
            return false;

          notified_ = false;
          return true;
        };

        if (reclaim_interval_.count() == 0)
          cond_.wait    (guard, pred);
        else
          cond_.wait_for(guard, reclaim_interval_, pred);

        for (auto it = ptrs_.begin(); it != ptrs_.end();)
        {
          if ((*it).use_count() > 1) { ++it; continue; }

          unique_ptrs.emplace_back(std::move(*it));
          it = ptrs_.erase(it);
        }
      }

      unique_ptrs.clear();
    }
  }

  void create_worker()
  {
    worker_ = std::thread([this]()
    {
      thread_id_.store(std::this_thread::get_id(), std::memory_order_release);
      worker_loop();
    });
  }

  void create_worker_and_wait()
  {
    std::promise<void> ready_promise;
    auto ready_future = ready_promise.get_future();

    worker_ = std::thread([this, ready = std::move(ready_promise)]() mutable
    {
      thread_id_.store(std::this_thread::get_id(), std::memory_order_release);
      ready.set_value();
      worker_loop();
    });

    ready_future.wait();
  }

  void init(bool wait_until_execution)
  {
    if (wait_until_execution == false)
      create_worker();
    else
      create_worker_and_wait();
  }

protected:
  std::atomic<std::thread::id> thread_id_;
  std::unordered_set<std::shared_ptr<const void>> ptrs_;

protected:
  std::mutex              lock_;
  std::condition_variable cond_;
  bool                    notified_ = false;

protected:
  std::chrono::microseconds reclaim_interval_{10000};
  std::atomic<bool>         stop_{false};
  std::thread               worker_;
};

}
