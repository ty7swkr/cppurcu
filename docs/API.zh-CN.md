# API 参考

## `cppurcu::storage<T>`

RCU 保护数据存储的主要类。

### 构造函数
```cpp
storage(std::shared_ptr<const T> init_value,
        std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
```
使用初始数据创建新的 storage。

**参数：**
- `init_value`：要存储的初始数据
- `reclaimer`（可选）：用于后台销毁的 reclaimer_thread 实例。如果为 nullptr，T 对象将在读取线程中销毁。

**生命周期要求：**
- `storage<T>` 实例必须比所有调用 `load()` 的线程存活更久
- 确保适当的生命周期管理是调用者的责任
- 违反会导致未定义行为（悬空引用）

### 方法

**`guard<T> load()`**
- 线程安全
- 返回一个提供当前数据访问的 guard 对象
- 在作用域内首次 load() 时，如果有新数据，则替换为新数据

**`guard<T> load_with_tls_release()`**
- 类似于 load()，但在嵌套作用域中最外层 guard 被销毁时，安排线程本地缓存的释放。
- 当多个 guard 嵌套时（ref_count > 1），TLS 缓存仅在最后一个 guard（ref_count == 0）离开作用域时释放，确保所有嵌套读取在清理前完成。
- 当你希望确保读取操作完成后 TLS 资源被及时释放，防止过期缓存时使用。
- 要取消已安排的 TLS 释放，请在返回的 guard 对象上调用 `tls.retain()`。
- 参见：`guard::tls_t::retain()`、`guard::tls_t::schedule_release()`

**`void update(std::shared_ptr<const T> value)`**
- 发布新数据
- 即使在与 load() 函数相同的作用域内并发使用也不会产生死锁。
- cppurcu 不自行回收旧数据，而是将回收委托给 std::shared_ptr。

**`void operator=(std::shared_ptr<const T> value)`**
- 更新的便捷运算符
- 等同于 `update(value)`

## `cppurcu::guard<T>`

由 `storage<T>::load()` 返回的提供快照隔离的 RAII guard。

### 说明
- 不可复制或移动。（仅线程本地）
- guard 存在期间数据保持有效。
- 同一线程上嵌套的多个 guard 共享相同的快照。

### 方法

**`const T* operator->()`**
- 提供类似智能指针的数据访问

**`const T& operator*()`**
- 提供解引用访问数据

**`explicit operator bool()`**
- 检查 guard 是否持有有效数据

**`uint64_t ref_count()`**
- 返回嵌套 guard 的当前引用计数

### TLS 缓存控制

**`guard::tls_t tls`**
- 提供对线程本地缓存行为的控制

**`void tls.schedule_release()`**
- 安排在退出最外层作用域时释放 TLS 缓存

**`void tls.retain()`**
- 取消已安排的 TLS 缓存释放，保持缓存存活

**`bool tls.release_scheduled()`**
- 如果 TLS 释放当前已安排，则返回 true

### 示例
```cpp
{
  auto guard = storage.load();
  if (guard) {
    guard->method();  // 通过 operator-> 安全访问
  }
  // Guard 在此销毁，但 TLS 缓存数据保留。
  // 数据可能在下次 load() 时更新
} 

// TLS 释放控制
{
  auto guard1 = storage.load();
  {
    auto guard2 = storage.load_with_tls_release();
    if (guard2)
      guard2->method();

    // TLS 缓存数据不会在此立即销毁，
    // 而是安排在退出最外层作用域时销毁。
  }

  // `load_with_tls_release()` 安排的 TLS 缓存释放
  // 可以通过 `guard<T>::tls.retain()` 取消。
  guard1.tls.retain();
} // 最外层
```

## `cppurcu::guard_pack<Ts...>`

将多个 guard 作为单个对象管理的 RAII 辅助工具

### 说明
- 不可复制或移动。
- 不得比其引用的 storage 存活更久。

### 方法

**`template<std::size_t I> auto& get()`**
- 通过编译时索引 I 访问 guard

**`static constexpr std::size_t size()`**
- 返回 pack 中 guard 的数量

### 示例
```cpp
auto pack = cppurcu::load(storageA, storageB, storageC);
// 或
auto pack = cppurcu::make_guard_pack(storageA.load(), storageB.load(), storageC.load());

pack.get<0>()->method_a();
pack.get<1>()->method_b();
pack.get<2>()->method_c();

// 结构化绑定
const auto& [a, b, c] = cppurcu::load(storageA, storageB, storageC);
a->method_a();
b->method_b();
c->method_c();
```

## `cppurcu::load`

从多个 storage 创建 guard_pack 的工厂函数。

### 签名
```cpp
template<typename... Ts>
guard_pack<Ts...> load(storage<Ts>&... storages);
```

### 参数
- `storages`：要加载的 storage 实例的引用

### 返回值
- 包含所有 storage 的 guard 的 `guard_pack`

## `cppurcu::make_guard_pack`

从多个 guard 创建 guard_pack 的工厂函数。

> **已弃用**：`make_guard_pack(storage<Ts>&...)` 已弃用。请使用 `cppurcu::load(storage<Ts>&...)` 替代。

### 签名
```cpp
template<typename... Ts>
guard_pack<Ts...> make_guard_pack(guard<Ts>&&... guards);
```

### 参数
- `guards`：要移入 pack 的 `guard<T>` 实例

### 返回值
- 包含所有 guard 的 `guard_pack`

## `cppurcu::reclaimer_thread`

用于处理对象销毁的后台线程。

### 构造函数
```cpp
  reclaimer_thread(bool wait_until_execution = true,
                   std::chrono::microseconds reclaim_interval =
                   std::chrono::microseconds{10000})

  reclaimer_thread(std::chrono::microseconds reclaim_interval,
                   bool wait_until_execution = true)
```
*定期扫描回收队列，在 shared_ptr 变为 unique 时移除并触发其销毁。*<br>
*仍被其他地方引用的对象无法回收，将保留在队列中。*

**参数：**
- `wait_until_execution`（可选）：如果为 true，构造函数等待 reclaimer_thread 启动。如果为 false，立即返回。
- `reclaim_interval`（可选，默认：10000μs = 10ms）：定期扫描回收队列的间隔。
  - 如果间隔 > 0μs：除通知外，按指定间隔定期扫描。
  - 如果间隔为 0μs：仅通知模式。仅在调用 push() 时扫描。
    - 如果更新不频繁，可能会延迟回收。


### 方法

**`template<typename T> void push(std::shared_ptr<T> &&ptr)`**
- 将对象排入后台销毁队列
- 通常由 storage::load() 在数据更新时内部调用

**`std::thread::id thread_id() const`**
- reclaimer_thread 的 ID
