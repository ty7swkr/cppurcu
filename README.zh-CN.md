# cppurcu <sub><sup>[🇰🇷 한국어](README.ko.md) | [English](README.md)</sup></sub>

一个简单的 C++ RCU（read-copy-update）用户空间库实现，基于 RAII 的快照隔离，仅头文件且无外部依赖，仅使用 C++17 标准库。
<br>
<br>

## 主要特性

- **仅头文件**：仅依赖标准库，无外部依赖
- **无锁读取**：缓存预热后，读取路径上的竞争最小化
- **快照隔离**：调用线程中基于 RAII guard 模式的快照隔离<br>
（guard_pack 可在一行中加载多个 storage）
- **无数据复制**：不对每个线程进行深拷贝
- **可选的后台销毁**：reclaimer_thread 可将对象销毁卸载到单独的线程，减轻读取线程的负担

## 性能
基准测试结果请参见 [PERFORMANCE.zh-CN.md](docs/PERFORMANCE.zh-CN.md)。
<br>
<br>

## 快速开始
### API 概览
无需读取器注册、宽限期管理或内存屏障。<br>
```cpp
storage = new_storage;      // 更新示例（std::shared_ptr<T> new_storage）
auto data = storage.load(); // 返回 guard 对象
```
*注1：与传统 RCU 不同，cppurcu 不直接回收旧数据，
而是将回收委托给 `std::shared_ptr`。
旧对象的 `std::shared_ptr` 引用在作用域内首次调用 `load()` 时释放。
因此，当所有 `std::shared_ptr` 实例的所有引用都释放时，内存才会被回收。*<br>

*注2：因此，无论在何处使用，update 调用都不会产生死锁。*

*注3：尽管如此，cppurcu 并不是一个简单的 std::shared_ptr 包装器——它通过 RCU 语义提供无锁的读取端访问，不同于 std::shared_ptr 在每次读取时都需要原子引用计数操作。*

### 基本用法
```cpp
#include <cppurcu/cppurcu.h>
#include <memory>
#include <string>
#include <map>

// 使用初始数据创建 storage
auto storage = cppurcu::create(std::make_shared<std::map<std::string, std::string>>());

// 读取（无锁）- 返回 guard 对象
auto data = storage.load(); // cppurcu::guard<T>
if (data->count("key") > 0) {
  // 使用数据
}

// 更新（立即生效，回收委托给 std::shared_ptr）
auto new_data = std::make_shared<std::map<std::string, std::string>>();
(*new_data)["key"] = "value";
storage = new_data; // 或 storage.update(new_data);
```
**⚠️ 重要：**
- `storage<T>` 实例必须比所有使用它的线程存活更久
- 在线程仍在访问时销毁 `storage` 会导致未定义行为
- 通常将 `storage` 声明为全局、静态或长生命周期的成员变量

### 快照隔离
即使在同一线程的特定作用域内，通过复杂的调用链发生多次 `storage::load()` 调用，或者其他线程发生数据更新，该线程内的所有读取操作都将强制看到相同的数据版本。

当所有 guard 被销毁后，下一次 load() 将获取更新后的版本
```cpp
{
  auto data = storage.load();    // 快照版本 1
  {
    storage.update(new_data2);   // 更新到版本 2
    auto data1 = storage.load(); // 仍然是快照版本 1
  } // data1 Guard 销毁
} // data Guard 销毁

storage.update(new_data3);       // 更新到版本 3
{
  auto data = storage.load();    // 加载版本 3
}
```
### 多 Storage 快照

当需要在一行中加载（快照）多个 storage 时，可以使用 `cppurcu::load(storage<Ts>&...)`:
```cpp
#include <cppurcu/cppurcu.h>

// 多个数据 storage
auto storageA = cppurcu::create(...);
auto storageB = cppurcu::create(...);
auto storageC = cppurcu::create(...);

// 加载所有 - 保持相同的快照点
const auto &[a, b, c] = cppurcu::load(storageA, storageB, storageC);

// 此作用域内的所有读取都看到一致的数据，
// 即使其他线程发生了更新。
a->lookup(...);
b->query(...);
c->find(...);
```
如果需要在特定作用域内保持一致的快照，可以这样编写代码：
```cpp
#include <cppurcu/cppurcu.h>
...........
{
  auto pack = cppurcu::load(storageA, storageB, storageC);
  // 或使用 storage<T>::load() 返回的 `guard<T>` 对象
  auto pack = cppurcu::make_guard_pack(storageA.load(), storageB.load(), storageC.load());
  .....
  // 即使 storageA 和 storageC 在 calculate(...) 函数的
  // 调用链中的某处被使用，
  // pack 仍然保持 storageA、B 和 C 的一致快照。
  my_class.calculate(...);
  .....
}
```

**注意：**<br>
- 每个 storage 仍然独立版本管理；`guard_pack` 是一个用于便捷多 storage 快照加载的 RAII 辅助工具，而非跨 storage 的事务机制。

### 后台销毁（可选）

