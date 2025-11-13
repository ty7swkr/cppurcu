/*
 * storage.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/local.h>

namespace cppurcu
{

template<typename T>
class storage
{
public:
  /**
   * @param init_value Initial value. Nullptr is also allowed.
   * @note  This storage permits nullptr as a stored state.
   *        Callers must check for nullptr if nullptr carries semantic meaning in their context.
   * @see   guard::operator*() const
   * @param reclaimer An optional `reclaimer_thread` instance for background destruction.
   *        If `nullptr`, the `T` object's destruction will occur on the reader thread.
   */
  storage(const std::shared_ptr<const_t<T>> &init_value,
          std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
  : reclaimer_(reclaimer),
    source_   (init_value, reclaimer.get()),
    local_    (source_,    reclaimer.get()) {}

  storage(std::shared_ptr<const_t<T>>       &&init_value,
          std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
  : reclaimer_(reclaimer),
    source_   (std::move(init_value), reclaimer.get()),
    local_    (source_,               reclaimer.get()) {}

  void update(const std::shared_ptr<const_t<T>> &value)
  {
    source_.update(value);
  }

  void operator=(const std::shared_ptr<const_t<T>> &value)
  {
    source_ = value;
  }

  guard<T> load()
  {
    return local_.load();
  }

private:
  std::shared_ptr<reclaimer_thread> reclaimer_ = nullptr;
  source<T> source_;
  local <T> local_;
};

template<typename T> storage<T>
create(const std::shared_ptr<const_t<T>> &init_value,
       std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
{
  return storage<T>(init_value, reclaimer);
}

template<typename T> storage<T>
create(const std::shared_ptr<T> &init_value,
       std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
{
  return storage<T>(std::static_pointer_cast<const_t<T>>(init_value), reclaimer);
}

}
