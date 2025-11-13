#include <cppurcu/cppurcu.h>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <cassert>
#include <stdexcept>

using namespace std;
using namespace cppurcu;

#define TEST_START(name) \
  cout << "[ TEST ] " << name << " ... "; \
  try {
#define TEST_END() \
    cout << "OK" << endl; \
  } catch (const exception &e) { \
    cout << "FAILED: " << e.what() << endl; \
    exit(1); \
  }

// ============================================================================
// TEST 1: Memory leak verification (not detectable by TSan; requires LSan)
// ============================================================================

void test_memory_leak_detection() {
  TEST_START("MemoryLeakDetection")

  weak_ptr<int> weak_initial, weak_updated;
  {
    auto initial = make_shared<int>(100);
    weak_initial = initial;

    storage<int> store(initial);

    // Reference count: initial(1) + store.source_(1) = 2
    assert(weak_initial.use_count() == 2);

    // Update
    auto updated = make_shared<int>(200);
    weak_updated = updated;
    store.update(updated);

    // Load to refresh the cache
    auto g = store.load();
    assert(*g == 200);

    // 'updated' must still be alive
    assert(!weak_updated.expired());

  } // storage destroyed

  // After a short delay, all references should be released
  this_thread::sleep_for(chrono::milliseconds(10));

  // Check for memory leaks
  assert(weak_initial.expired() || weak_initial.use_count() <= 1);
  assert(weak_updated.expired() || weak_updated.use_count() <= 1);

  TEST_END()
}

// ============================================================================
// TEST 2: nullptr handling (not detectable by TSan) — updated
// ============================================================================

void test_nullptr_handling() {
  TEST_START("NullptrHandling")

  // Initialize with nullptr
  storage<int> store(nullptr);

  {
    auto g = store.load();

    // Test operator bool()
    assert(!g);  // Must be false because it is nullptr
  } // g destroyed

  // Update with a real value
  store.update(make_shared<int>(42));

  {
    auto g2 = store.load();
    assert(g2);  // Must be true now
    assert(*g2 == 42);
  }

  // Back to nullptr
  store.update(nullptr);

  {
    auto g3 = store.load();
    assert(!g3);
  }

  TEST_END()
}

// ============================================================================
// TEST 3: Storage destruction timing (not detectable by TSan)
// ============================================================================

