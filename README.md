# cppurcu

A simple implementation of the C++ RCU (read-copy-update) user-space library that combines thread-local caching with version tracking using only the C++17 standard library.
<br>
<br>

## Key Features

- **Header-Only**: Works with standard library only, no external dependencies
- **Lock-Free Reads**: Implemented so that contention is minimized on the read path after cache warm-up.
- **Snapshot Isolation**: RAII guard pattern for snapshot isolation in the calling thread.<br>
(guard_pack loads multiple storages in a single line.)
- **No Data Duplication**: Data is not deep-copied per thread
- **Optional Background Destruction**: reclaimer_thread can offload object destruction to a separate thread, reducing burden on reader threads
<br>

## API Overview

No reader registration, grace period management, or memory barriers required.<br>
```cpp
storage = new_storage;      // Update example (std::shared_ptr<T> new_storage)
auto data = storage.load(); // Returns guard object
```
*Note1: Unlike traditional RCU, cppurcu does not directly reclaim the old data,
but delegates reclamation to `std::shared_ptr`.
The `std::shared_ptr` reference to the old object is released upon the first `load()` call within the scope.
Therefore, memory is reclaimed when all references to all `std::shared_ptr` instances are released.*<br>

*Note2: Consequently, update calls are deadlock-free regardless of their location.*
<br>
<br>

## Quick Start

### Basic Usage
```cpp
#include <cppurcu/cppurcu.h>
#include <memory>
#include <string>
#include <map>

// Create a storage with initial data
auto storage = cppurcu::create(std::make_shared<std::map<std::string, std::string>>());

// Read (lock-free) - returns a guard object
auto data = storage.load(); // cppurcu::guard<T>
if (data->count("key") > 0) {
  // Use the data
}

// Update (immediate, delegates reclamation to std::shared_ptr)
auto new_data = std::make_shared<std::map<std::string, std::string>>();
(*new_data)["key"] = "value";
storage = new_data; // or storage.update(new_data);
```
**⚠️ Important:**
- The `storage<T>` instance must outlive all threads that use it
- Destroying `storage` while threads are still accessing it results in undefined behavior
- Typically, declare `storage` as a global, static, or long-lived member variable

### Snapshot Isolation
Even when multiple `storage::load()` calls occur across complex call chains within a specific scope in the same thread, or when data updates occur from other threads, all read operations within that thread are enforced to see the same data version.

When all guards are destroyed, the next load() gets the updated version
```cpp
{
  auto data = storage.load();    // Snapshot version 1
  {
    storage.update(new_data2);   // Update to version 2
    auto data1 = storage.load(); // Still Snapshot version 1
  } // data1 Guard destroyed
} // data Guard destroyed

storage.update(new_data3);       // Update to version 3
{
  auto data = storage.load();    // Loads version 3
}
```
### Multi-Storage Snapshot

When you need to load (snapshot) multiple storages in a single line, you can use `guard_pack` as follows:
```cpp
#include <cppurcu/cppurcu.h>

// Multiple data storages
auto storageA = cppurcu::create(...);
auto storageB = cppurcu::create(...);
auto storageC = cppurcu::create(...);

// Load all at once – maintains the same snapshot point
const auto &[a, b, c] = cppurcu::make_guard_pack(storageA, storageB, storageC);

// All reads within this scope see consistent data,
// even if updates occur from other threads.
a->lookup(...);
b->query(...);
c->find(...);
```
If you need a consistent snapshot within a specific scope, you can write code like this:
```cpp
#include <cppurcu/cppurcu.h>
...........
{
  auto pack = cppurcp::make_guard_pack(storageA, storageB, storageC);
  .....
  // Even if storageA and storageC are used here, 
  // the pack maintains a consistent snapshot of storageA, B, and C.
  my_class.calculate(...);
  .....
}
```

**Note:**<br>
- `guard_pack` loads a snapshot from each storage and keeps them alive within the same scope, but it does **not** provide global transactional atomicity across multiple storages.
- Each storage is still versioned independently; `guard_pack` is an RAII helper for convenient multi-storage snapshot loading, not a cross-storage transaction mechanism.

### With Background Destruction (Optional)

