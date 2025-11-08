#include <cppurcu/cppurcu.h>

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <cassert>
#include <cmath> // for abs in test_mixed_types

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
// Basic Tests (unchanged)
// ============================================================================

void test_construct_and_load()
{
  TEST_START("ConstructAndLoad")

  auto initial = make_shared<int>(42);
  storage<int> store(initial);

  auto data = store.load();
  assert(*data == 42);

  TEST_END()
}

void test_update_and_load()
{
  TEST_START("UpdateAndLoad")

  auto initial = make_shared<int>(42);
  storage<int> store(initial);

  auto new_value = make_shared<int>(73);
  store.update(new_value);

  auto data = store.load();
  assert(*data == 73);

  TEST_END()
}

void test_operator_assign()
{
  TEST_START("OperatorAssign")

  auto initial = make_shared<int>(42);
  storage<int> store(initial);

  auto new_value = make_shared<int>(99);
  store = new_value;

  auto data = store.load();
  assert(*data == 99);

  TEST_END()
}

void test_multiple_updates()
{
  TEST_START("MultipleUpdates")

  auto initial = make_shared<int>(0);
  storage<int> store(initial);

  for (int i = 1; i <= 10; ++i)
  {
    store.update(make_shared<int>(i));
    auto data = store.load();
    assert(*data == i);
  }

  TEST_END()
}

// ============================================================================
// Guard Tests (unchanged)
// ============================================================================
void test_guard()
{
  TEST_START("Guard")

  using MapType = unordered_map<string, int>;

  auto initial = make_shared<MapType>();
  (*initial)["key1"] = 100;

  storage<MapType> store(initial);

  auto data = store.load();
  assert(data->count("key1") > 0);
  assert(data->at("key1") == 100);

  auto updated = make_shared<MapType>();
  (*updated)["key1"] = 200;

  store.update(updated);

  auto data1 = store.load(); // guard
  assert(data1->at("key1") == 100);

  TEST_END()
}

void test_nested_guard()
{
  TEST_START("NestedGuard")

  using MapType = unordered_map<string, int>;

  auto initial = make_shared<MapType>();
  (*initial)["key1"] = 100;

  storage<MapType> store(initial);

  auto data = store.load();
  assert(data->count("key1") > 0);
  assert(data->at("key1") == 100);

  auto updated = make_shared<MapType>();
  (*updated)["key1"] = 200;
  store.update(updated);

  {
    auto data1 = store.load(); // guard
    assert(data1->at("key1") == 100);
  }

  TEST_END()
}

void test_nested_guard_update()
{
  TEST_START("NestedGuardUpdate")

  using MapType = unordered_map<string, int>;

  auto initial = make_shared<MapType>();
  (*initial)["key1"] = 100;

  storage<MapType> store(initial);

  {
    auto data  = store.load();
    auto data1 = store.load();
    assert(data->at("key1") == 100);  // same snapshot

    auto updated = make_shared<MapType>();
    (*updated)["key1"] = 200;
    store.update(updated);
  }  // All guards destroyed

  auto data2 = store.load();  // Now the new version
  assert(data2->at("key1") == 200);  // updated value

  TEST_END()
}

// ============================================================================
// Thread Safety Tests (unchanged)
// ============================================================================

void test_concurrent_reads()
{
  TEST_START("ConcurrentReads")

  auto initial = make_shared<int>(42);
  storage<int> store(initial);

  atomic<bool> stop{false};
  atomic<size_t> read_count{0};

  vector<thread> readers;
  for (int i = 0; i < 10; ++i)
  {
    readers.emplace_back([&]()
    {
      while (!stop.load())
      {
        auto data = store.load();
        assert(*data == 42);
        read_count++;
      }
    });
  }

  this_thread::sleep_for(chrono::milliseconds(100));
  stop = true;

  for (auto &t : readers)
  {
    t.join();
  }

  assert(read_count > 0);

  TEST_END()
}

