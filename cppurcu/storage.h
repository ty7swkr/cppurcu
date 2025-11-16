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
  // Using it directly without return (ex. storage::load()->value)
  // can be dangerous because it does not isolate snapshots.
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
