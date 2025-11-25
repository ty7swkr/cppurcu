/*
 * guard_pack.h
 *
 *  Created on: 2025. 11. 22.
 *      Author: tys
 */

#pragma once

#include <cppurcu/storage.h>
#include <cstddef>
#include <new>
#include <tuple>

namespace cppurcu
{

template<typename... Ts>
class guard_pack;

/**
 * @brief Creates a guard_pack from multiple storages
 *
 * Factory function that loads all storages in sequence and returns
 * a guard_pack holding all guards. This ensures snapshot isolation
 * across multiple storages within the same scope.
 *
 * @tparam Ts Types stored in each storage
 * @param storages References to storage instances to load from
 * @return guard_pack containing guards for all storages
 *
 * @throws Any exception thrown by storage::load()
 *
 * @note This is equivalent to:
 *       make_guard_pack(storage1.load(), storage2.load(), ...)
 *       Guards are loaded left-to-right.
 *
 * @example Structured binding (C++17)
 * @code
 * auto g1 = cppurcu::storage(...);
 * auto g2 = cppurcu::storage(...);
 *
 * // Must use 'const auto&' - guard_pack is non-copyable/non-movable
 * // In C++17, structured binding's const reference return is a method with guaranteed safety defined in the specification
 * const auto &[config, cache] = make_guard_pack(g1, g2);
 *
 * config->config_value;
 * cache->cache_value;
 * @endcode
 *
 * @example
 * @code
 * storage<Config> config_storage(config_data);
 * storage<Cache>  cache_storage(cache_data);
 *
 * auto pack = make_guard_pack(config_storage, cache_storage);
 * pack.get<0>()->config_value;
 * pack.get<1>()->cache_value;
 * @endcode
 */
template<typename... Ts>
guard_pack<Ts...>
make_guard_pack(storage<Ts> &... storages)
{
  return make_guard_pack(storages.load()...);
}

/**
 * @brief Creates a guard_pack from multiple guards
 *
 * This function is the guard<T> version of make_guard_pack(storage...).
 * It's simply useful for isolating multiple guards on a single line.
 *
 * @tparam Ts Types managed by each guard
 * @param guards Guard instances to move into the pack
 * @return guard_pack containing all guards
 *
 * @note The lvalue version of make_guard_pack(guards...) was intentionally deleted.
 *
 * ## Lifetime
 * - guard_pack must not outlive the storages that the guards reference
 * - All guards share the same lifetime within this scope
 * - Destruction occurs in reverse order (LIFO)
 *
 * @example Usage with get<I>()
 * @code
 * auto g1 = cppurcu::storage(...);
 * auto g2 = cppurcu::storage(...);
 *
 * auto pack = make_guard_pack(g1.load(), g2.load());
 *
 * pack.get<0>()->config_value;
 * pack.get<1>()->cache_value;
 * @endcode
 */
template<typename... Ts>
guard_pack<Ts...>
make_guard_pack(guard<Ts> &&... guards);

template<typename... Ts>
guard_pack<Ts...> make_guard_pack(guard<Ts> &... guards) = delete;

/**
 * @brief RAII guard pack for multiple guards
 *
 * Holds multiple guard<T> objects in a single container.
 * Guards are moved into the pack and destroyed in reverse order.
 *
 * @tparam Ts Types stored in each guard
 */
template<typename... Ts>
class guard_pack
{
public:
  static_assert(sizeof...(Ts) > 0, "guard_pack requires at least one guard");

  /**
   * @brief Constructs guard_pack by moving guards
   *
   * Moves each guard in order and stores them
   * in the internal byte array using placement new.
   *
   * @param guards Guard instances to move
   *
   * @throws Any exception thrown by guard move constructor.
   *         If an exception occurs, already constructed guards are destroyed
   *         in reverse order before re-throwing.
   */
  guard_pack(guard<Ts> &&... guards)
  {
    construct_guards<0>(std::move(guards)...);
  }

  // Non-copyable and non-movable
  guard_pack(const guard_pack &) = delete;
  guard_pack(guard_pack &&) = delete;
  guard_pack &operator=(const guard_pack &) = delete;
  guard_pack &operator=(guard_pack &&) = delete;