void test_concurrent_read_write()
{
  TEST_START("ConcurrentReadWrite")

  auto initial = make_shared<int>(0);
  storage<int> store(initial);

  atomic<bool> stop{false};
  atomic<size_t> read_count{0};
  atomic<size_t> write_count{0};

  // Readers
  vector<thread> readers;
  for (int i = 0; i < 5; ++i)
  {
    readers.emplace_back([&]()
    {
      while (!stop.load())
      {
        store.load();
        read_count++;
      }
    });
  }

  // Writers
  vector<thread> writers;
  for (int i = 0; i < 2; ++i)
  {
    writers.emplace_back([&, i]()
    {
      int value = i *1000;
      while (!stop.load())
      {
        store.update(make_shared<int>(value++));
        write_count++;
        this_thread::sleep_for(chrono::microseconds(100));
      }
    });
  }

  this_thread::sleep_for(chrono::milliseconds(100));
  stop = true;

  for (auto &t : readers)
  {
    t.join();
  }
  for (auto &t : writers)
  {
    t.join();
  }

  assert(read_count > 0);
  assert(write_count > 0);

  TEST_END()
}

void test_reader_stability()
{
  TEST_START("ReaderStability")

  auto initial = make_shared<int>(1);
  storage<int> store(initial);

  atomic<bool> stop{false};

  // Writer thread
  thread writer([&]()
  {
    int value = 2;
    while (!stop.load())
    {
      store.update(make_shared<int>(value++));
      this_thread::sleep_for(chrono::microseconds(100));
    }
  });

  // Reader thread - verify pointer remains stable during read
  thread reader([&]()
  {
    while (!stop.load())
    {
      auto data1 = store.load();
      int val1 = *data1;

      // The same load() call should return the same pointer
      auto data2 = store.load();
      int val2 = *data2;

      // Values might differ due to version update, but should be consistent
      assert(data1.operator *() == data2.operator *() || val1 <= val2);
    }
  });

  this_thread::sleep_for(chrono::milliseconds(100));
  stop = true;

  writer.join();
  reader.join();

  TEST_END()
}

// ============================================================================
// Dataset Change Tests (unchanged)
// ============================================================================

void test_dataset_changes()
{
  TEST_START("DatasetChanges")

  auto initial = make_shared<int>(0);
  storage<int> store(initial);

  atomic<bool> stop{false};
  atomic<bool> value_changed{false};
  atomic<bool> reader_started{false};

  // Reader thread - checks if values actually change
  thread reader([&]()
  {
    reader_started = true;
    int prev_value = -1;
    while (!stop.load())
    {
      auto data = store.load();
      int current_value = *data;

      if (prev_value != -1 && prev_value != current_value)
      {
        value_changed = true;
      }

      prev_value = current_value;
      this_thread::sleep_for(chrono::milliseconds(5));
    }
  });

  // Wait until the reader has started
  while (!reader_started.load())
  {
    this_thread::sleep_for(chrono::milliseconds(1));
  }

  // Writer thread - updates value every 10ms
  thread writer([&]()
  {
    for (int value = 1; value <= 100; ++value)
    {
      store.update(make_shared<int>(value));
      this_thread::sleep_for(chrono::milliseconds(10));
    }
  });

  writer.join();

  // After the writer finishes, give the reader enough time to verify
  this_thread::sleep_for(chrono::milliseconds(50));
  stop = true;
  reader.join();

  assert(value_changed.load());

  TEST_END()
}

void test_multiple_dataset_changes()
{
  TEST_START("MultipleDatasetChanges")

  using MapType = unordered_map<string, int>;

  auto initial = make_shared<MapType>();
  (*initial)["count"] = 0;
  (*initial)["extra"] = 0;

  storage<MapType> store(initial);

  atomic<bool> stop{false};
  atomic<int> observed_values{0};
  atomic<int> readers_started{0};

  // Reader threads - verify they see changing values
  vector<thread> readers;
  for (int tid = 0; tid < 3; ++tid)
  {
    readers.emplace_back([&]()
    {
      readers_started.fetch_add(1);
      unordered_map<int, bool> seen_counts;

      while (!stop.load())
      {
        auto data = store.load();

        // Check key existence
        if (data->count("count") == 0 || data->count("extra") == 0)
        {
          continue;
        }

        int count = data->at("count");

        if (seen_counts.count(count) == 0)
        {
          seen_counts[count] = true;
          observed_values.fetch_add(1);
        }

        // Verify consistency
        assert(data->at("extra") == count *10);

        this_thread::sleep_for(chrono::milliseconds(15));
      }
    });
  }

  // Wait until all readers have started
  while (readers_started.load() < 3)
  {
    this_thread::sleep_for(chrono::milliseconds(1));
  }

  // Writer thread - updates map with increasing count
  thread writer([&]()
  {
    for (int i = 1; i <= 50; ++i)
    {
      auto updated = make_shared<MapType>();
      (*updated)["count"] = i;
      (*updated)["extra"] = i *10;
      store.update(updated);
      this_thread::sleep_for(chrono::milliseconds(20));
    }
  });

  writer.join();

  // After the writer finishes, give readers enough time to verify
  this_thread::sleep_for(chrono::milliseconds(100));
  stop = true;

  for (auto &t : readers)
  {
    t.join();
  }

  // Should have observed multiple different values
  assert(observed_values > 3);

  TEST_END()
}

