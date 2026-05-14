# cppurcu <sub><sup>[English](README.md) | [简体中文](README.zh-CN.md)</sup></sub>

RAII 기반 스냅샷 격리를 제공하는 간단한 C++ RCU(read-copy-update) 유저스페이스 라이브러리 구현체로, 헤더 온리이며 외부 의존성 없이 C++17 표준 라이브러리만 사용합니다.
<br>

## **RCU가 뭐에요?**

RCU(Read-Copy-Update)는 `std::mutex`, `std::shared_mutex`, 또는 스핀락이 확장성 병목이 되는 읽기 위주(read-heavy) 워크로드를 위해 설계된 동기화 기법으로 라이터가 새로운 데이터를 발행하는 동안 리더가 락프리로 읽을 수 있게 합니다.

읽기 위주 워크로드에서 RCU 알고리즘 사용은 일반적으로 락 기반 동기화 대비 읽기 측에서 최소 수배에서 10배 이상의 성능 향상을 제공합니다

<br>

## 주요 특징

> std::mutex보다 사용하기 쉽고, 잘못된 사용을 방지하기 위한 스냅샷 격리를 지원하는 모던 C++ 라이브러리입니다.

- **헤더 온리**: 표준 라이브러리만으로 동작하며 외부 의존성 없음
- **락프리 읽기**: 캐시 워밍업 이후 읽기 경로에서 경합이 최소화되도록 구현
- **스냅샷 격리**: 호출 스레드에서의 스냅샷 격리를 위한 RAII 가드 패턴<br>
  (guard_pack을 사용하면 여러 스토리지를 한 줄로 로드 가능)
- **데이터 복제 없음**: 스레드별 데이터 깊은 복사(deep copy) 없음
- **선택적 백그라운드 소멸**: reclaimer_thread가 객체 소멸을 별도 스레드로 오프로드하여 리더 스레드의 부담을 줄임
<br>

## 성능

벤치마크 결과는 [PERFORMANCE.ko.md](docs/PERFORMANCE.ko.md)를 참고하세요.
<br>

## 빠른 시작

### API 개요

전통적인 RCU의 리더 등록, 유예 기간(grace period) 관리, 메모리 배리어등을 사용하지 않습니다.<br>

```cpp
storage = new_storage;      // 업데이트 예시 (std::shared_ptr<T> new_storage)
auto data = storage.load(); // guard 객체를 반환
```

*참고1: 전통적인 RCU와 달리, cppurcu는 이전 데이터를 직접 회수하지 않고 `std::shared_ptr`에 회수를 위임합니다. 이전 객체에 대한 `std::shared_ptr` 참조는 스코프 내에서 첫 번째 `load()` 호출 시 해제됩니다. 따라서 모든 `std::shared_ptr` 인스턴스에 대한 모든 참조가 해제될 때 메모리가 회수됩니다.*<br>

*참고2: 결과적으로, 업데이트 호출은 위치와 관계없이 데드락이 발생하지 않습니다.*

*참고3: 그렇다고 해서 cppurcu가 단순한 std::shared_ptr 래퍼는 아닙니다 — 매 읽기마다 원자적 참조 카운트 연산이 필요한 std::shared_ptr과 달리, RCU 시맨틱을 통해 락프리 읽기 측 접근을 제공합니다.*

### 기본 사용법

```cpp
#include <cppurcu/cppurcu.h>
#include <memory>
#include <string>
#include <map>

// 초기 데이터로 스토리지 생성
auto storage = cppurcu::create(std::make_shared<std::map<std::string, std::string>>());

// 읽기 (락프리) - data(guard 객체)를 반환
auto data = storage.load(); // cppurcu::guard<T>
if (data->count("key") > 0) {
  // 데이터 사용
}

// 업데이트 (즉시 적용, 회수는 std::shared_ptr에 위임)
auto new_data = std::make_shared<std::map<std::string, std::string>>();
(*new_data)["key"] = "value";
storage = new_data; // 또는 storage.update(new_data);
```

**⚠️ 중요:**

- `storage<T>` 인스턴스는 이를 사용하는 모든 스레드보다 오래 살아있어야 합니다
- 스레드가 아직 접근 중일 때 `storage`를 소멸시키면 정의되지 않은 동작(undefined behavior)이 발생합니다
- `storage`는 전역 변수, 정적 변수 또는 수명이 긴 멤버 변수로 선언해야 합니다.

### 스냅샷 격리

동일 스레드 내의 특정 스코프에서 복잡한 호출 체인을 통해 여러 `storage::load()` 호출이 발생하거나, 다른 스레드에서 데이터 업데이트가 발생하더라도, 해당 스레드 내의 모든 읽기 작업은 동일한 데이터 버전을 보도록 강제됩니다.

