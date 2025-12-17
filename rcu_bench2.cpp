#include <cppurcu/cppurcu.h>
#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <atomic>

#include <urcu.h>
#include <urcu/rculist.h>
#include <urcu-call-rcu.h>

using namespace std;
using namespace chrono;

vector<pair<string, string>> generate_test_ips(size_t count)
{
  vector<pair<string, string>> ips;
  ips.reserve(count);

  mt19937 gen(12345);
  uniform_int_distribution<> dist(0, 255);

  for (size_t i = 0; i < count; ++i)
  {
    string ip = to_string(dist(gen)) + "." +
        to_string(dist(gen)) + "." +
        to_string(dist(gen)) + "." +
        to_string(dist(gen));
    ips.push_back({ip, "test-data"});
  }

  return ips;
}

class MutexContainer
{
public:
  MutexContainer()
  {
    ips_ = make_shared<unordered_map<string, string>>();
  }

  bool contains(const string &ip)
  {
    lock_guard<mutex> lock(mutex_);
    return ips_->count(ip) > 0;
  }

  void update(shared_ptr<unordered_map<string, string>> new_ips)
  {
    lock_guard<mutex> lock(mutex_);
    ips_ = new_ips;
  }

private:
  shared_ptr<unordered_map<string, string>> ips_;
  mutex mutex_;
};

void benchmark_mutex(
    size_t num_readers,
    size_t num_writers,
    seconds test_duration,
    const vector<shared_ptr<unordered_map<string, string>>> &test_data_array,
    const vector<pair<string, string>> &test_ips)
{
  cout << "\n========================================\n";
  cout << "std::mutex\n";
  cout << "========================================\n";
  cout << "Reader thread  : " << num_readers << "\n";
  cout << "Writer thread  : " << num_writers << "\n";
  cout << "test duration  : " << test_duration.count() << " sec\n";

  MutexContainer container;
  container.update(test_data_array[0]);

  atomic<bool> stop_flag{false};
  atomic<bool> start_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  vector<thread> readers;
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]()
    {
      mt19937 gen(12345 + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      for (int w = 0; w < 1000; ++w)
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
      }

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }
    });
  }

  vector<thread> writers;
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]()
    {
      size_t index = i;

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        container.update(test_data_array[index]);
        index += num_writers;
        total_writes.fetch_add(1, memory_order_relaxed);
        this_thread::sleep_for(milliseconds(100));
      }
    });
  }

  this_thread::sleep_for(milliseconds(100));

  start_flag.store(true, memory_order_release);
  auto start = high_resolution_clock::now();

  this_thread::sleep_for(test_duration);

  stop_flag.store(true, memory_order_relaxed);

  for (auto &t : readers)
    t.join();
  for (auto &t : writers)
    t.join();

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

class CPPURCUContainer
{
public:
  CPPURCUContainer()
  : ips_(std::make_shared<unordered_map<string, string>>())
  {
  }

  bool contains(const string &ip)
  {
    auto ips = ips_.load();
    return ips->count(ip) > 0;
  }

  void update(shared_ptr<unordered_map<string, string>> new_ips)
  {
    ips_ = new_ips;
  }

private:
  cppurcu::storage<unordered_map<string, string>> ips_;
};

void benchmark_cppurcu(
    size_t num_readers,
    size_t num_writers,
    seconds test_duration,
    const vector<shared_ptr<unordered_map<string, string>>> &test_data_array,
    const vector<pair<string, string>> &test_ips)
{
  cout << "\n========================================\n";
  cout << "cppurcu\n";
  cout << "========================================\n";
  cout << "Reader thread  : " << num_readers << "\n";
  cout << "Writer thread  : " << num_writers << "\n";
  cout << "test duration  : " << test_duration.count() << " sec\n";

  CPPURCUContainer container;
  container.update(test_data_array[0]);

  atomic<bool> stop_flag{false};
  atomic<bool> start_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  vector<thread> readers;
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]()
    {
      mt19937 gen(12345 + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      for (int w = 0; w < 1000; ++w)
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
      }

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }
    });
  }

  vector<thread> writers;
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]()
    {
      size_t index = i;

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        container.update(test_data_array[index]);
        index += num_writers;
        total_writes.fetch_add(1, memory_order_relaxed);
        this_thread::sleep_for(milliseconds(100));
      }
    });
  }

  this_thread::sleep_for(milliseconds(100));
  start_flag.store(true, memory_order_release);
  auto start = high_resolution_clock::now();

  this_thread::sleep_for(test_duration);
  stop_flag.store(true, memory_order_relaxed);

  for (auto &t : readers)
    t.join();
  for (auto &t : writers)
    t.join();

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

// ============================================================================
// liburcu with synchronize_rcu (sync)
// ============================================================================

class LiburcuContainerSync
{
public:
  LiburcuContainerSync()
  {
    rcu_assign_pointer(ptr_, &data_);
  }