void test_rapid_updates()
{
  TEST_START("RapidUpdates")

  auto initial = make_shared<int>(0);
  storage<int> store(initial);

  atomic<bool> stop{false};
  atomic<int> max_value_seen{0};
  atomic<bool> reader_started{false};

  // Reader - should see increasing values
  thread reader([&]()
  {
    reader_started = true;
    int prev_max = 0;
    while (!stop.load())
    {
      auto data = store.load();
      int current = *data;

      if (current > prev_max)
      {
        prev_max = current;
        max_value_seen = current;
      }

      // Value should never decrease
      assert(current >= 0 && current <= 1000);
    }
  });

  // Wait until the reader has started
  while (!reader_started.load())
  {
    this_thread::sleep_for(chrono::microseconds(10));
  }

  // Rapid writer - updates as fast as possible
  thread writer([&]()
  {
    for (int value = 1; value <= 1000; ++value)
    {
      store.update(make_shared<int>(value));
      // Very short sleep to give the reader a chance to observe intermediate values
      this_thread::sleep_for(chrono::microseconds(10));
    }
  });

  writer.join();

  // After the writer finishes, give the reader enough time to verify
  this_thread::sleep_for(chrono::milliseconds(50));
  stop = true;
  reader.join();

  // Should have seen significant progress
  assert(max_value_seen > 100);

  TEST_END()
}

void test_multiple_storage_instances()
{
  TEST_START("MultipleStorageInstances")

  // Create multiple storage instances of the same type
  auto data1 = make_shared<int>(100);
  auto data2 = make_shared<int>(200);
  auto data3 = make_shared<int>(300);

  storage<int> s1(data1);
  storage<int> s2(data2);
  storage<int> s3(data3);

  // Check initial values
  assert(*s1.load() == 100);
  assert(*s2.load() == 200);
  assert(*s3.load() == 300);

  atomic<bool> stop{false};
  atomic<int> errors{0};

  // Use each storage independently across multiple threads
  vector<thread> threads;
  for (int tid = 0; tid < 5; ++tid)
  {
    threads.emplace_back([&, tid]()
    {
      while (!stop.load())
      {
        // Verify each storage keeps its own independent value
        int v1 = *s1.load();
        int v2 = *s2.load();
        int v3 = *s3.load();

        // s1: range 100~199
        // s2: range 200~299
        // s3: range 300~399
        if (v1 < 100 || v1 >= 200 ||
            v2 < 200 || v2 >= 300 ||
            v3 < 300 || v3 >= 400)
        {
          errors.fetch_add(1);
        }

        this_thread::sleep_for(chrono::microseconds(100));
      }
    });
  }

  // Writer threads - update each storage independently
  thread writer1([&]()
  {
    for (int i = 100; i < 200 && !stop.load(); ++i)
    {
      s1.update(make_shared<int>(i));
      this_thread::sleep_for(chrono::milliseconds(10));
    }
  });

  thread writer2([&]()
  {
    for (int i = 200; i < 300 && !stop.load(); ++i)
    {
      s2.update(make_shared<int>(i));
      this_thread::sleep_for(chrono::milliseconds(10));
    }
  });

  thread writer3([&]()
  {
    for (int i = 300; i < 400 && !stop.load(); ++i)
    {
      s3.update(make_shared<int>(i));
      this_thread::sleep_for(chrono::milliseconds(10));
    }
  });

  // Run long enough
  this_thread::sleep_for(chrono::milliseconds(500));
  stop = true;

  writer1.join();
  writer2.join();
  writer3.join();

  for (auto &t : threads)
  {
    t.join();
  }

  // Verify storages operated independently
  assert(errors.load() == 0);

  TEST_END()
}

