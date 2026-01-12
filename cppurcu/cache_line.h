/*
 * cache_line.h
 *
 *  Created on: 2026. 1. 13.
 *      Author: tys
 */

#pragma once

#include <cstddef>

namespace cppurcu
{

#if defined(__aarch64__) || defined(_M_ARM64)
    constexpr std::size_t CACHE_LINE_SIZE = 128;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

}
