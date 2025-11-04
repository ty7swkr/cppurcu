/*
 * reclaimer_thread.h
 *
 *  Created on: 2025. 10. 26.
 *      Author: tys
 */

#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iostream>
#include <future>
#include <algorithm>

namespace cppurcu
{

class reclaimer_thread
{
public:
  static constexpr size_t capacity = 100;
  // Threshold for capacity waste ratio to trigger shrink_to_fit (e.g., 1.5 = 150%)
  static constexpr float  shrink_ratio_threshold = 1.5f;

  reclaimer_thread(bool wait_until_execution = false) : stop_(false)
  {
    front_.reserve(capacity);
    back_ .reserve(capacity);

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

    if (worker_.joinable())
      worker_.join();
  }

  template<typename T>
  void push(const std::shared_ptr<T> &ptr)
  {
    if (!ptr)
      return;

    std::lock_guard<std::mutex> lock(mutex_);
    back_.emplace_back(ptr);
  }

  std::thread::id
  thread_id() const
  {
    return thread_id_.load();
  }

protected:
  virtual void worker_loop()
  {
    while (!stop_.load(std::memory_order_acquire))
    {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(front_, back_);
      }

      if (front_.size()     > capacity &&
          front_.capacity() > front_.size() * shrink_ratio_threshold)
        front_.shrink_to_fit();

      front_.erase(
          std::remove_if(front_.begin(), front_.end(), [](const auto& sptr)
          { return sptr.unique(); }),
          front_.end() );

      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    back_ .clear();
    front_.clear();
  }

  void create_worker()
  {
    worker_ = std::thread([this]()
    {
      thread_id_ = std::this_thread::get_id();
      worker_loop();
    });
  }

  void create_worker_and_wait()
  {
    std::promise<void> ready_promise;
    auto ready_future = ready_promise.get_future();

    worker_ = std::thread([this, ready = std::move(ready_promise)]() mutable
    {
      thread_id_ = std::this_thread::get_id();
      ready.set_value();
      worker_loop();
    });

    ready_future.wait();
  }

protected:
  std::atomic<std::thread::id> thread_id_;
  std::vector<std::shared_ptr<const void>> front_;
  std::vector<std::shared_ptr<const void>> back_;

protected:
  std::mutex        mutex_;
  std::atomic<bool> stop_;
  std::thread       worker_;
};

}