void test_memory_cleanup()
{
  TEST_START("MemoryCleanup")

  // Track deallocation with weak_ptr
  weak_ptr<int> weak1, weak2, weak3;

  {
    auto data1 = make_shared<int>(100);
    weak1 = data1;

    storage<int> store(data1);

    // Reference count of data1: data1(1) + store.source_.value_(1) = 2
    assert(weak1.use_count() == 2);

    // Update with new data
    auto data2 = make_shared<int>(200);
    weak2 = data2;
    store.update(data2);

    // data1: released from store.source_.value_
    // but it may still remain in the thread_local cache
    // data1(1) + thread_local cache(1) = 1 or 2
    assert(weak1.use_count() >= 1);

    // Reference count of data2: data2(1) + store.source_.value_(1) + cache(1) = 3
    assert(weak2.use_count() >= 2);

    // Another update
    auto data3 = make_shared<int>(300);
    weak3 = data3;
    store.update(data3);

    // Check current value
    assert(*store.load() == 300);

    // Read on multiple threads
    atomic<bool> stop{false};
    vector<thread> readers;

    for (int i = 0; i < 5; ++i)
    {
      readers.emplace_back([&]()
      {
        while (!stop.load())
        {
          auto val = store.load();
          assert(*val == 300);
        }
      });
    }

    this_thread::sleep_for(chrono::milliseconds(50));
    stop = true;

    for (auto &t : readers)
    {
      t.join();
    }

    // After all threads have finished, only data3 should remain
    // data3: data3(1) + store.source_.value_(1) + caches = >= 2
    assert(weak3.use_count() >= 2);
  }
  // When store is destroyed, all data is released

  // Wait a moment (time for thread_local destructors)
  this_thread::sleep_for(chrono::milliseconds(100));

  // Verify all data has been released
  // The main thread's thread_local may still be alive
  // weak1 and weak2 should definitely be released
  assert(weak1.expired() || weak1.use_count() <= 1);
  assert(weak2.expired() || weak2.use_count() <= 1);

  TEST_END()
}

// ============================================================================
// Reclaimer Thread Tests (modified for assertion in destructor)
// ============================================================================

// ============================================================================
// Data Structures for Reclaimer Tests
// (Expected thread ID check added to destructor)
// ============================================================================

struct TestObject
{
  int value;
  // **ADDITION:** Expected thread ID for this object's destruction
  std::thread::id expected_destroy_thread_id;

  TestObject(int v, std::thread::id expected_id = std::this_thread::get_id())
    : value(v), expected_destroy_thread_id(expected_id)
  {
    cout << "[Create ] Object " << value << "\n";
  }

  ~TestObject()
  {
    std::thread::id current_id = std::this_thread::get_id();
    cout << "[Destroy] Object " << value << " (thread: "
         << current_id << ") " << "expected:" << expected_destroy_thread_id << "\n";
    // **CORE VERIFICATION:** Assert that the current destruction thread ID matches the expected ID
    assert(current_id == expected_destroy_thread_id);
  }
};

struct TypeA
{
  int value;
  std::thread::id expected_destroy_thread_id;

  TypeA(int v, std::thread::id expected_id = std::this_thread::get_id())
    : value(v), expected_destroy_thread_id(expected_id)
  {
    cout << "[Create ] TypeA " << value << "\n";
  }
  ~TypeA()
  {
    assert(std::this_thread::get_id() == expected_destroy_thread_id);
    cout << "[Destroy] TypeA " << value << " (thread: "
         << this_thread::get_id() << ") " << "expected:" << expected_destroy_thread_id << "\n";
  }
};

struct TypeB
{
  string name;
  std::thread::id expected_destroy_thread_id;

  TypeB(const string &n, std::thread::id expected_id = std::this_thread::get_id())
    : name(n), expected_destroy_thread_id(expected_id)
  {
    cout << "[Create ] TypeB " << name << "\n";
  }
  ~TypeB()
  {
    assert(std::this_thread::get_id() == expected_destroy_thread_id);
    cout << "[Destroy] TypeB " << name << " (thread: "
         << this_thread::get_id() << ") " << "expected:" << expected_destroy_thread_id << "\n";
  }
};

struct TypeC
{
  double data;
  std::thread::id expected_destroy_thread_id;