对于析构函数开销较大的对象，可以使用 reclaimer_thread 在后台处理销毁：
```cpp
#include <cppurcu/cppurcu.h>

// 创建 reclaimer_thread
auto reclaimer = std::make_shared<cppurcu::reclaimer_thread>();

// 使用 reclaimer_thread 创建 storage
auto storage1 = cppurcu::create(std::make_shared<std::set<std::string>>(), reclaimer);

// reclaimer_thread 可用于任意模板类型。
auto storage2 = cppurcu::create(std::make_shared<std::vector<int>>(), reclaimer);

// 在源代码的某处...（更新）
storage1 = new_data;

// 调用 load，获取更新后的对象。
// 旧对象在后台线程（reclaimer_thread）中销毁
auto data = storage1.load();

```
**注意（行为变更）**<br>
- 之前，系统在使用新数据更新时，使用 `reclaimer_thread`（及其互斥锁）来处理旧数据。
- 目前，读取器不再使用 `reclaimer_thread`（或其互斥锁）；`reclaimer_thread` 仅由 `storage<T>::update()` 使用。
<br>

## API 参考

详细的 API 文档请参见 [API.zh-CN.md](docs/API.zh-CN.md)。

快速参考：
- `cppurcu::storage<T>` - 主要的 RCU 保护数据存储
- `cppurcu::guard<T>` - 用于快照隔离的 RAII guard
- `cppurcu::guard_pack<Ts...>` - 多 storage 快照辅助工具
- `cppurcu::reclaimer_thread` - 后台销毁处理器
<br>

## 安装

cppurcu 是一个仅头文件的库。将 cppurcu/ 目录复制到你的 include 路径：

```bash
# 将头文件复制到你的项目
cp -r cppurcu/ /path/to/your/project/include/

# 或添加到 include 路径
g++ -I./path/to/cppurcu your_code.cpp
```

### 要求

- C++17 或更高版本
<br>

## 构建测试

构建并运行包含的测试：

```bash
# 使用 cppurcu 和 mutex 构建
make

# 构建包含 liburcu 对比（需要 liburcu-dev）
make liburcu

# 运行测试
./rcu_bench 1000000  # 使用 1M 项目测试，需要 20GB 内存
```

### Makefile 选项

- `make` - 构建 cppurcu 和 mutex 测试（默认）
- `make liburcu` - 构建包含 liburcu 对比的测试
- `make clean` - 清除构建产物
<br>

## 工作原理

主要类：

1. **`storage<T>`**：集成 source、local 和 guard 的用户接口 API
2. **`guard<T>`**：storage<T>::load() 的返回值，用于快照隔离的 RAII guard
3. **`source<T>`**：维护权威数据和版本计数器
4. **`local<T>`**：线程本地缓存（仅浅拷贝）
5. **`reclaimer_thread（可选）`**：用于处理对象销毁的后台线程

本设计不使用：
- ABA 问题解决方案（无标记指针）
- 风险指针（Hazard pointers）
- 基于纪元的回收器（Epoch-based reclaimer）

### 读取路径

创建 guard 时（每次 `load()` 调用）：
1. 检查缓存版本与源版本（如果 guard<T>.ref_count > 0 则跳过，以实现快照隔离）
2. 如果未更改：返回缓存的原始指针（快速路径）
3. 如果已更改：更新缓存中的版本、shared_ptr 和原始指针（慢速路径）
<br>如果启用了 reclaimer_thread：将旧值推入回收队列

### 回收线程（可选）

启用后，reclaimer_thread 在后台处理对象销毁：
1. storage::load() 在数据更新时将旧的 shared_ptr 推入回收队列
2. 工作线程定期扫描，在变为 unique() 时移除条目
3. 非唯一对象保留在队列中，直到可以回收
4. 对象销毁不会阻塞读取器，减少昂贵析构函数的开销

### 线程安全保证

- 无锁读取
- 线程安全的更新
- **要求**：`storage<T>` 的生命周期 > 线程生命周期
<br>

## 测试

### 单元测试

`test/` 目录包含全面的单元测试，涵盖正确性、线程安全性和内存管理：

| 目标 | 描述 | Sanitizer |
|------|------|-----------|
| `unit_test` | 核心功能：基本操作、guard、快照隔离、定时释放、reclaimer 线程 | 无 |
| `unit_test_guard_pack` | `guard_pack` 和结构化绑定测试 | ASan + LSan + UBSan |
| `unit_test_tsan` | 压力测试：线程爆炸、快速更新、大对象、嵌套 guard | ThreadSanitizer |
| `unit_test_lausan` | 内存泄漏检测、nullptr 处理、异常安全性、定时释放内存行为 | ASan + LSan + UBSan |

> **注意：** 单元测试需要 Clang 以支持 sanitizer（ThreadSanitizer、AddressSanitizer、LeakSanitizer）。

```bash
cd test

# 构建所有单元测试
make

# 构建单个目标
make test        # 仅 unit_test
make guard_pack  # 仅 guard_pack 测试
make tsan        # 仅 ThreadSanitizer 测试
make lsan        # 仅 Leak/Address sanitizer 测试

# 运行
./unit_test
./unit_test_guard_pack
./unit_test_tsan
./unit_test_lausan
```

### 基准测试

基准测试比较三种方法：

1. **std::mutex** - 传统的基于锁的保护
2. **cppurcu** - 本库
3. **liburcu** - 广泛使用的 RCU 库（可选）

使用不同数据大小运行基准测试：
```bash
./rcu_bench 1000      # 1K 项目
./rcu_bench 100000    # 100K 项目
./rcu_bench 1000000   # 1M 项目，需要 20GB 内存
```
