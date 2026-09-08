# DFPlayer Mini 遥控播放与跨项目移植说明

## 功能结果

- 主板：YB-DSF01-V1.1 / STM32F103ZETx。
- 接口：J8 的 UART4，`PC10=TX`、`PC11=RX`，9600 8N1。
- 音频：按 TF 卡物理文件顺序选择，当前音频默认单曲循环；下一曲和
  上一曲不要求 `/mp3/NNNN.mp3` 文件名。
- 遥控：方向区中间键切换播放/暂停；左/右键播放上一曲/下一曲；
  上/下键每次把音量提高/降低2级。
- 遥控解码值：上`0x01`、左`0x04`、中`0x05`、右`0x06`、下`0x09`。
  这与遥控器图上按位反向印刷的编码一一对应。
- 串口：`b`播放/暂停，`p`上一曲，`n`下一曲，`+`/`-`调音量，
  `x`或`X`停止。
- 上电：保持静音并等待3秒让DFPlayer和TF卡启动，先把音量设为20/30并
  启用当前音频循环；启动期间的播放/下一首请求会保留，初始化后发送。
- 发送/接收：UART4 TXE/RXNE 中断处理命令和当前曲目查询，不在控制
  主循环中忙等。
- 记忆：最近曲目的物理文件序号追加写入专用 Flash 页面；暂停后同次
  上电可从原时间位置继续，断电后重新播放时从记忆曲目的开头开始。
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

## TF卡与曲目顺序

- FAT16/FAT32，不超过32 GB。
- 必须断电插拔；热插拔后仅复位 STM32 不一定能让 DFPlayer 重新挂载卡，
  应让 DFPlayer 模块本身彻底断电后再上电。
- 上一曲/下一曲使用 DFPlayer 原生物理顺序命令，因此文件名与目录无需
  固定为 `0001.mp3`。物理顺序通常受文件复制到卡中的先后影响，不保证
  等同于 Windows 按文件名排序。
- 曲目记忆保存的是物理文件序号。更换或重新整理 TF 卡后，同一序号可能
  指向另一首音频；需要时可用左/右键重新选择。
- K210正在使用的卡应先完整备份；推荐给DFPlayer单独使用一张卡。

## 工程文件

| 文件 | 职责 |
|---|---|
| `Core/Inc/dfplayer_protocol.h` | 纯协议接口 |
| `Core/Src/dfplayer_protocol.c` | 生成带校验和的10字节命令包 |
| `Core/Inc/dfplayer_mini.h` | 可调启动延时、命令间隔、默认音量/循环和公开API |
| `Core/Src/dfplayer_mini.c` | PC10/PC11、UART4双向中断和非阻塞命令调度 |
| `Core/Inc/audio_resume_store.h` | 曲目记忆接口与专用 Flash 页面定义 |
| `Core/Src/audio_resume_store.c` | 掉电安全的追加记录、延迟擦页和恢复 |
| `Core/Inc/ir_remote_keymap.h` | 可主机测试的遥控命令映射接口 |
| `Core/Src/ir_remote_keymap.c` | 数字、STOP和方向区音频键映射 |
| `Core/Src/stm32f1xx_it.c` | 转发`UART4_IRQHandler()` |
| `Core/Src/main.c` | 遥控、诊断串口和安全停止集成 |
| `tests/dfplayer/` | 协议包和校验和主机测试 |

## 移植到另一个STM32工程

1. 复制四个`dfplayer_*.[ch]`文件；需要掉电记忆时再复制
   `audio_resume_store.[ch]`，并确保构建系统会编译对应`.c`文件。
2. 在目标板修改`dfplayer_mini.c`里的UART实例、GPIO和外设时钟；协议层无需改。
3. 在启动路径先调用`AudioResumeStore_Init()`，再调用
   `DfPlayerMini_Init()`和
   `DfPlayerMini_SetResumeTrack(AudioResumeStore_GetTrack())`。
4. 在主循环每次调用`DfPlayerMini_Task(HAL_GetTick())`。
5. 在对应串口ISR里调用`DfPlayerMini_UART4_IRQHandler()`。
6. 中键、左键、右键和音量事件分别调用
   `DfPlayerMini_TogglePlayPause()`、`DfPlayerMini_Previous()`、
   `DfPlayerMini_Next()`和`DfPlayerMini_AdjustVolume()`；STOP/故障调用
   `DfPlayerMini_Stop()`。`DfPlayerMini_SetLoopCurrent(1U)`启用单曲循环。
7. 主循环取出`DfPlayerMini_TakeTrackChanged()`后调用
   `AudioResumeStore_RequestTrack()`和`AudioResumeStore_Task()`；只有车辆
   STOP 时才向`AudioResumeStore_Task()`允许整页擦除。
8. 在链接脚本中把应用区截止到`0x0807EFFF`。本工程使用
   `0x0807F000..0x0807F7FF`保存曲目，继续保留
   `0x0807F800..0x0807FFFF`的电机/转向标定。
9. 如果TF卡或模块启动较慢，覆写`DFPLAYER_MINI_BOOT_DELAY_MS`；默认音量
   为20/30，遥控上键可继续提高到30/30。若喇叭失真或供电不稳应调低。

## 首次测试

1. 断电检查VCC/GND、RX/TX交叉和`SPK1/SPK2`，车轮离地，遥控STOP在手。
2. TF卡至少放入两首可播放音频，给 DFPlayer 完整断电再上电并等待3秒。
3. 先在STOP模式短按方向区中间键；应播放记忆的曲目并在结束后循环，
   车轮不得因音频按键动作。再按一次应暂停，第三次应从暂停处继续。
4. 短按左/右键应切换上一曲/下一曲；逐次短按上/下键应每次变化2级音量。长按产生的
   NEC repeat帧不重复上报，因此连续调节需要连续短按。
5. 再输入诊断串口`b/p/n/+/-`验证等价入口，输入`x`验证停止。
6. 按数字0、切换模式或触发安全状态时，外部音频应停止且电机安全逻辑不变。
7. 若无声，先确认模块供电、TF卡格式、文件路径、音量和SPK接线；电脑构建
   通过不能证明真实模块、喇叭或车辆已经工作。

## 当前边界

- 未连接DFPlayer `BUSY`引脚，因此软件仍不能直接确认扬声器是否真的发声；
  UART RX 只用于读取曲目序号，不替代实际听音验收。
- 标准串口协议可以记忆曲目序号和同次上电的暂停状态，但不能可靠保存
  毫秒播放进度；整机完全断电后只能从上次曲目的开头继续。
- 普通复位/断电会保留曲目页；重新烧录时，只有烧录工具同时保留
  `0x0807F000`与`0x0807F800`，才能继续保留曲目和电机标定。整片擦除会
  清除这两类数据。
- 本功能提交只代表源代码和电脑端验证；在明确获得“烧录”授权前不得写板。