  /**
   * @brief Destroys all guards in reverse order (LIFO)
   *
   * Ensures proper cleanup even if guards have interdependencies.
   * Destruction order is the reverse of construction order.
   */
  ~guard_pack()
  {
    if constexpr (sizeof...(Ts) > 0)
      destroy_guards<sizeof...(Ts) - 1>();
  }

  /**
   * @brief Access guard at compile-time index I
   *
   * @tparam I Zero-based index of the guard to access
   * @return Reference to guard<T> where T is the I-th type in Ts...
   *
   * @note The returned guard provides operator->() and operator*()
   *       for accessing the underlying data.
   */
  template<std::size_t I>
  auto &get() & noexcept
  {
    static_assert(I < sizeof...(Ts), "Index out of bounds");
    using T = std::tuple_element_t<I, std::tuple<Ts...>>;
    return *std::launder(reinterpret_cast<guard<T>*>(&storage_[offset<I>()]));
  }

  /**
   * @brief Access guard at compile-time index I (const version)
   *
   * @tparam I Zero-based index of the guard to access
   * @return Const reference to guard<T> where T is the I-th type in Ts...
   */
  template<std::size_t I>
  const auto &get() const & noexcept
  {
    static_assert(I < sizeof...(Ts), "Index out of bounds");
    using T = std::tuple_element_t<I, std::tuple<Ts...>>;
    return *std::launder(reinterpret_cast<const guard<T>*>(&storage_[offset<I>()]));
  }

  /**
   * @brief Returns the number of guards in the pack
   * @return sizeof...(Ts), the number of guards
   */
  static constexpr std::size_t size() noexcept { return sizeof...(Ts); }

private:
  /**
   * @brief Calculates byte offset for the I-th guard in storage_
   *
   * Each guard<T> may have different size and alignment requirements.
   * This function computes the properly aligned offset for each guard.
   *
   * Algorithm:
   * - offset<0>() = 0 (first guard starts at beginning)
   * - offset<I>() = align_up(offset<I-1>() + sizeof(guard<I-1>), alignof(guard<I>))
   *
   * @tparam I Index of the guard
   * @return Byte offset from storage_ start where guard<I> is located
   *
   * @note All calculations are constexpr, resolved at compile time.
   */
  template<std::size_t I>
  static constexpr std::size_t offset() noexcept
  {
    if constexpr (I == 0)
    {
      return 0;
    }
    else
    {
      using PREV_T = std::tuple_element_t<I - 1, std::tuple<Ts...>>;
      using CURR_T = std::tuple_element_t<I, std::tuple<Ts...>>;

      // End position of previous guard
      constexpr std::size_t prev_end = offset<I - 1>() + sizeof(guard<PREV_T>);

      // Alignment requirement of current guard
      constexpr std::size_t align    = alignof(guard<CURR_T>);

      // Round up to next aligned address: (prev_end + align - 1) / align * align
      return (prev_end + align - 1) / align * align;
    }
  }

  /**
   * @brief Calculates total size needed for storage_ array
   *
   * Computes the minimum byte array size required to hold all guards
   * with proper alignment.
   *
   * @return Total bytes needed, or 1 if pack is empty (to avoid zero-size array)
   */
  static constexpr std::size_t total_size() noexcept
  {
    if constexpr (sizeof...(Ts) == 0)
    {
      return 1;  // Avoid zero-size array (undefined behavior)
    }
    else
    {
      using LAST_T = std::tuple_element_t<sizeof...(Ts) - 1, std::tuple<Ts...>>;
      return offset<sizeof...(Ts) - 1>() + sizeof(guard<LAST_T>);
    }
  }

  /**
   * @brief Determines maximum alignment requirement among all guards
   *
   * The storage_ array must be aligned to satisfy all guard types.
   * This returns the strictest (largest) alignment requirement.
   *
   * @return Maximum alignof(guard<T>) across all T in Ts...
   */
  static constexpr std::size_t max_align() noexcept
  {
    if constexpr (sizeof...(Ts) == 0)
      return 1;
    else
      return std::max({alignof(guard<Ts>)...});
  }

