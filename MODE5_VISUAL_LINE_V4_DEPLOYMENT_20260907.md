# 模式 5 视觉寻线 v4 部署记录（2026-09-07）

## 部署对象

- 分支：`feature/mode5-visual-line-v4`
- STM32 源提交：`27a05aa2e2bceb7c8fdd0b2044297e47752fc972`
- 回滚标签：`rollback/2026-09-07-before-mode5-visual-line-v4`
- STM32 端口：USB-SERIAL CH340K `COM11`
- K210 端口：USB-SERIAL CH340 `COM14`
- K210 板间发送：IO8/UART1_TX -> STM32 PD6/USART2_RX，115200 8-N-1

## 构建与写入

- 正式 BIN：79,172 字节
- BIN SHA-256：`1F7D39C95EA321A26760012250CABDB3B930370933863D9BC61C5FBFCBB8729E`
- HEX SHA-256：`B1EBE492CC64153DF2C3B5732B6ED8BA6617E095DD3BEBFAF2C7A0415C638C57`
- ROM Bootloader：57,600 baud，偶校验
- 选择性擦除：39 个 2 KiB 应用页
- 保留页：`0x0807F800..0x0807FFFF`
- 写入结果：`WRITE OK: 79172 bytes`
- 独立只读回读：79,172 字节逐字节等于正式 BIN
- 回读 SHA-256：`1F7D39C95EA321A26760012250CABDB3B930370933863D9BC61C5FBFCBB8729E`
- 校准页回读 SHA-256：`D0FF1B294B5288D1AE1421EADF5B2D38A8752B76D472FF30BED9028E25B1C5B8`
- 返回应用：`GO OK: 0x08000000`

最初一次调用被 Windows PowerShell 5 的语法解析拒绝，发生在打开烧录脚本前，
未擦除、未写入。第二次在主板未运行时无法同步 Bootloader，同样未擦除、未写入。
主板重新上电后才执行上述成功写入。

## 静态运行与板间通信

- K210 启动后持续输出 v4 的 `s/off/ang/bot/obs`，实测约 8--9 FPS。
- STM32 在 STOP 状态发送 `v`，首次得到：
  `VLINK V4=77 SIGN=0 BAD=5 F=1 AGE=80`。
- 后续计数由 `77` 增至 `229`，`BAD=5` 保持不变。
- 最终重新烧录、复位后的三次采样：
  - `V4=241 BAD=20 AGE=12`
  - `V4=258 BAD=20 AGE=66`
  - `V4=276 BAD=20 AGE=50`
- `V4` 持续增长、帧龄低于 150 ms、错误计数不再增长，证明 K210 v4 帧已通过
  IO8 -> PD6 到达 STM32 的严格八字段解析器。
- `SIGN=0`，模式 5 链路没有收到或使用路标帧。
- 全程保持 STOP；遥测为 `DRV M=0`，四轮请求、控制目标和 PWM 均为 0。
- 最终电池读数约 8.38 V。

## 验证边界

- 通过：主机单元测试、模式 3/4 回归、ARM 正式构建、STM32 写入、完整回读、
  应用启动、STOP 状态 K210->STM32 板间通信。
- 未执行：模式 5 车轮离地测试、实际转向方向验证、地面循线、锐角转弯、视觉障碍停车。
- `VERIFY OK` 和 `VLINK` 只证明固件字节及通信链路，不证明车辆运动效果。