  TypeC(double d, std::thread::id expected_id = std::this_thread::get_id())
    : data(d), expected_destroy_thread_id(expected_id)
  {
    cout << "[Create ] TypeC " << data << "\n";
  }
  ~TypeC()
  {
    cout << "[Destroy] TypeC " << data << " (thread: "
         << this_thread::get_id() << ") " << "expected:" << expected_destroy_thread_id << "\n";

    assert(std::this_thread::get_id() == expected_destroy_thread_id);
  }
};

void test_reclaimer()
{
  TEST_START("ReclamerThreadTest")

  {
    cout << "Main thread ID: " << this_thread::get_id() << "\n";

    // Create reclaimer_thread
    auto rt = make_shared<reclaimer_thread>(true);
    cout << "reclaimer_thread ID: " << rt->thread_id() << endl << endl;
    std::thread::id reclaimer_id = rt->thread_id();

    // Initial object is created. Its destruction is expected on the reclaimer thread because 'store' is destroyed after 'rt' reference is created.
    storage<TestObject> store(make_shared<TestObject>(100, reclaimer_id), rt);

    cout << "Initial value: " << store.load()->value << "\n\n";

    int final_value = 0;

    // Multiple updates (101 ~ 105)
    for (int i = 1; i <= 5; ++i)
    {
      final_value = 100 + i;
      // Set the expected ID for the new object to the reclaimer thread ID
      auto new_val = make_shared<TestObject>(final_value, reclaimer_id);
      store.update(new_val);
      cout << "Updated to: " << store.load()->value << "\n";
      this_thread::sleep_for(chrono::milliseconds(150));
    }

    cout << "\nWaiting for reclaimer thread cleanup...\n";
    this_thread::sleep_for(chrono::milliseconds(500));

    // Final value verification
    assert(store.load()->value == final_value);
    // Object destruction thread ID verification is done inside the TestObject destructor.

    cout << "\nFinal value: " << store.load()->value << "\n";
  }

  cout << "----------------------------------------" << endl;
  TEST_END()
}

void test_reclaimer_multithread()
{
  TEST_START("ReclamerMultiThreadTest")
  {
    cout << "Main thread ID: " << this_thread::get_id() << "\n";

    // Create reclaimer_thread
    auto rt = make_shared<reclaimer_thread>(true);
    cout << "reclaimer_thread ID: " << rt->thread_id() << "\n\n";
    std::thread::id reclaimer_id = rt->thread_id();

    // Initial object expects destruction on reclaimer thread
    storage<TestObject> store(make_shared<TestObject>(0, reclaimer_id), rt);

    atomic<bool> stop{false};
    atomic<int> read_count{0};
    atomic<int> write_count{0};
    atomic<int> last_written_value{0};

    // Reader threads
    vector<thread> readers;
    for (int i = 0; i < 5; ++i)
    {
      readers.emplace_back([&, tid = i]()
      {
        cout << "Reader " << tid << " started (thread: "
             << this_thread::get_id() << ")\n";

        while (!stop.load())
        {
          auto obj = store.load();
          assert(obj->value >= 0);
          read_count++;
          this_thread::sleep_for(chrono::milliseconds(10));
        }
      });
    }

    // Writer threads
    vector<thread> writers;
    for (int i = 0; i < 1; ++i)
    {
      writers.emplace_back([&, tid = i]()
      {
        cout << "Writer " << tid << " started (thread: "
             << this_thread::get_id() << ")\n";

        int base = tid *1000;
        int count = 0;

        while (!stop.load())
        {
          last_written_value = base + count;
          // Set the expected ID for the new object to the reclaimer thread ID
          auto new_obj = make_shared<TestObject>(base + count, reclaimer_id);
          store.update(new_obj);
          write_count++;
          count++;
          this_thread::sleep_for(chrono::milliseconds(200));
        }
      });
    }

    cout << "\nRunning for 3 seconds...\n\n";
    this_thread::sleep_for(chrono::seconds(3));

    stop = true;

    for (auto &t : readers) t.join();
    for (auto &t : writers) t.join();

    cout << "Test completed!\n";
    cout << "Total reads : " << read_count << "\n";
    cout << "Total writes: " << write_count << "\n";
    cout << "Final value : " << store.load()->value << "\n\n";

    // Verify minimum read/write operations
    assert(read_count > 10);
    assert(write_count > 10);
    assert(store.load()->value == last_written_value);

    cout << "Waiting for reclaimer cleanup...\n";
    this_thread::sleep_for(chrono::milliseconds(500));
  }
  cout << "Cleanup done! ----------------------------------------\n";

  TEST_END()
}

