#include <cppurcu/cppurcu.h>

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cassert>
#include <unordered_map>

using namespace std;
using namespace cppurcu;

// TEST 1: Thread explosion - create/destroy 1000 threads, repeated 10 times
void test_thread_explosion() {
  cout << "\n[TEST 1] Thread Explosion (1000 threads * 10 rounds)\n";
  auto data = make_shared<int>(0);
  storage<int> store(data);
  atomic<int> max_value{0};
  atomic<bool> stop{false};

  thread writer([&]() {
    int val = 0;
    while (!stop) {
      store.update(make_shared<int>(val++));
      max_value = val;
      this_thread::sleep_for(chrono::milliseconds(10));
    }
  });

  for (int round = 0; round < 10; ++round) {
    cout << "  Round " << (round + 1) << "/10...\n";
    vector<thread> threads;
    for (int i = 0; i < 1000; ++i) {
      threads.emplace_back([&]() {
        for (int j = 0; j < 100; ++j) {
          auto val = store.load();
          assert(*val >= 0);
        }
      });
    }
    for (auto &t : threads) t.join();
  }
  stop = true;
  writer.join();
  cout << "  * PASSED\n";
}

// TEST 2: Ultra-fast updates - hammer updates with no sleep
void test_rapid_updates() {
  cout << "\n[TEST 2] Rapid Updates (10 writers, NO sleep)\n";
  auto data = make_shared<int>(0);
  storage<int> store(data);
  atomic<bool> stop{false};
  atomic<size_t> total_updates{0}, total_reads{0};

  vector<thread> writers;
  for (int i = 0; i < 10; ++i) {
    writers.emplace_back([&, base = i * 1000000]() {
      int val = base;
      while (!stop) {
        store.update(make_shared<int>(val++));
        total_updates.fetch_add(1);
      }
    });
  }

  vector<thread> readers;
  for (int i = 0; i < 20; ++i) {
    readers.emplace_back([&]() {
      while (!stop) {
        auto val = store.load();
        assert(*val >= 0);
        total_reads.fetch_add(1);
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(5));
  stop = true;
  for (auto &t : writers) t.join();
  for (auto &t : readers) t.join();
  cout << "  Updates: " << total_updates << ", Reads: " << total_reads << "\n";
  cout << "  * PASSED\n";
}

// TEST 3: Large objects - repeatedly replace 10MB objects
struct HugeObject {
  vector<int> data;
  HugeObject(size_t s) : data(s, 42) {}
};

void test_huge_objects() {
  cout << "\n[TEST 3] Huge Objects (10MB each, rapid replace)\n";
  const size_t obj_size = 10 * 1024 * 1024 / sizeof(int);
  auto data = make_shared<HugeObject>(obj_size);
  storage<HugeObject> store(data);
  atomic<bool> stop{false};
  atomic<int> updates{0};

  thread writer([&]() {
    while (!stop) {
      store.update(make_shared<HugeObject>(obj_size));
      updates.fetch_add(1);
      this_thread::sleep_for(chrono::milliseconds(50));
    }
  });

  vector<thread> readers;
  for (int i = 0; i < 10; ++i) {
    readers.emplace_back([&]() {
      while (!stop) {
        auto obj = store.load();
        assert(obj->data.size() == obj_size && obj->data[0] == 42);
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(3));
  stop = true;
  writer.join();
  for (auto &t : readers) t.join();
  cout << "  Updates: " << updates << "\n  * PASSED\n";
}

// TEST 4: Multiple storages - concurrently access 100 storage instances
void test_multiple_storages() {
  cout << "\n[TEST 4] Multiple Storages (100 instances, 50 threads)\n";
  const int num_storages = 100;
  vector<unique_ptr<storage<int>>> storages;
  for (int i = 0; i < num_storages; ++i) {
    storages.push_back(make_unique<storage<int>>(make_shared<int>(i)));
  }

  atomic<bool> stop{false};
  atomic<size_t> errors{0};
  vector<thread> threads;
  for (int tid = 0; tid < 50; ++tid) {
    threads.emplace_back([&, tid]() {
      mt19937 gen(tid);
      uniform_int_distribution<> dist(0, num_storages - 1);
      while (!stop) {
        int idx = dist(gen);
        auto val = storages[idx]->load();
        if (*val < 0 || *val >= num_storages * 1000) errors.fetch_add(1);
        if (tid % 10 == 0)
          storages[idx]->update(make_shared<int>(idx * 1000 + tid));
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(5));
  stop = true;
  for (auto &t : threads) t.join();
  cout << "  Errors: " << errors << "\n";
  assert(errors == 0);
  cout << "  * PASSED\n";
}

// TEST 5: Reclaimer extreme - fast updates with background reclamation
void test_reclaimer_stress() {
  cout << "\n[TEST 5] Reclaimer Thread Stress (10 writers, 30 readers)\n";
  auto rt = make_shared<reclaimer_thread>();
  auto data = make_shared<int>(0);
  storage<int> store(data, rt);
  atomic<bool> stop{false};
  atomic<size_t> updates{0}, reads{0};

  vector<thread> writers;
  for (int i = 0; i < 10; ++i) {
    writers.emplace_back([&, base = i * 10000]() {
      int val = base;
      while (!stop) {
        store.update(make_shared<int>(val++));
        updates.fetch_add(1);
        if (val % 100 == 0) this_thread::sleep_for(chrono::microseconds(100));
      }
    });
  }

  vector<thread> readers;
  for (int i = 0; i < 30; ++i) {
    readers.emplace_back([&]() {
      while (!stop) {
        auto val = store.load();
        assert(*val >= 0);
        reads.fetch_add(1);
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(5));
  stop = true;
  for (auto &t : writers) t.join();
  for (auto &t : readers) t.join();
  cout << "  Updates: " << updates << ", Reads: " << reads << "\n  * PASSED\n";
}

// TEST 6: Deeply nested guards - 5-level nesting during concurrent updates
void test_nested_guards_extreme() {
  cout << "\n[TEST 6] Nested Guards Extreme (5-level nesting)\n";
  using MapType = unordered_map<string, int>;
  auto data = make_shared<MapType>();
  (*data)["key"] = 0;
  storage<MapType> store(data);
  atomic<bool> stop{false};

  thread writer([&]() {
    int val = 1;
    while (!stop) {
      auto new_data = make_shared<MapType>();
      (*new_data)["key"] = val++;
      store.update(new_data);
      this_thread::sleep_for(chrono::milliseconds(10));
    }
  });

  vector<thread> readers;
  for (int i = 0; i < 10; ++i) {
    readers.emplace_back([&]() {
      while (!stop) {
        auto g1 = store.load(); int v1 = g1->at("key");
        { auto g2 = store.load(); int v2 = g2->at("key");
          { auto g3 = store.load(); int v3 = g3->at("key");
            { auto g4 = store.load(); int v4 = g4->at("key");
              { auto g5 = store.load(); int v5 = g5->at("key");
                assert(v1 == v2 && v2 == v3 && v3 == v4 && v4 == v5);
              }
            }
          }
        }
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(3));
  stop = true;
  writer.join();
  for (auto &t : readers) t.join();
  cout << "  * PASSED\n";
}

// TEST 7: Random workload - unpredictable mixed read/write pattern
void test_random_workload() {
  cout << "\n[TEST 7] Random Workload (50 threads, random ops)\n";
  auto data = make_shared<vector<int>>(1000, 42);
  storage<vector<int>> store(data);
  atomic<bool> stop{false};
  atomic<size_t> operations{0};

  vector<thread> threads;
  for (int i = 0; i < 50; ++i) {
    threads.emplace_back([&, seed = i]() {
      mt19937 gen(seed);
      uniform_int_distribution<> op_dist(0, 9);
      uniform_int_distribution<> sleep_dist(0, 100);
      while (!stop) {
        if (op_dist(gen) < 8) { // 80% reads
          auto vec = store.load();
          assert(vec->size() == 1000 && (*vec)[0] >= 42);
        } else { // 20% writes
          store.update(make_shared<vector<int>>(1000, 42 + seed));
        }
        operations.fetch_add(1);
        if (sleep_dist(gen) < 10)
          this_thread::sleep_for(chrono::microseconds(sleep_dist(gen)));
      }
    });
  }

  this_thread::sleep_for(chrono::seconds(5));
  stop = true;
  for (auto &t : threads) t.join();
  cout << "  Operations: " << operations << "\n  * PASSED\n";
}

int main() {
  try {
    test_thread_explosion();
    test_rapid_updates();
    test_huge_objects();
    test_multiple_storages();
    test_reclaimer_stress();
    test_nested_guards_extreme();
    test_random_workload();
    cout << "\n========================================\n";
    cout << "All tests passed!\n";
    cout << "========================================\n";
  } catch (const exception &e) {
    cout << "\n!!! TEST FAILED: " << e.what() << "\n";
    return 1;
  }
  return 0;
}


