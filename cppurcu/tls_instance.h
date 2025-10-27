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

  virtual ~tls_instance()      { storage_.erase(self_); }

  virtual tls_instance<T> &
  set(const T &value)
  {
    storage_[self_] = value;
    return *this;
  }

  virtual tls_instance<T> &
  set(T &&value)
  {
    storage_[self_] = std::move(value);
    return *this;
  }

  virtual bool
  has() const
  {
    return storage_.count(self_) > 0;
  }

  virtual T &
  operator()() { return storage_[self_]; }

  virtual T &
  ref() { return storage_[self_]; }

  /**
   * @brief Returns a reference to the value in thread-local storage
   * @return A const reference to the stored value
   * @throws std::out_of_range If the key does not exist
   */
  virtual const T &
  ref_const() const { return storage_.at(self_); }

  virtual std::optional<std::reference_wrapper<const T>>
  ref_if() const
  {
    auto it = storage_.find(self_);
    if (it == storage_.end())
      return std::nullopt;

    return it->second;
  }

  virtual std::optional<std::reference_wrapper<T>>
  ref_if()
  {
    auto it = storage_.find(self_);
    if (it == storage_.end())
      return std::nullopt;

    return it->second;
  }

protected:
  /** @brief Stores the address of the current tls_instance<T> as an integer (used as the key) */
  uintptr_t self_ = reinterpret_cast<uintptr_t>(this);

  /**
   * @brief Per-thread storage using instance address as key
   * Each thread gets default-initialized values on first access via operator[].
   */
  static thread_local inline std::unordered_map<uintptr_t, T> storage_;
};

}

