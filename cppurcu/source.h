/*
 * source.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/reclaimer_thread.h>
#include <cppurcu/satomic.h>

namespace cppurcu
{

template<typename T>
using const_t = std::add_const_t<T>;

template<typename T>
class source
{
public:
  source(const std::shared_ptr<const_t<T>> &init_value,
         reclaimer_thread                  *reclaimer = nullptr)
  : value_(init_value), reclaimer_(reclaimer) {}

  source(      std::shared_ptr<const_t<T>> &&init_value,
               reclaimer_thread            *reclaimer = nullptr)
  : value_(std::move(init_value)), reclaimer_(reclaimer) {}

  void operator=(const std::shared_ptr<const_t<T>> &value)
  {
    this->update(value);
  }

  void update(const std::shared_ptr<const_t<T>> &value)
  {
    auto old = value_.load(std::memory_order_acquire);

    value_  .store    (value, std::memory_order_release);
    version_.fetch_add(1,     std::memory_order_release);

    if (reclaimer_ != nullptr)
      reclaimer_->push(old);
  }

  std::tuple<uint64_t, std::shared_ptr<const_t<T>>>
  load(uint64_t value_version) const
  {
    auto version = version_.load(std::memory_order_acquire);
    if (value_version == version)
      return {value_version, nullptr};

    return {version, value_.load(std::memory_order_acquire)};
  }

  std::tuple<uint64_t, std::shared_ptr<const_t<T>>>
  load() const noexcept
  {
    auto version = version_.load(std::memory_order_acquire);
    auto value   = value_  .load(std::memory_order_acquire);
    return {version, value};
  }

protected:
  satomic<const_t<T>>   value_;
  std::atomic<uint64_t> version_{0};
  reclaimer_thread      *reclaimer_ = nullptr;
};

}
