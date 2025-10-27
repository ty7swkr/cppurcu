/*
 * RCULocal.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/source.h>
#include <cppurcu/retirement_thread.h>
#include <cppurcu/tls_instance.h>
#include <atomic>

namespace cppurcu
{

template<typename T>
class local
{
public:
  using CONST_T = std::add_const_t<T>;

  local(const source<T> &source, retirement_thread *retirement = nullptr)
  : source_(source), retirement_(retirement) {}

  virtual ~local() {}

  virtual CONST_T *load()
  {
    auto &[init, version, ptr, value] = tls_value_.ref();
    if (init == false)
    {
      auto [new_version, new_source] = source_.load();
      init = true; version = new_version; ptr = new_source.get(); value = new_source;
      return ptr;
    }

    if (auto [new_version, new_source] = source_.load(version); new_version != version)
    {
      // Raw pointer update only when version changes, Fast Path
      version = new_version;
      ptr     = new_source.get();
      if (retirement_ != nullptr) retirement_->push(value);
      value   = std::move(new_source);
    }

    return ptr;
  }

protected:
  // CONST_T * Fast Path
  tls_instance<std::tuple<bool, uint64_t, CONST_T *, std::shared_ptr<CONST_T>>> tls_value_;
  const source<T>   &source_;
  retirement_thread *retirement_ = nullptr;
};

}
