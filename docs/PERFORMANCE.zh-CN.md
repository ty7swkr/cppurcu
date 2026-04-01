# 性能

*这些测试使用预构建的数据结构来测量 RCU 性能，不包含内存分配开销。*<br>
*更新每 100ms 发生一次。*

## 300K 项目，10 个读取线程，2 个写入线程，10 秒：
- Ubuntu 22.04, AMD Ryzen 7 8845HS / 16G<br>
*5 次基准测试运行的平均值*
<table>
  <thead>
    <tr>
      <th>实现</th>
      <th>总读取量</th>
      <th>相对性能</th>
      <th>更新（200 次尝试）</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>20.05M</td>
      <td>1.0x（基准）</td>
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

## 1M 项目，10 个读取线程，2 个写入线程，10 秒：
- Ubuntu 22.04, AMD Ryzen 9 7945HX / 32G
<table>
  <thead>
    <tr>
      <th>实现</th>
      <th>总读取量</th>
      <th>相对性能</th>
      <th>更新（200 次尝试）</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>80.6M</td>
      <td>1.0x（基准）</td>
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
      <th>实现</th>
      <th>总读取量</th>
      <th>相对性能</th>
      <th>更新（200 次尝试）</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>std::mutex</td>
      <td>6.6M</td>
      <td>1.0x（基准）</td>
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
结果可能因环境和配置而异。<br>
在本次运行中，liburcu 记录了约 175 次更新，但此数值可能因不同环境和配置而变化。
