#include <cppurcu/cppurcu.h>

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <cassert>

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
// Test Data Types
// ============================================================================

struct Config {
  int version;
  string name;
  Config(int v, const string& n) : version(v), name(n) {}
};

struct Cache {
  int hits;
  int misses;
  Cache(int h, int m) : hits(h), misses(m) {}
};

struct State {
  bool active;
  double value;
  State(bool a, double v) : active(a), value(v) {}
};

// ============================================================================
// Basic Tests
// ============================================================================

void test_guard_pack_basic()
{
  TEST_START("GuardPackBasic")

  auto config_data = make_shared<Config>(1, "test");
  auto cache_data = make_shared<Cache>(100, 10);

  storage<Config> config_storage(config_data);
  storage<Cache> cache_storage(cache_data);

  auto pack = make_guard_pack(config_storage, cache_storage);

  // get<I>() access
  assert(pack.get<0>()->version == 1);
  assert(pack.get<0>()->name == "test");
  assert(pack.get<1>()->hits == 100);
  assert(pack.get<1>()->misses == 10);

  // Check size()
  static_assert(decltype(pack)::size() == 2, "Size should be 2");

  TEST_END()
}

void test_guard_pack_single()
{
  TEST_START("GuardPackSingle")

  auto data = make_shared<int>(42);
  storage<int> store(data);

  auto pack = make_guard_pack(store);

  assert(*pack.get<0>() == 42);
  static_assert(decltype(pack)::size() == 1, "Size should be 1");

  TEST_END()
}

void test_guard_pack_three_types()
{
  TEST_START("GuardPackThreeTypes")

  auto config_data = make_shared<Config>(2, "prod");
  auto cache_data = make_shared<Cache>(500, 50);
  auto state_data = make_shared<State>(true, 3.14);

  storage<Config> config_storage(config_data);
  storage<Cache> cache_storage(cache_data);
  storage<State> state_storage(state_data);

  auto pack = make_guard_pack(config_storage, cache_storage, state_storage);

  assert(pack.get<0>()->version == 2);
  assert(pack.get<1>()->hits == 500);
  assert(pack.get<2>()->active == true);
  assert(abs(pack.get<2>()->value - 3.14) < 0.0001);

  static_assert(decltype(pack)::size() == 3, "Size should be 3");

  TEST_END()
}

// ============================================================================
// Structured Binding Tests
// ============================================================================

void test_structured_binding()
{
  TEST_START("StructuredBinding")

  auto config_data = make_shared<Config>(1, "binding_test");
  auto cache_data = make_shared<Cache>(200, 20);
  auto state_data = make_shared<State>(false, 2.71);

  storage<Config> config_storage(config_data);
  storage<Cache> cache_storage(cache_data);
  storage<State> state_storage(state_data);

  // Structured binding with const auto&
  const auto& [config, cache, state] = make_guard_pack(config_storage,
                                                        cache_storage,
                                                        state_storage);

  assert(config->version == 1);
  assert(config->name == "binding_test");
  assert(cache->hits == 200);
  assert(cache->misses == 20);
  assert(state->active == false);
  assert(abs(state->value - 2.71) < 0.0001);

  TEST_END()
}

void test_structured_binding_two()
{
  TEST_START("StructuredBindingTwo")

  auto int_data = make_shared<int>(100);
  auto str_data = make_shared<string>("hello");

  storage<int> int_storage(int_data);
  storage<string> str_storage(str_data);

  const auto& [num, str] = make_guard_pack(int_storage, str_storage);

  assert(*num == 100);
  assert(*str == "hello");

  TEST_END()
}

// ============================================================================
// Snapshot Isolation Tests
// ============================================================================

void test_snapshot_isolation()
{
  TEST_START("SnapshotIsolation")

  auto config_data = make_shared<Config>(1, "v1");
  auto cache_data = make_shared<Cache>(10, 1);

  storage<Config> config_storage(config_data);
  storage<Cache> cache_storage(cache_data);

  {
    auto pack = make_guard_pack(config_storage, cache_storage);

    // Update after creating pack
    config_storage.update(make_shared<Config>(2, "v2"));
    cache_storage.update(make_shared<Cache>(20, 2));

    // pack should still see previous version
    assert(pack.get<0>()->version == 1);
    assert(pack.get<0>()->name == "v1");
    assert(pack.get<1>()->hits == 10);
    assert(pack.get<1>()->misses == 1);
  }

  // After pack is destroyed, new load should see updated values
  auto new_config = config_storage.load();
  auto new_cache = cache_storage.load();

  assert(new_config->version == 2);
  assert(new_cache->hits == 20);

  TEST_END()
}

void test_snapshot_isolation_with_individual_guards()
{
  TEST_START("SnapshotIsolationWithIndividualGuards")

  auto data1 = make_shared<int>(100);
  auto data2 = make_shared<int>(200);

  storage<int> store1(data1);
  storage<int> store2(data2);

  {
    // Mix individual guard and guard_pack
    auto g1 = store1.load();

    auto pack = make_guard_pack(store1, store2);

    // Update
    store1.update(make_shared<int>(101));
    store2.update(make_shared<int>(201));

    // All should see the same snapshot
    assert(*g1 == 100);
    assert(*pack.get<0>() == 100);
    assert(*pack.get<1>() == 200);
  }

  TEST_END()
}

// ============================================================================
// Memory Tests
// ============================================================================

