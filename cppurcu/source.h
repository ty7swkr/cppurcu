/*
 * source.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/reclaimer_thread.h>
#include <cppurcu/satomic.h>
#include <cppurcu/spinlock.h>
#include <tuple>

namespace cppurcu
{

template<typename T>
using const_t = std::add_const_t<T>;

template<typename T>
class source
{
public:
  source(std::shared_ptr<const_t<T>> init_value,
         reclaimer_thread            *reclaimer = nullptr)
  : value_(std::move(init_value)), reclaimer_(reclaimer) {}

  ~source()
  {
    if (auto value = value_.load(); reclaimer_ != nullptr && value != nullptr)
    {
      reclaimer_->push(std::move(value));
      value_.reset();
    }
  }

  void operator=(std::shared_ptr<const_t<T>> value)
  {
    this->update(std::move(value));
  }

  void update(std::shared_ptr<const_t<T>> value)
  {
    std::shared_ptr<const_t<T>> old = nullptr;
    {
      std::lock_guard<spinlock> guard(update_lock_);
      old = value_.load(std::memory_order_acquire);

      value_  .store    (std::move(value), std::memory_order_release);
      version_.fetch_add(1,                std::memory_order_release);
    }

    if (reclaimer_ != nullptr && old != nullptr)
      reclaimer_->push(std::move(old));
  }

  std::tuple<uint64_t, std::shared_ptr<const_t<T>>>
  load(uint64_t value_version) const noexcept
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
    return {version, value_.load(std::memory_order_acquire)};
  }

protected:
  mutable spinlock     update_lock_;

  satomic<const_t<T>>   value_;
  std::atomic<uint64_t> version_{0};
  reclaimer_thread      *reclaimer_ = nullptr;
};

}