  ~LiburcuContainerSync()
  {
    synchronize_rcu();
  }

  bool contains(const string &ip)
  {
    rcu_read_lock();
    auto *m = rcu_dereference(ptr_);
    bool ok = (m->count(ip) > 0);
    rcu_read_unlock();
    return ok;
  }

  void update(unordered_map<string, string> *new_ips_sp)
  {
    rcu_xchg_pointer(&ptr_, new_ips_sp);
    synchronize_rcu();
  }

private:
  unordered_map<string, string> data_;
  unordered_map<string, string> *ptr_{nullptr};
};

void benchmark_liburcu_sync(
    size_t num_readers,
    size_t num_writers,
    chrono::seconds test_duration,
    const vector<shared_ptr<unordered_map<string, string>>> &test_data_array,
    const vector<pair<string, string>> &test_ips)
{
  cout << "\n========================================\n";
  cout << "liburcu (synchronize_rcu - sync)\n";
  cout << "========================================\n";
  cout << "Reader thread  : " << num_readers << "\n";
  cout << "Writer thread  : " << num_writers << "\n";
  cout << "test duration  : " << test_duration.count() << " sec\n";

  rcu_init();

  LiburcuContainerSync container;
  container.update(test_data_array[0].get());

  atomic<bool> stop_flag{false};
  atomic<bool> start_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  vector<thread> readers;
  readers.reserve(num_readers);
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]()
    {
      rcu_register_thread();

      mt19937 gen(12345 + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      for (int w = 0; w < 1000; ++w)
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
      }

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }

      rcu_unregister_thread();
    });
  }

  vector<thread> writers;
  writers.reserve(num_writers);
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]()
    {
      size_t index = i;

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        container.update(test_data_array[index].get());
        index += num_writers;
        total_writes.fetch_add(1, memory_order_relaxed);
        this_thread::sleep_for(chrono::milliseconds(100));
      }
    });
  }

  this_thread::sleep_for(chrono::milliseconds(100));
  start_flag.store(true, memory_order_release);
  auto start = chrono::high_resolution_clock::now();

  this_thread::sleep_for(test_duration);
  stop_flag.store(true, memory_order_relaxed);

  for (auto &t : readers)
    t.join();
  for (auto &t : writers)
    t.join();

  auto end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

// ============================================================================
// liburcu with call_rcu (async)
// ============================================================================

struct RcuMapWrapper
{
  struct rcu_head rcu;
  unordered_map<string, string> *map;
};

static void free_map_callback(struct rcu_head *head)
{
  caa_container_of(head, RcuMapWrapper, rcu);
}

class LiburcuContainerAsync
{
public:
  LiburcuContainerAsync()
  {
    rcu_assign_pointer(ptr_, &data_);
  }

  ~LiburcuContainerAsync()
  {
    rcu_barrier();
  }

  bool contains(const string &ip)
  {
    rcu_read_lock();
    auto *m = rcu_dereference(ptr_);
    bool ok = (m->count(ip) > 0);
    rcu_read_unlock();
    return ok;
  }

  void update(unordered_map<string, string> *new_ips_sp, RcuMapWrapper *wrapper)
  {
    auto *old = rcu_xchg_pointer(&ptr_, new_ips_sp);

    if (old != nullptr)
    {
      wrapper->map = old;
      call_rcu(&wrapper->rcu, free_map_callback);
    }
  }

private:
  unordered_map<string, string> data_;
  unordered_map<string, string> *ptr_{nullptr};
};

void benchmark_liburcu_async(
    size_t num_readers,
    size_t num_writers,
    chrono::seconds test_duration,
    const vector<shared_ptr<unordered_map<string, string>>> &test_data_array,
    const vector<pair<string, string>> &test_ips)
{
  cout << "\n========================================\n";
  cout << "liburcu (call_rcu - async)\n";
  cout << "========================================\n";
  cout << "Reader thread  : " << num_readers << "\n";
  cout << "Writer thread  : " << num_writers << "\n";
  cout << "test duration  : " << test_duration.count() << " sec\n";

  rcu_init();

  RcuMapWrapper wrapper;
  std::vector<RcuMapWrapper> wrappers(300);

  LiburcuContainerAsync container;
  container.update(test_data_array[0].get(), &wrapper);

  atomic<bool> stop_flag{false};
  atomic<bool> start_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  vector<thread> readers;
  readers.reserve(num_readers);
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]()
    {
      rcu_register_thread();

      mt19937 gen(12345 + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      for (int w = 0; w < 1000; ++w)
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
      }

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }

      rcu_unregister_thread();
    });
  }

  vector<thread> writers;
  writers.reserve(num_writers);
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]()
    {
      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      size_t index = i;
      while (!stop_flag.load(memory_order_relaxed))
      {
        container.update(test_data_array[index].get(), &wrappers[index]);
        index += num_writers;
        total_writes.fetch_add(1, memory_order_relaxed);
        this_thread::sleep_for(chrono::milliseconds(100));
      }
    });
  }

  this_thread::sleep_for(chrono::milliseconds(100));
  start_flag.store(true, memory_order_release);
  auto start = chrono::high_resolution_clock::now();

  this_thread::sleep_for(test_duration);
  stop_flag.store(true, memory_order_relaxed);

  for (auto &t : readers)
    t.join();
  for (auto &t : writers)
    t.join();

  auto end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

