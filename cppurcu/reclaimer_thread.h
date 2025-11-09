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

class reclaimer_thread
{
public:
  reclaimer_thread(bool wait_until_execution = true) : stop_(false)
  {
    if (wait_until_execution == false)
      create_worker();
    else
      create_worker_and_wait();
  }

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
  void push(const std::shared_ptr<T> &ptr)
  {
    if (!ptr)
      return;

    std::lock_guard<std::mutex> guard(lock_);
    ptrs_.insert(ptr);

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
        cond_.wait(guard, [this]()
        {
          if (notified_ == false)
            return false;

          notified_ = false;
          return true;
        });

        for (auto it = ptrs_.begin(); it != ptrs_.end();)
        {
          if ((*it).unique() == false) { ++it; continue; }

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

protected:
  std::atomic<std::thread::id> thread_id_;
  std::unordered_set<std::shared_ptr<const void>> ptrs_;

protected:
  std::mutex              lock_;
  std::condition_variable cond_;
  bool                    notified_ = false;

protected:
  std::atomic<bool>       stop_{false};
  std::thread             worker_;
};

}
