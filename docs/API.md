# API Reference

## `cppurcu::storage<T>`

Main class for RCU-protected data storage.

### Constructor
```cpp
storage(std::shared_ptr<const T> init_value,
        std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
```
Creates a new storage with initial data.

**Parameters:**
- `init_value`: Initial data to store
- `reclaimer` (optional): reclaimer_thread instance for background destruction. If nullptr, the T object is destroyed in the reader's thread.

**Lifetime Requirements:**
- The `storage<T>` instance must outlive all threads that call `load()` on it
- It is the caller's responsibility to ensure proper lifetime management
- Violation results in undefined behavior (dangling references)

### Methods

**`guard<T> load()`**
- Thread-safe
- Returns a guard object that provides access to the current data
- On the first load() within the scope, if there is new data, replace it with the new data.

**`guard<T> load_with_tls_release()`**
- Similar to load(), but schedules the thread-local cache for release when the outermost guard in nested scopes is destroyed.
- When multiple guards are nested (ref_count > 1), the TLS cache is released only when the last remaining guard (ref_count == 0) goes out of scope, ensuring all nested reads complete before cleanup.
- Use this when you want to ensure TLS resources are released promptly after read operations complete, preventing stale cache.
- To cancel the scheduled TLS release, call `tls.retain()` on the returned guard object.
- See: `guard::tls_t::retain()`, `guard::tls_t::schedule_release()`

**`void update(std::shared_ptr<const T> value)`**
- Publishes new data
- Deadlocks do not occur even when used concurrently within the same scope as the load() function.
- cppurcu does not reclaim old data itself; it delegates reclamation to std::shared_ptr.

**`void operator=(std::shared_ptr<const T> value)`**
- Convenience operator for updates
- Equivalent to `update(value)`

## `cppurcu::guard<T>`

RAII guard object returned by `storage<T>::load()`.

### Notes
- Cannot be copied or moved. (thread-local only)
- Data remains valid while guard exists.
- Multiple guards nested on the same thread share the same snapshot.

### Methods

**`const T* operator->()`**
- Provides smart pointer-like access to the data

**`const T& operator*()`**
- Provides dereferencing access to the data

**`explicit operator bool()`**
- Checks if the guard holds valid data

**`uint64_t ref_count()`**
- Returns the current reference count of nested guards

### TLS Cache Control

**`guard::tls_t tls`**
- Provides control over thread-local cache behavior

**`void tls.schedule_release()`**
- Schedules the TLS cache for release when this guard is destroyed

**`void tls.retain()`**
- Cancels the scheduled TLS cache release, keeping the cache alive

**`bool tls.release_scheduled()`**
- Returns true if TLS release is currently scheduled

### Example
```cpp
auto guard = storage.load();
if (guard) {
  guard->method();  // Safe access via operator->
}
// Guard destroyed here, data may be updated by next load()

// With TLS release control
auto guard2 = storage.load_with_tls_release();
guard2.tls.retain();  // Cancel the scheduled release
```

## `cppurcu::guard_pack<Ts...>`

An RAII helper that manages multiple guards as a single object

### Notes
- Cannot be copied or moved.
- Must not outlive the storages it references.

### Methods

**`template<std::size_t I> auto& get()`**
- Access guard at compile-time index I

**`static constexpr std::size_t size()`**
- Returns the number of guards in the pack

### Example
```cpp
auto pack = cppurcu::make_guard_pack(storageA, storageB, storageC);
// or
auto pack = cppurcu::make_guard_pack(storageA.load(), storageB.load(), storageC.load());

pack.get<0>()->method_a();
pack.get<1>()->method_b();
pack.get<2>()->method_c();

// Structured binding
const auto& [a, b, c] = cppurcu::make_guard_pack(storageA, storageB, storageC);
a->method_a();
b->method_b();
c->method_c();
```

## `cppurcu::make_guard_pack`

Factory function that creates a guard_pack from multiple storages.

### Signature
```cpp
template<typename... Ts>
guard_pack<Ts...> make_guard_pack(storage<Ts>&... storages);

template<typename... Ts>
guard_pack<Ts...> make_guard_pack(guard<Ts>&&... guards);
```

### Parameters
- `storages`: References to storage instances to load from
- `guards`: `guard<T>` instances to move into the pack

### Returns
- `guard_pack` containing guards for all storages

## `cppurcu::reclaimer_thread`

Background thread for handling object destruction.

### Constructor
```cpp
  reclaimer_thread(bool wait_until_execution = true,
                   std::chrono::microseconds reclaim_interval =
                   std::chrono::microseconds{10000})

  reclaimer_thread(std::chrono::microseconds reclaim_interval,
                   bool wait_until_execution = true)
```
*Periodically scans the reclaim queue and removes shared_ptrs when they become unique, triggering their destruction.*<br>
*Objects that are still referenced elsewhere cannot be reclaimed and remain in the queue.*

**Parameters:**
- `wait_until_execution` (optional): If true, constructor waits until the reclaimer_thread starts. If false, returns immediately.
- `reclaim_interval` (optional, default: 10000μs = 10ms): Interval for periodic scanning of the reclaim queue.
  - If interval > 0μs: Scans periodically at the specified interval, in addition to notifications.
  - If interval is 0μs: Notification-only mode. Scans only when push() is called.
    - May delay reclamation if updates are infrequent.


### Methods

**`template<typename T> void push(std::shared_ptr<T> &&ptr)`**
- Queues an object for background destruction
- Usually called internally by storage::load() when data is updated

**`std::thread::id thread_id() const`**
- ID of the reclaimer_thread
