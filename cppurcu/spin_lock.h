/*
 * spin_lock.h
 *
 *  Created on: 2025. 11. 1.
 *      Author: tys
 */
#pragma once

#include <atomic>   // std::atomic_flag
#include <thread>   // std::this_thread::yield()

namespace cppurcu
{

class spin_lock
{
public:
  spin_lock() {}

  void lock()
  {
    while (flag_.test_and_set(std::memory_order_acquire))
      std::this_thread::yield();
  }

  void unlock()
  {
    flag_.clear(std::memory_order_release);
  }

private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

}
