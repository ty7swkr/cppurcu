/*
 * local.h
 *
 *  Created on: 2025. 10. 25.
 *      Author: tys
 */

#pragma once

#include <cppurcu/guard.h>
#include <cppurcu/tls_instance.h>

namespace cppurcu
{

template<typename T>
class local
{
public:
  local(const source<T> &source, retirement_thread *retirement = nullptr)
  : source_(source), retirement_(retirement) {}
  ~local() {}

  guard<T> load()
  {
    auto &tls_value = tls_value_.ref();
    if (tls_value.init == false)
    {
      auto [new_version, new_source] = source_.load();
      tls_value.init    = true;
      tls_value.version = new_version;
      tls_value.ptr     = new_source.get();
      tls_value.value   = new_source;
    }

    return guard<T>(tls_value, source_, retirement_);
  }

protected:
  // CONST_T * Fast Path
  tls_instance<tls_value_t<T>> tls_value_;

  const source<T>   &source_;
  retirement_thread *retirement_ = nullptr;
};

}
