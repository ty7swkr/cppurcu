/*
 * source.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/retirement_thread.h>
#include <cppurcu/satomic.h>
#include <memory>

namespace cppurcu
{

// Forward declaration
class retirement_thread;

template<typename T>
class source
{
public:
  using CONST_T = std::add_const_t<T>;

  source(const std::shared_ptr<CONST_T> &init_value,
         retirement_thread              *retirement = nullptr)
  : value_(init_value), retirement_(retirement) {}

  source(      std::shared_ptr<CONST_T> &&init_value,
               retirement_thread        *retirement = nullptr)
  : value_(std::move(init_value)), retirement_(retirement) {}

  void operator=(const std::shared_ptr<CONST_T> &value)
  {
    this->update(value);
  }

  void update(const std::shared_ptr<CONST_T> &value)
  {
    auto old = value_.load(std::memory_order_acquire);

    value_  .store    (value, std::memory_order_release);
    version_.fetch_add(1, std::memory_order_release);

    if (retirement_ != nullptr)
      retirement_->push(old);
  }

  std::tuple<uint64_t, std::shared_ptr<CONST_T>>
  load(uint64_t value_version) const noexcept
  {
    auto version = version_.load(std::memory_order_acquire);
    if (value_version == version)
      return {value_version, nullptr};

    return {version, value_.load(std::memory_order_acquire)};
  }

  std::tuple<uint64_t, std::shared_ptr<CONST_T>>
  load() const noexcept
  {
    auto version = version_.load(std::memory_order_acquire);
    auto value   = value_  .load(std::memory_order_acquire);
    return {version, value};
  }

protected:
  satomic<CONST_T>      value_;
  std::atomic<uint64_t> version_{0};
  retirement_thread    *retirement_ = nullptr;
};

}
