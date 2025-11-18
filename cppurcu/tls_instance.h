/*
 * tls_instance.h
 *
 *  Created on: 2024. 12. 23.
 *      Author: tys
 */


#pragma once

#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>

namespace cppurcu
{

/**
 * IMPORTANT: Thread-local initialization behavior
 * - When a new thread accesses storage_[self_] for the first time,
 *   std::unordered_map::operator[] creates the value using T's default constructor
 *   bool:false, shared_ptr:nullptr, uint64_t:0, T*:nullptr
 */
template <typename T>
class tls_instance
{
public:
  tls_instance() {}

  tls_instance(const T &value) = delete;
  tls_instance(T      &&value) = delete;

  tls_instance(const tls_instance&) = delete;
  tls_instance(tls_instance&&)      = delete;

  tls_instance& operator=(const tls_instance&)  = delete;
  tls_instance& operator=(tls_instance&&)       = delete;

  virtual ~tls_instance() {}

  virtual T &
  ref() { return storage_()[self_]; }

  /**
   * @brief Returns a reference to the value in thread-local storage
   * @return A const reference to the stored value
   * @throws std::out_of_range If the key does not exist
   */
  virtual const T &
  ref_const() const { return storage_().at(self_); }

protected:
  /**
   * @brief Per-thread storage using instance address as key
   * Each thread gets default-initialized values on first access via operator[].
   *
   * @note Implemented as function-local static to avoid g++ < 9.1 bug with
   *       thread_local inline combination causing TLS guard redefinition errors.
   *       Original: `static thread_local inline std::unordered_map<uint64_t, T> storage_;`
   */
  static std::unordered_map<uint64_t, T> & storage_()
  {
    static thread_local std::unordered_map<uint64_t, T> s;
    return s;
  }

  static inline std::atomic<uint64_t> self_allocator_{0};
  uint64_t self_ = self_allocator_.fetch_add(1, std::memory_order_relaxed) + 1;
};

}

