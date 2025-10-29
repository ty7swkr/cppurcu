/*
 * guard.h
 *
 *  Created on: 2025. 10. 28.
 *      Author: tys
 */

#pragma once

#include <cppurcu/source.h>
#include <tuple>

namespace cppurcu
{

template<typename T>
struct tls_value_t
{
  bool        init      = false;
  uint64_t    version   = 0;
  const_t<T>  *ptr      = nullptr;  // Fast path
  uint64_t    ref_count = 0;
  std::shared_ptr<const_t<T>> value = nullptr;
};

template<typename T>
class local;

/**
 * RAII guard for snapshot isolation
 *
 * Multiple storage<T>::load() calls within the same thread share the same data snapshot,
 * even across complex call chains. The first load() determines the version, and
 * all subsequent calls within that scope share the same snapshot.
 */
template<typename T>
class guard final
{
public:
  guard(const guard &) = delete;
  guard(guard &&other) = delete;
  guard &operator=(const guard &) = delete;

  ~guard() { --tls_value_.ref_count; }

  // Pointer-like access
  const_t<T> *operator->() const noexcept { return tls_value_.ptr;    }
  const_t<T> &operator* () const noexcept { return *(tls_value_.ptr); }
  explicit operator bool() const noexcept { return tls_value_.ptr != nullptr; }

protected:
  friend class local<T>;

  guard(tls_value_t<T> &tls_value, const source<T> &source, reclaimer_thread *reclaimer = nullptr)
  : tls_value_(tls_value), source_(source), reclaimer_(reclaimer)
  {
    if (tls_value_.ref_count++ > 0)
      return;

    // tls_value_.ref_count == 0
    if (auto [new_version, new_source] = source_.load(tls_value_.version); new_version != tls_value_.version)
    {
      // Raw pointer update only when version changes, Fast Path
      tls_value_.version = new_version;
      tls_value_.ptr     = new_source.get();
      if (reclaimer_ != nullptr) reclaimer_->push(tls_value_.value);
      tls_value_.value   = std::move(new_source);
    }
  }

private:
  tls_value_t<T>    &tls_value_;
  const source<T>   &source_;
  reclaimer_thread  *reclaimer_ = nullptr;
};

}
