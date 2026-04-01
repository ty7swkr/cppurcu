# 성능

*이 테스트들은 메모리 할당 오버헤드 없이 RCU 성능을 측정하기 위해 미리 빌드된 데이터 구조를 사용합니다.*<br>
*업데이트는 100ms마다 발생합니다.*

## 300K 항목, 10 리더 스레드, 2 라이터 스레드, 10초:
- Ubuntu 22.04, AMD Ryzen 7 8845HS / 16G<br>
*5회 벤치마크 실행의 평균*
<table>
  <thead>
    <tr>
      <th>구현체</th>
      <th>총 읽기</th>
      <th>상대 성능</th>
      <th>업데이트 (200회 시도)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>20.05M</td>
      <td>1.0x (기준)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+reclaimer</td>
      <td>368.6M</td>
      <td>18.0x</td>
      <td>200</td>
    </tr>
    <tr>
      <td>liburcu</td>
      <td>358.8M</td>
      <td>17.5x</td>
      <td>200</td>
    </tr>
  </tbody>
</table>
<br>

## 1M 항목, 10 리더 스레드, 2 라이터 스레드, 10초:
- Ubuntu 22.04, AMD Ryzen 9 7945HX / 32G
<table>
  <thead>
    <tr>
      <th>구현체</th>
      <th>총 읽기</th>
      <th>상대 성능</th>
      <th>업데이트 (200회 시도)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>80.6M</td>
      <td>1.0x (기준)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+reclaimer</td>
      <td>377.2M</td>
      <td>4.6x</td>
      <td>200</td>
    </tr>
    <tr>
      <td>liburcu</td>
      <td>341.3M</td>
      <td>4.2x</td>
      <td>200</td>
    </tr>
  </tbody>
</table>
<br>

- RedHat 8.7, VM Intel Xeon(Cascadelake) 2.6Ghz / 48G
<table>
  <thead>
    <tr>
      <th>구현체</th>
      <th>총 읽기</th>
      <th>상대 성능</th>
      <th>업데이트 (200회 시도)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>6.6M</td>
      <td>1.0x (기준)</td>
      <td>200</td>
    </tr>
    <tr style="font-weight: bold;">
      <td>cppurcu+reclaimer</td>
      <td>76.7M</td>
      <td>11.6x</td>
      <td>200</td>
    </tr>
    <tr>
      <td>liburcu</td>
      <td>79.6M</td>
      <td>12.0x</td>
      <td>175</td>
    </tr>
  </tbody>
</table>
결과는 환경과 구성에 따라 다를 수 있습니다.<br>
이 실행에서 liburcu는 약 175회 업데이트를 기록했지만, 이 수치는 환경과 구성에 따라 달라질 수 있습니다.