void test_memory_cleanup_pack()
{
  TEST_START("MemoryCleanupPack")

  weak_ptr<int> weak1, weak2, weak3;

  {
    auto data1 = make_shared<int>(1);
    auto data2 = make_shared<int>(2);
    auto data3 = make_shared<int>(3);

    weak1 = data1;
    weak2 = data2;
    weak3 = data3;

    storage<int> store1(data1);
    storage<int> store2(data2);
    storage<int> store3(data3);

    {
      auto pack = make_guard_pack(store1, store2, store3);

      assert(*pack.get<0>() == 1);
      assert(*pack.get<1>() == 2);
      assert(*pack.get<2>() == 3);

      // References kept alive inside pack
      assert(!weak1.expired());
      assert(!weak2.expired());
      assert(!weak3.expired());
    }
    // pack destroyed

  }
  // storage destroyed

  this_thread::sleep_for(chrono::milliseconds(50));

  // Up to one cached instance per storage (3 storages -> alive <= 3)
  int alive = 0;
  if (!weak1.expired()) alive++;
  if (!weak2.expired()) alive++;
  if (!weak3.expired()) alive++;

  // thread_local of main thread
  assert(alive <= 3); // main thread thread_local

  TEST_END()
}

void test_guard_pack_with_reclaimer()
{
  TEST_START("GuardPackWithReclaimer")

  auto rt = make_shared<reclaimer_thread>(true);

  thread test_thread([&]()
  {
    auto data1 = make_shared<int>(100);
    auto data2 = make_shared<string>("hello");

    storage<int> store1(data1, rt);
    storage<string> store2(data2, rt);

    {
      auto pack = make_guard_pack(store1, store2);

      assert(*pack.get<0>() == 100);
      assert(*pack.get<1>() == "hello");

      // Update
      store1.update(make_shared<int>(200));
      store2.update(make_shared<string>("world"));

      // pack keeps seeing old values
      assert(*pack.get<0>() == 100);
      assert(*pack.get<1>() == "hello");
    }

    // New pack sees updated values
    auto pack2 = make_guard_pack(store1, store2);
    assert(*pack2.get<0>() == 200);
    assert(*pack2.get<1>() == "world");

    this_thread::sleep_for(chrono::milliseconds(100));
  });

  test_thread.join();

  this_thread::sleep_for(chrono::milliseconds(100));

  TEST_END()
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_same_storage_multiple_times()
{
  TEST_START("SameStorageMultipleTimes")

  auto data = make_shared<int>(42);
  storage<int> store(data);

  // Use same storage multiple times
  auto pack = make_guard_pack(store, store, store);

  assert(*pack.get<0>() == 42);
  assert(*pack.get<1>() == 42);
  assert(*pack.get<2>() == 42);

  // All point to the same value
  assert(&(*pack.get<0>()) == &(*pack.get<1>()));
  assert(&(*pack.get<1>()) == &(*pack.get<2>()));

  TEST_END()
}

void test_nested_guard_pack()
{
  TEST_START("NestedGuardPack")

  auto data1 = make_shared<int>(1);
  auto data2 = make_shared<int>(2);

  storage<int> store1(data1);
  storage<int> store2(data2);

  {
    auto pack1 = make_guard_pack(store1, store2);

    {
      auto pack2 = make_guard_pack(store1, store2);

      store1.update(make_shared<int>(10));
      store2.update(make_shared<int>(20));

      // Both keep seeing old values
      assert(*pack1.get<0>() == 1);
      assert(*pack2.get<0>() == 1);
    }

    // pack1 still sees old value
    assert(*pack1.get<0>() == 1);
  }

  // New guard sees updated value
  auto g = store1.load();
  assert(*g == 10);

  TEST_END()
}

void test_guard_pack_ref_count()
{
  TEST_START("GuardPackRefCount")

  auto data = make_shared<int>(42);
  storage<int> store(data);

  {
    auto g1 = store.load();
    assert(g1.ref_count() == 1);

    {
      auto pack = make_guard_pack(store);
      // Guard inside pack increases ref_count
      assert(pack.get<0>().ref_count() == 2);
      assert(g1.ref_count() == 2);
    }

    // After pack is destroyed
    assert(g1.ref_count() == 1);
  }

  TEST_END()
}

// ============================================================================
// ADL get() Tests
// ============================================================================

void test_adl_get()
{
  TEST_START("ADLGet")

  auto data1 = make_shared<int>(10);
  auto data2 = make_shared<string>("test");

  storage<int> store1(data1);
  storage<string> store2(data2);

  auto pack = make_guard_pack(store1, store2);

  // Call get via ADL
  auto& g0 = get<0>(pack);
  auto& g1 = get<1>(pack);

  assert(*g0 == 10);
  assert(*g1 == "test");

  // const version
  const auto& const_pack = pack;
  const auto& cg0 = get<0>(const_pack);
  const auto& cg1 = get<1>(const_pack);

  assert(*cg0 == 10);
  assert(*cg1 == "test");

  TEST_END()
}

// ============================================================================
// Main
// ============================================================================

int main()
{
  cout << "\n=== guard_pack Basic Tests ===" << endl;
  test_guard_pack_basic();
  test_guard_pack_single();
  test_guard_pack_three_types();

  cout << "\n=== Structured Binding Tests ===" << endl;
  test_structured_binding();
  test_structured_binding_two();

  cout << "\n=== Snapshot Isolation Tests ===" << endl;
  test_snapshot_isolation();
  test_snapshot_isolation_with_individual_guards();

  cout << "\n=== Memory Tests ===" << endl;
  test_memory_cleanup_pack();
  test_guard_pack_with_reclaimer();

  cout << "\n=== Edge Cases ===" << endl;
  test_same_storage_multiple_times();
  test_nested_guard_pack();
  test_guard_pack_ref_count();

  cout << "\n=== ADL get() Tests ===" << endl;
  test_adl_get();

  cout << "\n========================================" << endl;
  cout << "All guard_pack tests passed!" << endl;
  cout << "========================================" << endl;

  return 0;
}