class CPPURCURetirementContainer
{
public:
  CPPURCURetirementContainer()
  : ips_(std::make_shared<unordered_map<string, string>>(),
         std::make_shared<cppurcu::reclaimer_thread>())
  {
  }

  bool contains(const string &ip)
  {
    auto ips = ips_.load();
    return ips->count(ip) > 0;
  }

  void update(shared_ptr<unordered_map<string, string>> new_ips)
  {
    ips_ = new_ips;
  }

private:
  cppurcu::storage<unordered_map<string, string>> ips_;
};

void benchmark_reclaimer(
    size_t num_readers,
    size_t num_writers,
    seconds test_duration,
    const vector<shared_ptr<unordered_map<string, string>>> &test_data_array,
    const vector<pair<string, string>> &test_ips)
{
  cout << "\n========================================\n";
  cout << "cppurcu + reclaimer_thread\n";
  cout << "========================================\n";
  cout << "Reader thread  : " << num_readers << "\n";
  cout << "Writer thread  : " << num_writers << "\n";
  cout << "test duration  : " << test_duration.count() << " sec\n";

  CPPURCURetirementContainer container;
  container.update(test_data_array[0]);

  atomic<bool> stop_flag{false};
  atomic<bool> start_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  vector<thread> readers;
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]()
    {
      mt19937 gen(12345 + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      for (int w = 0; w < 1000; ++w)
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
      }

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }
    });
  }

  vector<thread> writers;
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]()
    {
      size_t index = i;

      while (!start_flag.load(memory_order_acquire))
        this_thread::yield();

      while (!stop_flag.load(memory_order_relaxed))
      {
        container.update(test_data_array[index]);
        index += num_writers;
        total_writes.fetch_add(1, memory_order_relaxed);
        this_thread::sleep_for(milliseconds(100));
      }
    });
  }

  this_thread::sleep_for(milliseconds(100));
  start_flag.store(true, memory_order_release);
  auto start = high_resolution_clock::now();

  this_thread::sleep_for(test_duration);
  stop_flag.store(true, memory_order_relaxed);

  for (auto &t : readers)
    t.join();
  for (auto &t : writers)
    t.join();

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

void flush_cache()
{
  const size_t cache_size = 128 * 1024 * 1024;
  volatile char *dummy = new char[cache_size];

  for (size_t i = 0; i < cache_size; i += 64)
    dummy[i] = 1;

  delete[] dummy;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char **argv)
{
  size_t gen_size = 1000;
  int num_runs = 1;

  if (argc >= 2)
    gen_size = atoi(argv[1]);

  size_t num_readers = 10;
  size_t num_writers = 2;
  seconds test_duration(10);

  cout << "==================================\n";
  cout << "TEST SET         : " << gen_size << "\n";
  cout << "- Reader thread  : " << num_readers << "\n";
  cout << "- Writer thread  : " << num_writers << "\n";
  cout << "- test duration  : " << test_duration.count() << " sec\n";
  cout << "- Update period  : 100 ms\n";

  cout << "generating test data...\n";
  auto test_ips = generate_test_ips(gen_size);

  auto test_data = make_shared<unordered_map<string, string>>();
  for (const auto &[ip, value] : test_ips)
  {
    test_data->insert({ip, value});
  }

  vector<shared_ptr<unordered_map<string, string>>> test_data_array;
  test_data_array.reserve(220);
  for (int i = 0; i < 220; ++i)
    test_data_array.push_back(make_shared<unordered_map<string, string>>(*test_data));

  cout << "Test data generation completed (200 copies)\n";

  for (int run = 0; run < num_runs; ++run)
  {
    if (num_runs > 1)
      cout << "\n********** Run " << (run + 1) << " / " << num_runs << " **********\n";

    flush_cache();
    benchmark_mutex(num_readers, num_writers, test_duration, test_data_array, test_ips);

    flush_cache();
    benchmark_reclaimer(num_readers, num_writers, test_duration, test_data_array, test_ips);

    flush_cache();
    benchmark_cppurcu(num_readers, num_writers, test_duration, test_data_array, test_ips);

    flush_cache();
    benchmark_liburcu_sync(num_readers, num_writers, test_duration, test_data_array, test_ips);

    flush_cache();
    benchmark_liburcu_async(num_readers, num_writers, test_duration, test_data_array, test_ips);
  }

  cout << "\n==================================\n";
  cout << "Test completed\n";

  return 0;
}
