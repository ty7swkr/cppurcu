
# cppurcu

A simple implementation of the C++ RCU (read-copy-update) user-space library that combines thread-local caching with version tracking using only the C++17 standard library.
<br>
<br>

## Overview

cppurcu is a header-only C++ library that provides an implementation of the RCU pattern for read-heavy workloads.

### Key Features

- **Header-Only**: Works with standard library only, no external dependencies
- **Lock-Free Reads**: Implemented so that contention is minimized on the read path after cache warm-up.
- **Optional Background Destruction**: retirement_thread can offload object destruction to a separate thread, reducing burden on reader threads
- **Performance**: about 5-15x improvement over mutex-based approaches in tested environments
<br>

## What is cppurcu?

Traditional synchronization mechanisms like mutexes cause contention between readers and writers. In cppurcu, reads are not blocked even during updates.

cppurcu caches thread-local pointers and checks for updates only via a lightweight version counter.
- Cached pointer return when version unchanged
- Reduces cache line bouncing between CPU cores using thread local storage
- Efficient performance for read-heavy scenarios

### API Overview

Unlike some RCU implementations that require reader registration, grace period management, and memory barriers, cppurcu provides an interface without them.

```cpp
storage = new_storage;      // Update example (shared_ptr:new_storage)
auto data = storage.load(); // Simple usage   (const pointer)
```
Simply call load().
<br>
<br>

## Performance

*These tests uses pre-built data structures to measure RCU performance without memory allocation overhead.*<br>
*Updates occur every 100ms.*

#### 300K items, 10 reader threads, 2 writer threads, 10 seconds:
- Ubuntu 22.04, AMD Ryzen 7 8845HS / 16G
<table>
  <thead>
    <tr>
      <th>Implementation</th>
      <th>Reads/sec</th>
      <th>Relative Performance</th>
      <th>Update (200 attempts)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>20.1M</td>
      <td>1.0x (baseline)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+retirement</td>
      <td>391.6.5M</td>
      <td>19.4x</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu</td>
      <td>404.8M</td>
      <td>20.1x</td>
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
      <th>Reads/sec</th>
      <th>Relative Performance</th>
      <th>Update (200 attempts)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>79.22M</td>
      <td>1.0x (baseline)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+retirement</td>
      <td>440.38M</td>
      <td>5.5x</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu</td>
      <td>433.4M</td>
      <td>5.4x</td>
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
      <th>Reads/sec</th>
      <th>Relative Performance</th>
      <th>Update (200 attempts)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>6.1M</td>
      <td>1.0x (baseline)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+retirement</td>
      <td>80.6M</td>
      <td>13.1x</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu</td>
      <td>85.6M</td>
      <td>13.9x</td>
      <td>200</td>
    </tr>
  </tbody>
</table>
Results may vary depending on environment and configuration.
<br>
<br>

## Quick Start

### Basic Usage
```cpp
#include <cppurcu/cppurcu.h>
#include <memory>
#include <string>
#include <unordered_map>

// Create a storage with initial data
auto storage = cppurcu::storage(std::make_shared<std::unordered_map>());

// Read (lock-free)
auto data = storage.load(); // const pointer
if (data->count("key") > 0) {
  // Use the data
}

// Update (immediate, old data reclaimed automatically by shared_ptr)
auto new_data = std::make_shared<std::unordered_map>();
(*new_data)["key"] = "value";
storage = new_data; // or storage.update(new_data);
```

### With Background Destruction (Optional)

For objects with expensive destructors, you can use a retirement_thread to handle destruction in the background:
```cpp
#include <cppurcu/cppurcu.h>

// Create a retirement thread
auto retirement = std::make_shared<cppurcu::retirement_thread>();

// Create storage with retirement thread
auto storage1 = cppurcu::storage(
  std::make_shared<std::unordered_map>(),
  retirement
);

// retirement_thread can be used regardless of template type.
auto storage2 = cppurcu::storage(
  std::make_shared<std::vector>(),
  retirement
);

// Read and update work the same way
auto data = storage1.load();
storage1.update(new_data);

// Old objects are destroyed in the background thread
```
**Note:** When using retirement_thread, the reader uses the retirement_thread's mutex when updating.
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
- Standard library with `<memory>`, `<atomic>`, `<unordered_map>`
<br>

## Building the Tests

To build and run the included tests:

```bash
# Build with cppurcu and mutex
make

# Build with liburcu comparison (requires liburcu-dev)
make liburcu

# Run Tests
./rcu_bench 1000000  # Test with 1M items, Memory required is 20G
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
storage(const std::shared_ptr<const T>& init_value,
        std::shared_ptr<retirement_thread> retirement = nullptr)

storage(std::shared_ptr<const T>&& init_value,
        std::shared_ptr<retirement_thread> retirement = nullptr)
```
Creates a new storage with initial data.

**Parameters:**
- `init_value`: Initial data to store
- `retirement` (optional): retirement_thread instance for background destruction. If nullptr, objects are destroyed in the reader's thread.

#### Methods

**`const T* load()`**
- Thread-safe
- Returns a pointer to the current data
- Lock-free read except during updates when using retirement_thread
- Returned pointer remains valid while the thread's TLS cache is maintained (e.g., before thread exit) or until storage is destroyed.<br>
*Note: During an update, calling `load()` again in the same scope may destroy previously loaded variables.*

**`void update(const std::shared_ptr<const T>& value)`**
- Publishes new data
- Old data is safely reclaimed after all readers finish

**`void operator=(const std::shared_ptr<const T>& value)`**
- Convenience operator for updates
- Equivalent to `update(value)`

### `cppurcu::retirement_thread`

Background thread for handling object destruction.

#### Constructor
```cpp
retirement_thread(bool wait_until_execution = false)
```
Creates a background retirement thread.

**Parameters:**
- `wait_until_execution` (optional): If true, constructor waits until the retirement thread starts. If false, returns immediately.

#### Methods

**`template<typename T> void push(const std::shared_ptr<T>& ptr)`**
- Queues an object for background destruction
- Usually called internally by storage::load() when data is updated

**Members:**
- `std::atomic<std::thread::id> thread_id`: ID of the retirement thread
<br>

## How It Works

cppurcu uses a multi-layer approach:

1. **`source<T>`**: Maintains the authoritative data and version counter
2. **`local<T>`**: Thread-local cache with version tracking
3. **`storage<T>`**: User-facing API combining both
4. **`retirement_thread`** (optional): Background thread for `<T>` object destruction

This design does not use:
- ABA problem solutions (no tagged pointers)
- Hazard pointers
- Epoch-based reclamation

### Read Path

On each `load()`:
1. Check cached version against source version
2. If unchanged: return cached pointer (fast path)
3. If changed: update cache and pointer (slow path)
<br>If retirement_thread enabled: push old value to retirement queue

### Retirement Thread (Optional)

When enabled, retirement_thread handles object destruction in the background:
1. storage::load() pushes old values to a double-buffered queue when version changes
2. Background thread periodically swaps and clears the queue
3. Objects are destroyed without blocking readers
4. Reduces overhead for objects with expensive destructors
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
./rcu_bench 1000000   # 1M items, Memory required is 20G
```