For objects with expensive destructors, you can use a reclaimer_thread to handle destruction in the background:
```cpp
#include <cppurcu/cppurcu.h>

// Create a reclaimer_thread
auto reclaimer = std::make_shared<cppurcu::reclaimer_thread>();

// Create storage with reclaimer_thread
auto storage1 = cppurcu::create(std::make_shared<std::set<std::string>>(), reclaimer);

// reclaimer_thread can be used regardless of template type.
auto storage2 = cppurcu::create(std::make_shared<std::vector<int>>(), reclaimer);

// Somewhere in the source...(update)
storage1 = new_data;

// calls load, it gets the updated object.
// The old object is destroyed in the background thread(reclaimer_thread)
auto data = storage1.load();

```
**Note (behavior change)**<br>
- Previously, the system used the `reclaimer_thread` (and its mutex) to handle the previous data when updating with new data.
- Currently, the reader no longer uses the `reclaimer_thread` (or its mutex); the `reclaimer_thread` is only used by `storage<T>::update()`.
<br>

## Performance

*These tests use pre-built data structures to measure RCU performance without memory allocation overhead.*<br>
*Updates occur every 100ms.*

#### 300K items, 10 reader threads, 2 writer threads, 10 seconds:
- Ubuntu 22.04, AMD Ryzen 7 8845HS / 16G<br>
*Average of 5 benchmark runs*
<table>
  <thead>
    <tr>
      <th>Implementation</th>
      <th>Total read</th>
      <th>Relative Performance</th>
      <th>Update (200 attempts)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>20.05M</td>
      <td>1.0x (baseline)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+reclaimer</td>
      <td>368.6M</td>
      <td>18.0x</td>
      <td>200</td>
    </tr>
    <tr>
      <td>liburcu</td>
      <td>358.8M</td>
      <td>17.5x</td>
      <td>200</td>
    </tr>
  </tbody>
</table>
<br>

#### 1M items, 10 reader threads, 2 writer threads, 10 seconds:
- Ubuntu 22.04, AMD Ryzen 9 7945HX / 32G
<table>
  <thead>
    <tr>
      <th>Implementation</th>
      <th>Total read</th>
      <th>Relative Performance</th>
      <th>Update (200 attempts)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>80.6M</td>
      <td>1.0x (baseline)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+reclaimer</td>
      <td>377.2M</td>
      <td>4.6x</td>
      <td>200</td>
    </tr>
    <tr>
      <td>liburcu</td>
      <td>341.3M</td>
      <td>4.2x</td>
      <td>200</td>
    </tr>
  </tbody>
</table>
<br>

- RedHat 8.7, VM Intel Xeon(Cascadelake) 2.6Ghz / 48G
<table>
  <thead>
    <tr>
      <th>Implementation</th>
      <th>Total read</th>
      <th>Relative Performance</th>
      <th>Update (200 attempts)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>6.6M</td>
      <td>1.0x (baseline)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+reclaimer</td>
      <td>76.7M</td>
      <td>11.6x</td>
      <td>200</td>
    </tr>
    <tr>
      <td>liburcu</td>
      <td>79.6M</td>
      <td>12.0x</td>
      <td>175</td>
    </tr>
  </tbody>
</table>
Results may vary depending on environment and configuration.<br>
In this run, liburcu recorded approximately 175 updates, but this figure may vary across different environments and configurations.
<br>
<br>

## Installation

cppurcu is a header-only library. Copy the cppurcu/ directory to your include path:

```bash
# Copy headers to your project
cp -r cppurcu/ /path/to/your/project/include/

# Or add to your include path
g++ -I./path/to/cppurcu your_code.cpp
```

### Requirements

- C++17 or later
<br>

## Building the Tests

To build and run the included tests:

```bash
# Build with cppurcu and mutex
make

# Build with liburcu comparison (requires liburcu-dev)
make liburcu

# Run Tests
./rcu_bench 1000000  # Test with 1M items, memory required is 20GB
```

### Makefile Options

- `make` - Builds tests with cppurcu and mutex (default)
- `make liburcu` - Builds tests including liburcu comparison
- `make clean` - Removes build artifacts
<br>

## API Reference

### `cppurcu::storage<T>`

Main class for RCU-protected data storage.

#### Constructor
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

#### Methods

**`guard<T> load()`**
- Thread-safe
- Returns a guard object that provides access to the current data
- On the first load() within the scope, if there is new data, replace it with the new data and release the shared_ptr of the previous data.

**`void update(std::shared_ptr<const T> value)`**
- Publishes new data
- Deadlocks do not occur even when used concurrently within the same scope as the load() function.
- cppurcu does not reclaim old data itself; it delegates reclamation to std::shared_ptr.