모든 가드가 소멸되면, 다음 load()는 업데이트된 버전을 가져옵니다

```cpp
{
  auto data = storage.load();    // 스냅샷 버전 1
  {
    storage.update(new_data2);   // 버전 2로 업데이트
    auto data1 = storage.load(); // 여전히 스냅샷 버전 1
  } // data1 가드 소멸
} // data 가드 소멸

storage.update(new_data3);       // 버전 3으로 업데이트
{
  auto data = storage.load();    // 버전 3을 로드
}
```

### 멀티 스토리지 스냅샷

여러 스토리지를 한 줄로 로드(스냅샷)해야 할 때, `cppurcu::load(const storage<Ts>&...)`를 다음과 같이 사용할 수 있습니다:

```cpp
#include <cppurcu/cppurcu.h>

// 여러 데이터 스토리지
auto storageA = cppurcu::create(...);
auto storageB = cppurcu::create(...);
auto storageC = cppurcu::create(...);

// 각 스토리지별 스냅샷, 스코프 내에서 일관되게 유지
const auto &[a, b, c] = cppurcu::load(storageA, storageB, storageC);

// 이 스코프 안의 모든 읽기는 다른 스레드에서 업데이트가 발생하더라도
// 각 스토리지에 대해 동일한 스냅샷을 봅니다.
a->lookup(...);
b->query(...);
c->find(...);
```

각 스토리지별 스냅샷이 특정 스코프 내에서 일관되게 유지돼야 한다면, 다음과 같이 코드를 작성할 수 있습니다:

```cpp
#include <cppurcu/cppurcu.h>
...........
{
  auto pack = cppurcu::load(storageA, storageB, storageC);
  // 또는 storage<T>::load()에서 반환된 `guard<T>` 객체를 사용
  auto pack = cppurcu::make_guard_pack(storageA.load(), storageB.load(), storageC.load());
  .....
  // calculate(...) 함수 내의 호출 체인 어딘가에서
  // storageA와 storageC가 사용되더라도,
  // pack은 각 스토리지의 스냅샷을 스코프 전체에 일관되게 유지합니다.
  my_class.calculate(...);
  .....
}
```

**참고:**<br>

- 각 스토리지는 여전히 독립적으로 버전이 관리됩니다; `guard_pack`은 편리한 멀티 스토리지 스냅샷 로딩을 위한 RAII 헬퍼이지, 크로스 스토리지 트랜잭션 메커니즘이 아닙니다.

### 백그라운드 소멸 사용 (선택 사항)

소멸자가 비용이 큰 객체의 경우, reclaimer_thread를 사용하여 백그라운드에서 소멸을 처리할 수 있습니다:

```cpp
#include <cppurcu/cppurcu.h>

// reclaimer_thread 생성
auto reclaimer = std::make_shared<cppurcu::reclaimer_thread>();

// reclaimer_thread와 함께 스토리지 생성
auto storage1 = cppurcu::create(std::make_shared<std::set<std::string>>(), reclaimer);

// reclaimer_thread는 템플릿 타입과 관계없이 사용할 수 있습니다.
auto storage2 = cppurcu::create(std::make_shared<std::vector<int>>(), reclaimer);

// 소스 어딘가에서...(업데이트)
storage1 = new_data;

// load를 호출하면, 업데이트된 객체를 가져옵니다.
// 이전 객체는 백그라운드 스레드(reclaimer_thread)에서 소멸됩니다
auto data = storage1.load();

```

**참고 (동작 변경)**<br>

- 이전에는 새 데이터로 업데이트할 때 이전 데이터를 처리하기 위해 `reclaimer_thread`(와 그 뮤텍스)를 사용했습니다.
- 현재는 리더가 더 이상 `reclaimer_thread`(또는 그 뮤텍스)를 사용하지 않으며; `reclaimer_thread`는 `storage<T>::update()`에서만 사용됩니다.
<br>

## API 레퍼런스

자세한 API 문서는 [API.ko.md](docs/API.ko.md)를 참고하세요.

빠른 참조:

- `cppurcu::storage<T>` - 주요 RCU 보호 데이터 스토리지
- `cppurcu::guard<T>` - 스냅샷 격리를 위한 RAII 가드
- `cppurcu::guard_pack<Ts...>` - 멀티 스토리지 스냅샷 헬퍼
- `cppurcu::reclaimer_thread` - 백그라운드 소멸 핸들러
<br>

## 설치

cppurcu는 헤더 온리 라이브러리입니다. cppurcu/ 디렉토리를 인클루드 경로에 복사하세요:

```bash
# 프로젝트에 헤더 복사
cp -r cppurcu/ /path/to/your/project/include/

# 또는 인클루드 경로에 추가
g++ -I./path/to/cppurcu your_code.cpp
```

