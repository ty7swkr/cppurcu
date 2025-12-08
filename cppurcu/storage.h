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
class storage;

/**
 * @brief Creates a new storage object from a const-qualified T.
 *
 * Constructs and initializes a storage<T> object
 * using a std::shared_ptr<const T>.
 *
 * @param init_value Initial value to be stored in storage. May be nullptr.
 * @param reclaimer  Optional reclaimer_thread instance for background destruction.
 *                   If nullptr, the T object is destroyed on the reading thread.
 * @return A storage<T> object.
 */
template<typename T>
storage<T> create(std::shared_ptr<const T> init_value,
                  std::shared_ptr<reclaimer_thread> reclaimer = nullptr);

/**
 * @brief Creates a new storage object from a non-const T.
 *
 * This overload takes a std::shared_ptr<T> and internally converts it
 * to the const-qualified type required by storage.
 *
 * @param init_value Initial value to be stored in storage. May be nullptr.
 * @param reclaimer  Optional reclaimer_thread instance for background destruction.
 *                   If nullptr, the T object is destroyed on the reading thread.
 * @return The initialized storage<T> object.
 */
template<typename T>
storage<T> create(std::shared_ptr<T> init_value,
                  std::shared_ptr<reclaimer_thread> reclaimer = nullptr);

template<typename T>
class storage
{
public:
  /**
   * @param init_value Initial value. May be nullptr.
   * @param reclaimer  Optional reclaimer_thread instance for background destruction.
   *                   If nullptr, the T object is destroyed on the reader thread.
   *
   * @note  This storage allows nullptr as a valid stored value.
   *        Callers must check for nullptr when it has semantic meaning in their context.
   * @see   guard::operator*() const
   */
  storage(std::shared_ptr<const_t<T>> init_value,
          std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
  : reclaimer_(reclaimer),
    source_   (std::move(init_value), reclaimer.get()),
    local_    (source_) {}

  void update(std::shared_ptr<const_t<T>> value)
  {
    source_.update(std::move(value));
  }

  void operator=(std::shared_ptr<const_t<T>> value)
  {
    source_ = std::move(value);
  }

  // Return and use the guard object as if it were a load() function.
  // Using it directly without return (e.g., storage::load()->value)
  // does not provide snapshot isolation.
  guard<T> load()
  {
    return local_.load();
  }

  /**
   * @brief Loads the current value with automatic TLS cache release.
   *
   * Similar to load(), but schedules the thread-local cache for release
   * when the outermost guard in nested scopes is destroyed.
   *
   * When multiple guards are nested (ref_count > 1), the TLS cache is
   * released only when the last remaining guard (ref_count == 0) goes
   * out of scope, ensuring all nested reads complete before cleanup.
   *
   * @return A guard object with TLS release scheduled on destruction.
   *
   * @note Use this when you want to ensure TLS resources are released
   *       promptly after read operations complete, preventing stale cache.
   * @note To cancel the scheduled TLS release, call tls.retain() on
   *       the returned guard object.
   *
   * @see guard::tls_t::retain()
   * @see guard::tls_t::schedule_release()
   */
  guard<T> load_with_tls_release()
  {
    return local_.load_with_release();
  }

private:
  std::shared_ptr<reclaimer_thread> reclaimer_ = nullptr;
  source<T> source_;
  local <T> local_;
};

template<typename T> storage<T>
create(std::shared_ptr<const T> init_value,
       std::shared_ptr<reclaimer_thread> reclaimer)
{
  return storage<T>(std::move(init_value), reclaimer);
}

template<typename T> storage<T>
create(std::shared_ptr<T> init_value,
       std::shared_ptr<reclaimer_thread> reclaimer)
{
  return storage<T>(std::move(init_value), reclaimer);
}

}
