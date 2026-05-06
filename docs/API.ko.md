# API 레퍼런스

## `cppurcu::storage<T>`

RCU 보호 데이터 스토리지를 위한 주요 클래스.

### 생성자

```cpp
storage(std::shared_ptr<const T> init_value,
        std::shared_ptr<reclaimer_thread> reclaimer = nullptr)
```

초기 데이터로 새 스토리지를 생성합니다.

**매개변수:**

- `init_value`: 저장할 초기 데이터
- `reclaimer` (선택 사항): 백그라운드 소멸을 위한 reclaimer_thread 인스턴스. nullptr이면 T 객체는 리더 스레드에서 소멸됩니다.

**수명 요구 사항:**

- `storage<T>` 인스턴스는 `load()`를 호출하는 모든 스레드보다 오래 살아있어야 합니다
- 적절한 수명 관리는 호출자의 책임입니다
- 위반 시 정의되지 않은 동작(댕글링 참조)이 발생합니다

### 메서드

**`guard<T> load()`**

- 스레드 안전
- 현재 데이터에 접근할 수 있는 guard 객체를 반환합니다
- 스코프 내 첫 번째 load() 시, 새 데이터가 있으면 새 데이터로 교체합니다

**`guard<T> load_with_tls_release()`**

- load()와 유사하지만, 중첩된 스코프에서 가장 바깥쪽 가드가 소멸될 때 스레드 로컬 캐시를 해제하도록 예약합니다.
- 여러 가드가 중첩된 경우(ref_count > 1), TLS 캐시는 마지막 남은 가드(ref_count == 0)가 스코프를 벗어날 때만 해제되어, 모든 중첩된 읽기가 완료된 후 정리됩니다.
- 읽기 작업이 완료된 후 TLS 리소스가 즉시 해제되도록 하여 오래된 캐시를 방지하고 싶을 때 사용하세요.
- 예약된 TLS 해제를 취소하려면 반환된 guard 객체에서 `tls.retain()`을 호출하세요.
- 참고: `guard::tls_t::retain()`, `guard::tls_t::schedule_release()`

**`void update(std::shared_ptr<const T> value)`**

- 새 데이터를 게시합니다
- load() 함수와 동일 스코프 내에서 동시에 사용해도 데드락이 발생하지 않습니다.
- cppurcu는 이전 데이터를 직접 회수하지 않고 std::shared_ptr에 회수를 위임합니다.

**`void operator=(std::shared_ptr<const T> value)`**

- 업데이트를 위한 편의 연산자
- `update(value)`와 동일

## `cppurcu::guard<T>`

`storage<T>::load()`가 반환하는, 스냅샷 격리를 제공하는 RAII 가드.

### 참고

- 복사 및 이동 불가. (스레드 로컬 전용)
- 가드가 존재하는 동안 데이터는 유효합니다.
- 동일 스레드에서 중첩된 여러 가드는 동일한 스냅샷을 공유합니다.

### 메서드

**`const T* operator->()`**

- 데이터에 대한 스마트 포인터 스타일의 접근을 제공합니다

**`const T& operator*()`**

- 데이터에 대한 역참조 접근을 제공합니다

**`explicit operator bool()`**

- 가드가 유효한 데이터를 보유하고 있는지 확인합니다

**`uint64_t ref_count()`**

- 중첩된 가드의 현재 참조 카운트를 반환합니다

### TLS 캐시 제어

**`guard::tls_t tls`**

- 스레드 로컬 캐시 동작을 제어합니다

**`void tls.schedule_release()`**

- 가장 바깥쪽 스코프를 벗어날 때 TLS 캐시를 해제하도록 예약합니다

**`void tls.retain()`**

- 예약된 TLS 캐시 해제를 취소하여 캐시를 유지합니다

**`bool tls.release_scheduled()`**

- TLS 해제가 현재 예약되어 있으면 true를 반환합니다

### 예시