### 요구 사항

- C++17 이상
<br>

## 테스트 빌드

포함된 테스트를 빌드하고 실행하려면:

```bash
# cppurcu와 mutex로 빌드
make

# liburcu 비교 포함 빌드 (liburcu-dev 필요)
make liburcu

# 테스트 실행
./rcu_bench 1000000  # 1M 항목으로 테스트, 필요 메모리 20GB
```

### Makefile 옵션

- `make` - cppurcu와 mutex 테스트 빌드 (기본)
- `make liburcu` - liburcu 비교 포함 테스트 빌드
- `make clean` - 빌드 산출물 제거
<br>

## 동작 원리

주요 클래스:

1. **`storage<T>`**: source, local, guard를 통합하는 사용자 대면 API
2. **`guard<T>`**: storage`<T>`::load()의 반환값, 스냅샷 격리를 위한 RAII 가드
3. **`source<T>`**: 권위 있는 데이터와 버전 카운터를 유지
4. **`local<T>`**: 스레드 로컬 캐싱 (얕은 복사만)
5. **`reclaimer_thread (선택 사항)`**: 객체 소멸을 처리하는 백그라운드 스레드

이 설계는 다음을 사용하지 않습니다:

- ABA 문제 해결책 (태그드 포인터 없음)
- 해저드 포인터
- 에포크 기반 리클레이머

### 읽기 경로

가드 생성 시 (각 `load()` 호출):

1. 캐시된 버전을 소스 버전과 비교 (스냅샷 격리를 위해 guard`<T>`::ref_count > 0이면 건너뜀)
2. 변경 없으면: 캐시된 raw 포인터 반환 (빠른 경로)
3. 변경되었으면: 캐시의 버전, shared_ptr, raw 포인터를 업데이트 (느린 경로)<br>
읽기 경로에서는 리클레이머 큐 작업이 발생하지 않습니다.

### 리클레이머 스레드 (선택 사항)

활성화되면, reclaimer_thread가 백그라운드에서 객체 소멸을 처리합니다:

1. storage::update()가 데이터 업데이트 시 교체된 shared_ptr을 리클레임 큐에 푸시
2. 워커 스레드가 주기적으로 스캔하여 unique()인 항목을 제거
3. 비고유(non-unique) 객체는 회수 가능할 때까지 큐에 남음
4. 리더를 차단하지 않고 객체를 소멸시켜, 비용이 큰 소멸자의 오버헤드를 줄임

### 스레드 안전성 보장

- 락프리 읽기
- 스레드 안전한 업데이트
- **요구 사항**: `storage<T>` 수명 > 스레드 수명
<br>

## 테스트

### 유닛 테스트

`test/` 디렉토리에 정확성, 스레드 안전성, 메모리 관리를 검증하는 종합적인 유닛 테스트가 포함되어 있습니다:

| 타겟                     | 설명                                                                  | 새니타이저          |
| ------------------------ | --------------------------------------------------------------------- | ------------------- |
| `unit_test`            | 핵심 기능: 기본 연산, 가드, 스냅샷 격리, 예약 해제, 리클레이머 스레드 | 없음                |
| `unit_test_guard_pack` | `guard_pack` 및 구조화된 바인딩 테스트                              | ASan + LSan + UBSan |
| `unit_test_tsan`       | 스트레스 테스트: 스레드 폭증, 고속 업데이트, 대형 객체, 중첩 가드     | ThreadSanitizer     |
| `unit_test_lausan`     | 메모리 누수 감지, nullptr 처리, 예외 안전성, 예약 해제 메모리 동작    | ASan + LSan + UBSan |

> **참고:** 유닛 테스트는 sanitizer 지원을 위해 Clang이 필요합니다 (ThreadSanitizer, AddressSanitizer, LeakSanitizer).

```bash
cd test

# 모든 유닛 테스트 빌드
make

# 개별 타겟 빌드
make test        # unit_test만
make guard_pack  # guard_pack 테스트만
make tsan        # ThreadSanitizer 테스트만
make lsan        # Leak/Address 새니타이저 테스트만

# 실행
./unit_test
./unit_test_guard_pack
./unit_test_tsan
./unit_test_lausan
```

### 벤치마크

벤치마크는 세 가지 접근 방식을 비교합니다:

1. **std::mutex** - 전통적인 락 기반 보호
2. **cppurcu** - 이 라이브러리
3. **liburcu** - 널리 사용되는 RCU 라이브러리 (선택 사항)

다양한 데이터 크기로 벤치마크를 실행하세요:

```bash
./rcu_bench 1000      # 1K 항목
./rcu_bench 100000    # 100K 항목
./rcu_bench 1000000   # 1M 항목, 필요 메모리 20GB
```
