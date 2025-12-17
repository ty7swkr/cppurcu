/*
 * satomic.h
 *
 *  Created on: 2025. 5. 24.
 *      Author: tys
 */

#pragma once

#include <memory>
#include <atomic>
#include <utility>

namespace cppurcu
{

template<typename T>
class satomic
{
public:
  satomic() = delete;
  satomic(const satomic&) = delete;
  satomic(satomic&&     ) = delete;

  satomic& operator=(const satomic&) = delete;
  satomic& operator=(satomic&&     ) = delete;

  // Must be initialized internally using make_shared
  template<class... Args>
  explicit satomic(std::in_place_t, Args&&... args)
  : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

  explicit satomic(std::shared_ptr<T> ptr, std::memory_order m_order = std::memory_order_release) noexcept
  {
    store(std::move(ptr), m_order);
  }

  void store(std::shared_ptr<T> ptr, std::memory_order m_order = std::memory_order_release) noexcept
  {
#if defined(__cpp_lib_atomic_shared_ptr)
    ptr_.store(std::move(ptr), m_order);
#else
    std::atomic_store_explicit(&ptr_, std::move(ptr), m_order);
#endif
  }

  satomic &operator=(std::shared_ptr<T> ptr) noexcept
  {
    this->store(std::move(ptr));
    return *this;
  }

  std::shared_ptr<T> load(std::memory_order m_order = std::memory_order_acquire) const noexcept
  {
#if defined(__cpp_lib_atomic_shared_ptr)
    return ptr_.load(m_order);
#else
    return std::atomic_load_explicit(&ptr_, m_order);
#endif
  }

  void reset(std::memory_order m_order = std::memory_order_release) noexcept
  {
    this->store(nullptr, m_order);
  }

private:
#if defined(__cpp_lib_atomic_shared_ptr)
  std::atomic<std::shared_ptr<T>> ptr_;
#else
  std::shared_ptr<T> ptr_;
#endif
};

}
