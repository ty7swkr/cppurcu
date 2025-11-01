/*
 * storage.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/local.h>

/**
 * Versioned Thread-Local RCU Cache
 */
namespace cppurcu
{

template<typename T>
class storage
{
public:
  storage(const std::shared_ptr<const_t<T>> &init_value,
          std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
  : reclaimer_ (reclaimer),
    source_     (init_value, reclaimer.get()),
    local_      (source_,    reclaimer.get()) {}

  storage(std::shared_ptr<const_t<T>>       &&init_value,
          std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
  : reclaimer_ (reclaimer),
    source_     (std::move(init_value), reclaimer.get()),
    local_      (source_,               reclaimer.get()) {}

  void update(const std::shared_ptr<const_t<T>> &value)
  {
    source_.update(value);
  }

  void operator=(const std::shared_ptr<const_t<T>> &value)
  {
    source_ = value;
  }

  guard<T> load() noexcept(false)
  {
    return local_.load();
  }

private:
  std::shared_ptr<reclaimer_thread> reclaimer_ = nullptr;
  source<T> source_;
  local <T> local_;
};

}
