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
          std::shared_ptr<retirement_thread> retirement = nullptr)
  : retirement_ (retirement),
    source_     (init_value, retirement.get()),
    local_      (source_,    retirement.get()) {}

  storage(std::shared_ptr<const_t<T>> &&init_value,
          std::shared_ptr<retirement_thread> retirement = nullptr)
  : retirement_ (retirement),
    source_     (std::move(init_value), retirement.get()),
    local_      (source_,               retirement.get()) {}

  void update(const std::shared_ptr<const_t<T>> &value)
  {
    source_.update(value);
  }

  void operator=(const std::shared_ptr<const_t<T>> &value)
  {
    source_ = value;
  }

  // During an update, calling load() again in the same scope
  // may destroy previously loaded variables.
  guard<T> load()
  {
    return local_.load();
  }

private:
  std::shared_ptr<retirement_thread> retirement_ = nullptr;
  source<T> source_;
  local <T> local_;
};

}
