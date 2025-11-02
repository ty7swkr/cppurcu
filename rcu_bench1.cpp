#include <cppurcu/cppurcu.h>

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <atomic>

using namespace std;
using namespace chrono;

vector<pair<string, string>> generate_test_ips(size_t count)
{
  vector<pair<string, string>> ips;
  ips.reserve(count);

  random_device rd;
  mt19937 gen(rd());
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

  // Set initial data (use the first in the array)
  container.update(test_data_array[0]);

  atomic<bool> stop_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};
  atomic<size_t> index{0};  // Array index

  auto start = high_resolution_clock::now();

  // Reader threads
  vector<thread> readers;
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]() {
      random_device rd;
      mt19937 gen(rd() + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }
    });
  }

  // Writer threads
  vector<thread> writers;
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]() {
      size_t index = 0;
      while (!stop_flag.load(memory_order_relaxed)) {
        // Fetch sequentially from the array
        container.update(test_data_array[index++]);
        total_writes.fetch_add(1, memory_order_relaxed);

        // Simulate intermittent updates
        this_thread::sleep_for(milliseconds(100));
      }
    });
  }

  // Wait for the specified duration
  this_thread::sleep_for(test_duration);

  // Signal termination to all threads
  stop_flag.store(true, memory_order_relaxed);

  // Wait for all threads to finish
  for (auto &t : readers)
  {
    t.join();
  }
  for (auto &t : writers)
  {
    t.join();
  }

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
  // Set initial data (use the first in the array)
  container.update(test_data_array[0]);

  atomic<bool> stop_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  auto start = high_resolution_clock::now();

  // Reader threads
  vector<thread> readers;
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]() {
      random_device rd;
      mt19937 gen(rd() + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

      while (!stop_flag.load(memory_order_relaxed))
      {
        const auto &[ip, value] = test_ips[dist(gen)];
        container.contains(ip);
        total_reads.fetch_add(1, memory_order_relaxed);
      }
    });
  }

  // Writer threads
  vector<thread> writers;
  for (size_t i = 0; i < num_writers; ++i)
  {
    writers.emplace_back([&, i]() {
      size_t index = 0;
      while (!stop_flag.load(memory_order_relaxed))
      {
        // Fetch sequentially from the array
        container.update(test_data_array[index++]);
        total_writes.fetch_add(1, memory_order_relaxed);

        // Simulate intermittent updates
        this_thread::sleep_for(milliseconds(100));
      }
    });
  }

  // Wait for the specified duration
  this_thread::sleep_for(test_duration);

  // Signal termination to all threads
  stop_flag.store(true, memory_order_relaxed);

  // Wait for all threads to finish
  for (auto &t : readers)
  {
    t.join();
  }
  for (auto &t : writers)
  {
    t.join();
  }

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

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

  CPPURCUContainer container;
  container.update(test_data_array[0]);

  atomic<bool> stop_flag{false};
  atomic<size_t> total_reads{0};
  atomic<size_t> total_writes{0};

  auto start = high_resolution_clock::now();

  vector<thread> readers;
  for (size_t i = 0; i < num_readers; ++i)
  {
    readers.emplace_back([&, i]()
    {
      random_device rd;
      mt19937 gen(rd() + i);
      uniform_int_distribution<size_t> dist(0, test_ips.size() - 1);

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
      size_t index = 0;
      while (!stop_flag.load(memory_order_relaxed))
      {
        container.update(test_data_array[index++]);
        total_writes.fetch_add(1, memory_order_relaxed);
        this_thread::sleep_for(milliseconds(100));
      }
    });
  }

  this_thread::sleep_for(test_duration);
  stop_flag.store(true, memory_order_relaxed);

  for (auto &t : readers) t.join();
  for (auto &t : writers) t.join();

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  cout << "execution duration : " << duration.count() << " ms\n";
  cout << "total read  count  : " << total_reads << "\n";
  cout << "total write count  : " << total_writes << "\n";
  cout << "read throughput    : " << (total_reads * 1000.0 / duration.count()) << " ops/sec\n";
  cout << "read per second    : " << (total_reads / test_duration.count()) << " reads/sec\n";
}

// main에 추가
//benchmark_mutex(num_readers, num_writers, test_duration, test_data_array, test_ips);
//benchmark_cppurcu(num_readers, num_writers, test_duration, test_data_array, test_ips);
//benchmark_reclaimer(num_readers, num_writers, test_duration, test_data_array, test_ips);
// ============================================================================
// main
// ============================================================================
int main(int argc, char **argv)
{
  cout << "==================================\n";

  size_t gen_size = 1000;
  if (argc >= 2)
    gen_size = atoi(argv[1]);

  // Test parameters
  size_t  num_readers = 10;  // read thread
  size_t  num_writers = 2;   // write thread
  seconds test_duration(10); // 10 seconds

  cout << "TEST SET         : " << gen_size << "\n";
  cout << "- Reader thread  : " << num_readers << "\n";
  cout << "- Writer thread  : " << num_writers << "\n";
  cout << "- test duration  : " << test_duration.count() << " sec\n";
  cout << "- Update period  : 100 ms\n";

  // Pre-generate test data
  cout << "generating test data...\n";
  auto test_ips = generate_test_ips(gen_size);

  // Convert to unordered_map
  auto test_data = make_shared<unordered_map<string, string>>();
  for (const auto &[ip, value] : test_ips)
  {
    test_data->insert({ip, value});
  }

  // Pre-create 200 copies of test_data
  vector<shared_ptr<unordered_map<string, string>>> test_data_array;
  test_data_array.reserve(200);
  for (int i = 0; i < 200; ++i)
    test_data_array.push_back(make_shared<unordered_map<string, string>>(*test_data));

  cout << "Test data generation completed (200 copies)\n";

  // std::mutex test
  benchmark_mutex    (num_readers, num_writers, test_duration, test_data_array, test_ips);
  benchmark_reclaimer(num_readers, num_writers, test_duration, test_data_array, test_ips);
  benchmark_cppurcu  (num_readers, num_writers, test_duration, test_data_array, test_ips);

  cout << "\n==================================\n";
  cout << "Test completed\n";

  return 0;
}