void test_mixed_types()
{
  TEST_START("MixedTypesReclamerTest")

  {
    cout << "Main thread ID: " << this_thread::get_id() << "\n";

    // Share a single reclaimer_thread
    auto rt = make_shared<reclaimer_thread>(true);
    cout << "reclaimer_thread ID: " << rt->thread_id() << "\n\n";
    std::thread::id reclaimer_id = rt->thread_id();

    // Initial objects expect destruction on reclaimer thread
    storage<TypeA> storeA(make_shared<TypeA>(100, reclaimer_id), rt);
    storage<TypeB> storeB(make_shared<TypeB>("initial", reclaimer_id), rt);
    storage<TypeC> storeC(make_shared<TypeC>(3.14, reclaimer_id), rt);

    // Initialize thread_local cache via initial load
    storeA.load();
    storeB.load();
    storeC.load();

    cout << "Initial values loaded\n\n";

    int final_A_value = 100;
    // Update TypeA
    cout << "--- Updating TypeA ---\n";
    for (int i = 1; i <= 3; ++i)
    {
      final_A_value = 100 + i;
      // Set the expected ID for the new object to the reclaimer thread ID
      storeA.update(make_shared<TypeA>(final_A_value, reclaimer_id));
      storeA.load();
      this_thread::sleep_for(chrono::milliseconds(150));
    }

    string final_B_name = "initial";
    // Update TypeB
    cout << "\n--- Updating TypeB ---\n";
    for (int i = 1; i <= 3; ++i)
    {
      final_B_name = "update" + to_string(i);
      // Set the expected ID for the new object to the reclaimer thread ID
      storeB.update(make_shared<TypeB>(final_B_name, reclaimer_id));
      storeB.load();
      this_thread::sleep_for(chrono::milliseconds(150));
    }

    double final_C_data = 3.14;
    // Update TypeC
    cout << "\n--- Updating TypeC ---\n";
    for (int i = 1; i <= 3; ++i)
    {
      final_C_data = 3.14 + i;
      // Set the expected ID for the new object to the reclaimer thread ID
      storeC.update(make_shared<TypeC>(final_C_data, reclaimer_id));
      storeC.load();
      this_thread::sleep_for(chrono::milliseconds(150));
    }

    cout << "\n--- Waiting for cleanup ---\n";
    this_thread::sleep_for(chrono::milliseconds(500));

    cout << "\nFinal values:\n";
    cout << "TypeA: " << storeA.load()->value << "\n";
    cout << "TypeB: " << storeB.load()->name << "\n";
    cout << "TypeC: " << storeC.load()->data << "\n";

    // Final value verification
    assert(storeA.load()->value == final_A_value);
    assert(storeB.load()->name == final_B_name);
    assert(abs(storeC.load()->data - final_C_data) < 0.0001);
    // Object destruction thread ID verification is done inside each object's destructor.
  }

  cout << "Test completed! ----------------------------------------\n";
  TEST_END()
}

// ============================================================================
// Main
// ============================================================================

int main()
{
  cout << "========================================" << endl;
  cout << "cppurcu::storage Unit Tests" << endl;
  cout << "========================================" << endl;

  // Note: immediately flag is not used in the provided tests,
  // but keeping it here for completeness

  test_construct_and_load();
  test_update_and_load();
  test_operator_assign();
  test_multiple_updates();

  cout << "\n--- Guard ---" << endl;
  test_guard();
  test_nested_guard();
  test_nested_guard_update();

  cout << "\n--- Thread Safety ---" << endl;
  test_concurrent_reads();
  test_concurrent_read_write();
  test_reader_stability();

  cout << "\n--- Dataset Changes ---" << endl;
  test_dataset_changes();
  test_multiple_dataset_changes();
  test_rapid_updates();

  cout << "\n--- Advanced Tests ---" << endl;
  test_multiple_storage_instances();
  test_memory_cleanup();

  cout << "\n--- reclaimer_thread Tests ---" << endl;
  test_reclaimer();
  test_reclaimer_multithread();
  test_mixed_types();

  cout << "\n========================================" << endl;
  cout << "All tests passed!" << endl;
  cout << "========================================" << endl;

  return 0;
}