```cpp
{
  auto guard = storage.load();
  if (guard) {
    guard->method();  // operator->를 통한 안전한 접근
  }
  // 가드가 여기서 소멸되지만, TLS 캐시 데이터는 유지됩니다.
  // 데이터는 다음 load()에서 업데이트될 수 있습니다
}

// TLS 해제 제어 예시
{
  auto guard1 = storage.load();
  {
    // 가장 바깥쪽 스코프를 벗어날 때 thread-local storage에서 해제되도록 예약됩니다.
    // (비록 guard2에 의해 load_with_tls_release()가 호출되었지만
    //  가장 바깥쪽인 guard1의 스코프)
    auto guard2 = storage.load_with_tls_release();
    if (guard2)
      guard2->method();
  }

  // `load_with_tls_release()`에 의해 TLS 캐시 해제가 예약되었지만
  // `guard<T>::tls.retain()`을 통해 취소할 수 있습니다.
  guard1.tls.retain();
} // 가장 바깥쪽
```

## `cppurcu::guard_pack<Ts...>`

여러 가드를 단일 객체로 관리하는 RAII 헬퍼

### 참고

- 복사 및 이동 불가.
- 참조하는 스토리지보다 오래 살아있으면 안 됩니다.

### 메서드

**`template<std::size_t I> auto& get()`**

- I 번째 가드를 가져옵니다. I 는 템플릿 매개변수이며 컴파일 시간에 결정됩니다.

**`static constexpr std::size_t size()`**

- 팩 내 가드의 수를 반환

### 예시

```cpp
auto pack = cppurcu::load(storageA, storageB, storageC);
// 또는
auto pack = cppurcu::make_guard_pack(storageA.load(), storageB.load(), storageC.load());

pack.get<0>()->method_a();
pack.get<1>()->method_b();
pack.get<2>()->method_c();

// 구조화된 바인딩
const auto& [a, b, c] = cppurcu::load(storageA, storageB, storageC);
a->method_a();
b->method_b();
c->method_c();
```

## `cppurcu::load`

여러 스토리지에서 guard_pack을 생성하는 팩토리 함수.

### 시그니처

```cpp
template<typename... Ts>
guard_pack<Ts...> load(storage<Ts>&... storages);
```

### 매개변수

- `storages`: 로드할 스토리지 인스턴스들의 참조

### 반환값

- 모든 스토리지에 대한 가드를 포함하는 `guard_pack`

## `cppurcu::make_guard_pack`

여러 가드에서 guard_pack을 생성하는 팩토리 함수.

> **사용 중단(Deprecated)**: `make_guard_pack(storage<Ts>&...)`는 사용 중단되었습니다. 대신 `cppurcu::load(storage<Ts>&...)`를 사용하세요.

### 시그니처

```cpp
template<typename... Ts>
guard_pack<Ts...> make_guard_pack(guard<Ts>&&... guards);
```

### 매개변수

- `guards`: 팩에 이동시킬 `guard<T>` 인스턴스들

### 반환값

- 모든 가드를 포함하는 `guard_pack`

## `cppurcu::reclaimer_thread`

객체 소멸을 처리하는 백그라운드 스레드.

### 생성자

```cpp
  reclaimer_thread(bool wait_until_execution = true,
                   std::chrono::microseconds reclaim_interval =
                   std::chrono::microseconds{10000})

  reclaimer_thread(std::chrono::microseconds reclaim_interval,
                   bool wait_until_execution = true)
```

*주기적으로 리클레임 큐를 스캔하여 unique 상태가 된 shared_ptr을 제거하고 소멸을 트리거합니다.*`<br>`
*아직 다른 곳에서 참조 중인 객체는 회수할 수 없으며 큐에 남습니다.*

**매개변수:**

- `wait_until_execution` (선택 사항): true이면 생성자가 reclaimer_thread가 시작될 때까지 대기합니다. false이면 즉시 반환합니다.
- `reclaim_interval` (선택 사항, 기본값: 10000μs = 10ms): 리클레임 큐의 주기적 스캔 간격.
  - 간격 > 0μs: 알림 외에도 지정된 간격으로 주기적 스캔을 수행합니다.
  - 간격이 0μs: 알림 전용 모드. push()가 호출될 때만 스캔합니다.
    - 업데이트가 드문 경우 회수가 지연될 수 있습니다.

### 메서드

**`template<typename T> void push(std::shared_ptr<T> &&ptr)`**

- 백그라운드 소멸을 위해 객체를 큐에 추가합니다
- 보통 데이터가 업데이트될 때 storage::update() / source::update()에서 내부적으로 호출됩니다

**`std::thread::id thread_id() const`**

- reclaimer_thread의 ID
