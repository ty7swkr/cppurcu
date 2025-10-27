/*
 * retirement_thread.h
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

namespace cppurcu
{

class retirement_thread
{
public:
  retirement_thread(bool wait_until_execution = false) : stop_(false)
  {
    front_queue_.reserve(100);
    back_queue_ .reserve(100);

    if (wait_until_execution == false)
      create_worker();
    else
      create_worker_and_wait();
  }

  retirement_thread(const retirement_thread &) = delete;
  retirement_thread(retirement_thread &&) = delete;
  retirement_thread &operator=(const retirement_thread &) = delete;
  retirement_thread &operator=(retirement_thread &&) = delete;

  virtual ~retirement_thread()
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
    back_queue_.emplace_back(ptr);
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
        std::swap(front_queue_, back_queue_);
      }

      front_queue_.clear();
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    back_queue_ .clear();
    front_queue_.clear();
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
  std::vector<std::shared_ptr<const void>> front_queue_;
  std::vector<std::shared_ptr<const void>> back_queue_;

protected:
  std::mutex        mutex_;
  std::atomic<bool> stop_;
  std::thread       worker_;
};

}
