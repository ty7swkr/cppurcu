/*
 * guard.h
 *
 *  Created on: 2025. 10. 26.
 *      Author: tys
 */

#pragma once

#include <cppurcu/source.h>

namespace cppurcu
{

template<typename T>
struct alignas(64) tls_value_t
{
  bool        init      = false;
  uint64_t    version   = 0;
  const_t<T>  *ptr      = nullptr;  // Fast path
  uint64_t    ref_count = 0;
  bool        to_release = false;
  std::shared_ptr<const_t<T>> value = nullptr;
};

template<typename T>
class local;

/**
 * RAII guard for snapshot isolation
 *
 * Even when multiple `storage::load()` calls occur across complex call chains within
 * a specific scope in the same thread, or when data updates occur from other threads,
 * all read operations within that thread are enforced to see the same data version.
 */
template<typename T>
class guard final
{
public:
  guard(const guard &) = delete;
  guard &operator=(const guard &) = delete;

  ~guard() noexcept
  {
    if (moved_ == true)
      return;

    if (--tls_value_.ref_count > 0)
      return;

    if (tls_value_.to_release == false)
      return;

    --tls_value_.version;
    tls_value_.ptr = nullptr;
    tls_value_.value.reset();
    tls_value_.to_release = false;
  }

  // Pointer-like access
  // const T * objects returned as operator->() are very dangerous if stored separately
  // they should only be used within their scope.
  const_t<T> *operator->() const noexcept { return tls_value_.ptr;    }
  const_t<T> &operator* () const noexcept { return *(tls_value_.ptr); }
  explicit operator bool() const noexcept { return tls_value_.ptr != nullptr; }

  uint64_t ref_count    () const noexcept { return tls_value_.ref_count; }

  struct tls_t
  {
    explicit tls_t(bool &to_release) : to_release_(to_release) {}
    void schedule_release () const noexcept { to_release_ = true; }
    void retain           () const noexcept { to_release_ = false;}
    bool release_scheduled() const noexcept { return to_release_; }
  private:
    bool &to_release_;
  } tls;

protected:
  friend class local<T>;

  template<typename... Us>
  friend class guard_pack;

  guard(tls_value_t<T> &tls_value, const source<T> &source)
  : tls(tls_value.to_release), tls_value_(tls_value)
  {
    if (tls_value_.ref_count++ > 0)
      return;

    // in case tls_value_.ref_count == 0
    if (auto [new_version, new_source] = source.load(tls_value_.version); new_version != tls_value_.version)
    {
      // Raw pointer update only when version changes, Slow Path
      tls_value_.version = new_version;
      tls_value_.ptr     = new_source.get();
      tls_value_.value   = std::move(new_source);
    }
  }

  guard(tls_value_t<T> &tls_value, const source<T> &source, bool to_release)
  : guard(tls_value, source)
  {
    tls_value_.to_release = to_release;
  }

  // private move constructor - only accessible by guard_pack
  guard(guard &&other) noexcept
  : tls(other.tls_value_.to_release), tls_value_(other.tls_value_)
  {
    other.moved_ = true;
  }

private:
  tls_value_t<T>  &tls_value_;
  bool            moved_  = false;
};

}
