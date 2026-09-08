# DFPlayer Mini 遥控播放与跨项目移植说明

## 功能结果

- 主板：YB-DSF01-V1.1 / STM32F103ZETx。
- 接口：J8 的 UART4，`PC10=TX`、`PC11=RX`，9600 8N1。
- 音频：TF 卡 `/mp3/0001.mp3`起始，默认循环当前音频。
- 遥控：方向区中间键播放/重新开始`0001.mp3`；右键播放下一音频；
  上/下键每次把音量提高/降低2级；左键保留不用。
- 遥控解码值：上`0x01`、中`0x05`、右`0x06`、下`0x09`。这与遥控器
  图上按位反向印刷的`80/A0/60/90`一一对应。
- 串口：`b`播放第一首，`n`下一首，`+`/`-`调音量，`x`或`X`停止。
- 上电：保持静音并等待3秒让DFPlayer和TF卡启动，先把音量设为10/30并
  启用当前音频循环；启动期间的播放/下一首请求会保留，初始化后发送。
- 发送：UART4 TXE中断逐字节输出，不在控制主循环中忙等。
- 安全：显式STOP、模式切换和现有安全蜂鸣覆盖会停止用户触发的外部音频；
  远程播放本身不会启动或改变行驶模式。
- 板载PG12蜂鸣器仍保留给原有故障、安全和视觉鸣笛逻辑。

## 接线

```text
J8-1 5V       -> DFPlayer VCC
J8-2 PC11/RX  <- DFPlayer TX
J8-3 PC10/TX  -> 680 ohm~1 kohm series resistor -> DFPlayer RX
J8-4 GND      -> DFPlayer GND

DFPlayer SPK1 -> 8-ohm speaker lead 1
DFPlayer SPK2 -> 8-ohm speaker lead 2
```

`SPK1/SPK2`是桥接功放的两个输出，任何一根都不能接地。若改用8002B功放
喇叭一体模块，应改为`DFPlayer DAC_L -> 8002B SIG`，不能把`SPK1/SPK2`
送入另一级功放。

## TF卡

- FAT16/FAT32，不超过32 GB。
- 断电插拔。
- 音频按`/mp3/0001.mp3`、`/mp3/0002.mp3`……连续命名；第一首由中键
  显式选择，右键使用DFPlayer的下一文件命令。
- K210正在使用的卡应先完整备份；推荐给DFPlayer单独使用一张卡。

## 工程文件

| 文件 | 职责 |
|---|---|
| `Core/Inc/dfplayer_protocol.h` | 纯协议接口 |
| `Core/Src/dfplayer_protocol.c` | 生成带校验和的10字节命令包 |
| `Core/Inc/dfplayer_mini.h` | 可调启动延时、命令间隔、默认音量/循环和公开API |
| `Core/Src/dfplayer_mini.c` | PC10/PC11、UART4和非阻塞命令调度 |
| `Core/Inc/ir_remote_keymap.h` | 可主机测试的遥控命令映射接口 |
| `Core/Src/ir_remote_keymap.c` | 数字、STOP和方向区音频键映射 |
| `Core/Src/stm32f1xx_it.c` | 转发`UART4_IRQHandler()` |
| `Core/Src/main.c` | 遥控、诊断串口和安全停止集成 |
| `tests/dfplayer/` | 协议包和校验和主机测试 |

## 移植到另一个STM32工程

1. 复制四个`dfplayer_*.[ch]`文件，并确保构建系统会编译两个`.c`文件。
2. 在目标板修改`dfplayer_mini.c`里的UART实例、GPIO和外设时钟；协议层无需改。
3. 在启动路径调用一次`DfPlayerMini_Init()`。
4. 在主循环每次调用`DfPlayerMini_Task(HAL_GetTick())`。
5. 在对应串口ISR里调用`DfPlayerMini_UART4_IRQHandler()`。
6. 播放、下一首和音量事件分别调用`DfPlayerMini_PlayMp3Track(1U)`、
   `DfPlayerMini_Next()`和`DfPlayerMini_AdjustVolume()`；STOP/故障调用
   `DfPlayerMini_Stop()`。`DfPlayerMini_SetLoopCurrent(1U)`启用单曲循环。
7. 如果TF卡或模块启动较慢，覆写`DFPLAYER_MINI_BOOT_DELAY_MS`；音量范围
   只能是0～30。

## 首次测试

1. 断电检查VCC/GND、RX/TX交叉和`SPK1/SPK2`，车轮离地，遥控STOP在手。
2. TF卡至少放入已验证的`/mp3/0001.mp3`和`0002.mp3`，上电后等待3秒。
3. 先在STOP模式短按方向区中间键；应播放第一首并在结束后循环，车轮不得
   因音频按键动作。
4. 短按右键应切换下一首；逐次短按上/下键应每次变化2级音量。长按产生的
   NEC repeat帧不重复上报，因此连续调节需要连续短按。
5. 再输入诊断串口`b/n/+/-`验证等价入口，输入`x`验证停止。
6. 按数字0、切换模式或触发安全状态时，外部音频应停止且电机安全逻辑不变。
7. 若无声，先确认模块供电、TF卡格式、文件路径、音量和SPK接线；电脑构建
   通过不能证明真实模块、喇叭或车辆已经工作。

## 当前边界

- 未连接DFPlayer `BUSY`引脚，因此软件只知道已经请求播放，不知道扬声器的
  实际结束时刻；这不影响按键播放和显式停止。
- RX只用于保持标准双向布线，当前驱动会丢弃模块主动上报，不做卡状态诊断。
- 本功能提交只代表源代码和电脑端验证；在明确获得“烧录”授权前不得写板。
