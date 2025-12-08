# cppurcu

A simple implementation of the C++ RCU (read-copy-update) user-space library with RAII-based snapshot isolation, header-only and dependency-free, using only the C++17 standard library.
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

When you need to load (snapshot) multiple storages in a single line, you can use `cppurcu::load(storage<Ts>&...)` as follows:
```cpp
#include <cppurcu/cppurcu.h>

// Multiple data storages
auto storageA = cppurcu::create(...);
auto storageB = cppurcu::create(...);
auto storageC = cppurcu::create(...);

// Load all – maintains the same snapshot point
const auto &[a, b, c] = cppurcu::load(storageA, storageB, storageC);

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
  auto pack = cppurcu::load(storageA, storageB, storageC);
  // or `guard<T>` object from storage<T>::load()
  auto pack = cppurcu::make_guard_pack(storageA.load(), storageB.load(), storageC.load());
  .....
  // Even if storageA and storageC are used somewhere in the call chain 
  // within the calculate(...) function,
  // the pack maintains a consistent snapshot of storageA, B, and C.
  my_class.calculate(...);
  .....
}
```

**Note:**<br>
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

For detailed API documentation, see [API.md](docs/API.md).

Quick reference:
- `cppurcu::storage<T>` - Main RCU-protected data storage
- `cppurcu::guard<T>` - RAII guard for snapshot isolation
- `cppurcu::guard_pack<Ts...>` - Multi-storage snapshot helper
- `cppurcu::reclaimer_thread` - Background destruction handler
<br>

## How It Works

Main classes:

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