  /**
   * @brief Recursively constructs guards via placement new
   *
   * Move-constructs guard<U> at the correct offset, then recursively
   * constructs remaining guards.
   *
   * @tparam I Current index being constructed
   * @tparam U Type of current guard
   * @tparam Us Types of remaining guards
   * @param g Current guard to move
   * @param rest Remaining guards to move
   *
   * @throws Any exception from guard move constructor.
   *         On exception, destroys the guard at index I before re-throwing.
   *
   * @note Uses move construction from the passed guard.
   *       guard<T> has a private move constructor accessible via friend.
   */
  template<std::size_t I, typename U, typename... Us>
  void construct_guards(guard<U> &&g, guard<Us> &&... rest)
  {
    void *ptr = &storage_[offset<I>()];
    new (ptr) guard<U>(std::move(g));

    if constexpr (sizeof...(Us) > 0)
      construct_guards<I + 1>(std::move(rest)...);
  }

  /**
   * @brief Recursively destroys guards in reverse order
   *
   * Destroys guard at index I, then recursively destroys guards
   * at lower indices. This ensures LIFO destruction order.
   *
   * @tparam I Index of guard to destroy (starts at sizeof...(Ts) - 1)
   *
   * @note Called from destructor. Must not throw.
   */
  template<std::size_t I>
  void destroy_guards() noexcept
  {
    using T   = std::tuple_element_t<I, std::tuple<Ts...>>;
    auto *ptr = std::launder(reinterpret_cast<guard<T>*>(&storage_[offset<I>()]));
    ptr->~guard();

    if constexpr (I > 0)
      destroy_guards<I - 1>();
  }

  /**
   * @brief Raw storage for all guard objects
   *
   * Aligned byte array where guards are constructed via placement new.
   * Size and alignment are computed at compile time to accommodate
   * all guard types with proper alignment.
   *
   * Layout example for guard_pack<int, string, double>:
   * ```
   * [guard<int>][pad][guard<string>][pad][guard<double>]
   *  ^offset<0>      ^offset<1>          ^offset<2>
   * ```
   */
  alignas(max_align()) std::byte storage_[total_size()];
};

/**
 * @brief Factory function implementation
 * @see make_guard_pack declaration above for documentation
 */
template<typename... Ts>
guard_pack<Ts...>
make_guard_pack(guard<Ts> &&... guards)
{
  return guard_pack<Ts...>(std::move(guards)...);
}

} // namespace cppurcu

// ============================================================================
// Standard Library Customization for Structured Binding Support
// ============================================================================

namespace std
{

/**
 * @brief Specialization of std::tuple_size for guard_pack
 *
 * Tells the compiler how many elements can be bound.
 *
 * @code
 * static_assert(std::tuple_size_v<guard_pack<int, string>> == 2);
 * @endcode
 */
template<typename... Ts>
struct tuple_size<cppurcu::guard_pack<Ts...>>
: std::integral_constant<std::size_t, sizeof...(Ts)> {};

/**
 * @brief Specialization of std::tuple_element for guard_pack
 *
 * Maps index I to the corresponding guard<T> type.
 *
 * @code
 * // For guard_pack<int, string>:
 * // tuple_element_t<0, ...> = guard<int>
 * // tuple_element_t<1, ...> = guard<string>
 * @endcode
 */
template<std::size_t I, typename... Ts>
struct tuple_element<I, cppurcu::guard_pack<Ts...>>
{
  using type = std::tuple_element_t<I, std::tuple<cppurcu::guard<Ts>...>>;
};

} // namespace std

namespace cppurcu
{

// ============================================================================
// ADL (Argument-Dependent Lookup) get() Functions
// ============================================================================

/**
 * @brief ADL-discoverable get() for structured binding (non-const)
 *
 * @tparam I Index of element to retrieve
 * @tparam Ts Types in the guard_pack
 * @param pack The guard_pack to access
 * @return Reference to guard at index I
 */
template<std::size_t I, typename... Ts>
auto &get(guard_pack<Ts...> &pack)
{
  return pack.template get<I>();
}

/**
 * @brief ADL-discoverable get() for structured binding (const)
 *
 * @tparam I Index of element to retrieve
 * @tparam Ts Types in the guard_pack
 * @param pack The guard_pack to access
 * @return Const reference to guard at index I
 */
template<std::size_t I, typename... Ts>
const auto &get(const guard_pack<Ts...> &pack)
{
  return pack.template get<I>();
}

} // namespace cppurcu