void test_storage_destruction_timing() {
  TEST_START("StorageDestructionTiming")

  atomic<bool> stop{false};
  atomic<bool> error{false};
  atomic<int> operations{0};

  // Thread that rapidly creates/destroys Storage
  thread creator([&]() {
    try {
      while (!stop) {
        {
          storage<int> store(make_shared<int>(42));
          auto g = store.load();
          assert(*g == 42);
          operations.fetch_add(1);
        } // storage destroyed immediately
        this_thread::yield();
      }
    } catch (...) {
      error = true;
    }
  });

  // Worker threads performing concurrent activity
  vector<thread> workers;
  for (int i = 0; i < 5; ++i) {
    workers.emplace_back([&, i]() {
      try {
        while (!stop) {
          storage<int> temp_store(make_shared<int>(i));
          auto g = temp_store.load();
          assert(*g == i);
          operations.fetch_add(1);
          this_thread::yield();
        }
      } catch (...) {
        error = true;
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(2));
  stop = true;

  creator.join();
  for (auto &t : workers) t.join();

  assert(!error);
  assert(operations > 1000);

  TEST_END()
}

// ============================================================================
// TEST 4: Exception safety (not detectable by TSan)
// ============================================================================

struct MayThrowObject {
  int value;
  static atomic<int> throw_counter;
  static atomic<int> constructed;
  static atomic<int> destructed;

  MayThrowObject(int v) : value(v) {
    constructed.fetch_add(1);
    // Throw once every 10 constructions
    if (throw_counter.fetch_add(1) % 10 == 0) {
      throw runtime_error("Construction failed");
    }
  }

  ~MayThrowObject() {
    destructed.fetch_add(1);
  }
};

atomic<int> MayThrowObject::throw_counter{0};
atomic<int> MayThrowObject::constructed{0};
atomic<int> MayThrowObject::destructed{0};

void test_exception_safety() {
  TEST_START("ExceptionSafety")

  MayThrowObject::throw_counter = 0;
  MayThrowObject::constructed = 0;
  MayThrowObject::destructed = 0;

  atomic<int> successful_creates{0};
  atomic<int> failed_creates{0};
  atomic<bool> stop{false};

  vector<thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&, i]() {
      while (!stop) {
        try {
          {
            auto obj = make_shared<MayThrowObject>(i);
            storage<MayThrowObject> store(obj);
            auto g = store.load();
            assert(g->value == i);
            successful_creates.fetch_add(1);
          }
        } catch (const runtime_error &) {
          failed_creates.fetch_add(1);
        }
        this_thread::sleep_for(chrono::milliseconds(1));
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(2));
  stop = true;

  for (auto &t : threads) t.join();

  this_thread::sleep_for(chrono::milliseconds(500));

  int constructed = MayThrowObject::constructed.load();
  int destructed = MayThrowObject::destructed.load();
  int successful = successful_creates.load();
  int failed = failed_creates.load();

  cout << "\n    Constructed: " << constructed
       << ", Destructed: " << destructed
       << ", Successful: " << successful
       << ", Failed: " << failed << "\n    "
       << "Successful + Failed: " << successful + failed << "\n    ";

  // Validate the relationship
  assert(constructed == successful + failed);  // total attempts = success + failure

  // 'destructed' should equal 'successful', but may be slightly less due to thread_local caches
  int diff = successful - destructed;
  assert(diff >= 0 && diff <= 10);  // up to 10 threads worth of caches

  assert(successful > 0);
  assert(failed > 0);

  TEST_END()
}

// ============================================================================
// TEST 5: Guard lifetime & data retention, snapshot isolation (not detectable by TSan)
// ============================================================================

void test_guard_lifetime_and_snapshot() {
  TEST_START("GuardLifetimeAndSnapshot")

  weak_ptr<const int> weak_data;

  {
    auto data = make_shared<int>(42);
    weak_data = data;

    storage<int> store(data);

    {
      // Access within a guard scope
      auto g1 = store.load();
      assert(*g1 == 42);

      // Perform an update
      store.update(make_shared<int>(100));

      // Guards within the same scope must see the same version (snapshot isolation)
      auto g2 = store.load();
      assert(*g1 == *g2);  // both see 42

    } // guard destroyed

    // A new guard should now see the updated value
    auto g3 = store.load();
    assert(*g3 == 100);

  } // storage destroyed

  // After a short delay, ensure the data is released
  this_thread::sleep_for(chrono::milliseconds(10));

  // There must be no memory leaks
  assert(weak_data.expired() || weak_data.use_count() <= 1);

  TEST_END()
}

// ============================================================================
// TEST 6: Destroy storages concurrently across threads (timing issue that TSan cannot catch)
// ============================================================================

void test_concurrent_storage_destruction() {
  TEST_START("ConcurrentStorageDestruction")

  atomic<int> active_storages{0};
  atomic<bool> stop{false};
  atomic<int> errors{0};

  vector<thread> threads;
  for (int i = 0; i < 20; ++i) {
    threads.emplace_back([&, i]() {
      while (!stop) {
        try {
          active_storages.fetch_add(1);

          {
            storage<int> store(make_shared<int>(i));

            // Create multiple guards
            auto g1 = store.load();
            auto g2 = store.load();
            auto g3 = store.load();

            assert(*g1 == i);
            assert(*g2 == i);
            assert(*g3 == i);

          } // guards destroyed

          {
            // New guard after an update
            storage<int> store2(make_shared<int>(i * 100));
            store2.update(make_shared<int>(i * 200));

            auto g4 = store2.load();
            assert(*g4 == i * 200);

          } // storage and guard destroyed here

          active_storages.fetch_sub(1);
          this_thread::yield();

        } catch (...) {
          errors.fetch_add(1);
        }
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(3));
  stop = true;

  for (auto &t : threads) t.join();

  assert(errors == 0);
  assert(active_storages == 0);

  TEST_END()
}

// ============================================================================
// TEST 7: Heavy updates without a reclaimer (validate memory management)
// ============================================================================

void test_without_reclaimer_memory() {
  TEST_START("WithoutReclaimerMemory")

  weak_ptr<int> weak_refs[100];

  {
    storage<int> store(make_shared<int>(0));

    // Perform 100 updates
    for (int i = 1; i <= 100; ++i) {
      auto new_val = make_shared<int>(i);
      weak_refs[i-1] = new_val;
      store.update(new_val);

      // Load to refresh the cache
      auto g = store.load();
      assert(*g == i);
    }

    // Only the last value should remain alive
    assert(*store.load() == 100);

  } // storage destroyed

  this_thread::sleep_for(chrono::milliseconds(100));

  // All data should be released
  int alive_count = 0;
  for (int i = 0; i < 100; ++i) {
    if (!weak_refs[i].expired()) {
      alive_count++;
    }
  }

  // The last value may remain in a thread_local cache
  assert(alive_count <= 1);

  TEST_END()
}

// ============================================================================
// TEST 8: Prevent circular references for complex objects (logic bug guard)
// ============================================================================

struct Node {
  int value;
  weak_ptr<Node> next;  // weak_ptr to prevent cycles

  Node(int v) : value(v) {}
};

void test_circular_reference_prevention() {
  weak_ptr<Node> weak_node1, weak_node2;

  thread t([&]() {
    auto node1 = make_shared<Node>(1);
    auto node2 = make_shared<Node>(2);

    weak_node1 = node1;
    weak_node2 = node2;

    storage<Node> store1(node1);
    storage<Node> store2(node2);

    auto g1 = store1.load();
    auto g2 = store2.load();
  });

  t.join();  // Automatic cleanup on thread exit

  assert(weak_node1.expired());
  assert(weak_node2.expired());
}

// ============================================================================
// Main
// ============================================================================

int main() {
  try {
    test_memory_leak_detection();
    test_nullptr_handling();
    test_storage_destruction_timing();
    test_exception_safety();
    test_guard_lifetime_and_snapshot();
    test_concurrent_storage_destruction();
    test_without_reclaimer_memory();
    test_circular_reference_prevention();

    cout << "\n========================================" << endl;
    cout << "All additional tests passed!" << endl;
    cout << "========================================" << endl;
  } catch (const exception &e) {
    cout << "\n!!! TEST FAILED: " << e.what() << endl;
    return 1;
  }

  return 0;
}