**`void operator=(std::shared_ptr<const T> value)`**
- Convenience operator for updates
- Equivalent to `update(value)`

### `cppurcu::guard<T>`

RAII guard object returned by `storage<T>::load()`.

#### Notes
- Cannot be copied or moved. (thread-local only)
- Data remains valid while guard exists.
- Multiple guards nested on the same thread share the same snapshot.

#### Methods

**`const T* operator->()`**
- Provides smart pointer-like access to the data

**`explicit operator bool()`**
- Checks if the guard holds valid data

#### Example
```cpp
auto guard = storage.load();
if (guard) {
  guard->method();  // Safe access via operator->
}
// Guard destroyed here, data may be updated by next load()
```

### `cppurcu::guard_pack<Ts...>`

RAII guard pack for loading multiple storages at once.

#### Notes
- Cannot be copied or moved.
- Storages are loaded in order (left to right) and destruction occurs in reverse order (LIFO).
- If an exception occurs during construction, already constructed guards are properly destroyed.
- Must not outlive the storages it references.

#### Methods

**`template<std::size_t I> auto& get()`**
- Access guard at compile-time index I

**`static constexpr std::size_t size()`**
- Returns the number of guards in the pack

#### Example
```cpp
auto pack = cppurcu::make_guard_pack(storageA, storageB, storageC);

pack.get<0>()->method_a();
pack.get<1>()->method_b();
pack.get<2>()->method_c();

// Structured binding
const auto& [a, b, c] = cppurcu::make_guard_pack(storageA, storageB, storageC);
a->method_a();
b->method_b();
c->method_c();
```

### `cppurcu::make_guard_pack`

Factory function that creates a guard_pack from multiple storages.

#### Signature
```cpp
template<typename... Ts>
guard_pack<Ts...> make_guard_pack(storage<Ts>&... storages);
```

#### Parameters
- `storages`: References to storage instances to load from

#### Returns
- `guard_pack` containing guards for all storages

### `cppurcu::reclaimer_thread`

Background thread for handling object destruction.

#### Constructor
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
  - If interval is 0μs: Notification-only mode. Scans only when push() is called. Not recommended - may delay reclamation if updates are infrequent.


#### Methods

**`template<typename T> void push(std::shared_ptr<T> &&ptr)`**
- Queues an object for background destruction
- Usually called internally by storage::load() when data is updated

**`std::thread::id thread_id() const`**
- ID of the reclaimer_thread
<br>

## How It Works

cppurcu uses a multi-layer approach:

1. **`storage<T>`**: User-facing API integrating source, local, and guard
2. **`guard<T>`**: Return value of storage<T>::load(), RAII guard for snapshot isolation
3. **`source<T>`**: Maintains the authoritative data and version counter
4. **`local<T>`**: Thread-local caching (shallow copy only)
5. **`reclaimer_thread (optional)`**: Background thread for handling object destruction.

This design does not use:
- ABA problem solutions (no tagged pointers)
- Hazard pointers
- Epoch-based reclaimer

### Read Path

When creating a guard (each `load()` call):
1. Check cached version against source version (skipped if guard<T>.ref_count > 0 for snapshot isolation)
2. If unchanged: return cached raw pointer (fast path)
3. If changed: Updates the version, shared_ptr and raw pointers in the cache (slow path)
<br>If reclaimer_thread enabled: push old value to reclaimer queue

### Reclaimer Thread (Optional)

When enabled, reclaimer_thread handles object destruction in the background:
1. storage::load() pushes old shared_ptrs to the reclaim queue when data is updated
2. Worker thread scans periodically and removes entries when unique()
3. Non-unique objects remain in the queue until they become reclaimable
4. Objects are destroyed without blocking readers, reducing overhead for expensive destructors

### Thread Safety Guarantees

- Lock-free reads
- Thread-safe updates
- **Requirement**: `storage<T>` lifetime > thread lifetime
<br>

## Tests

The included tests compare three approaches:

1. **std::mutex** - Traditional lock-based protection
2. **cppurcu** - This library
3. **liburcu** - Widely-used RCU library (optional)

Run tests with different data sizes:
```bash
./rcu_bench 1000      # 1K items
./rcu_bench 100000    # 100K items
./rcu_bench 1000000   # 1M items, memory required is 20GB
```

