# 提前超声触发，并隔离转弯期间的旧测距

role: 独立寻线/绕障修复任务，交由“黑线避障”集成。

start_commit: 5bb2abd2ec0b2eb7f37500d55423fa47899b5d9f

result_commit: 本说明所在提交，完整 SHA 见外部交接包。

## 尺寸与编码器核对

用户本轮确认轮径为 47 mm，报告 24-cm 段实际不到一半。47-mm 轮径不变。
旧项目实测记录还记载轮距/轴距各 129 mm；它们不用于当前直行距离换算。
原文件为 docs/history/experiments/README_ENCODER_FIGURE8.md。

课程指导书把电机称为 310 编码电机。厂家 MD310Z20_7.4V 参数为 13 线、
1:20 减速；当前 AB 相每个合法 Gray 跳变计一次，采用四倍频，故该型号
理论上应为 13×20×4=1040 计数/输出轴圈，不能再乘一次 4。
来源：https://www.yahboom.net/public/upload/upload-html/1744700910/Motor%20introduction%20and%20usage.html

软件计数检查没有发现同一状态重复累计、四轮和直接充当单轮距离或单位
cm/mm 漏换算。24 cm 请求约每轮 1690 计数，12 cm 约 845 计数；四轮和达到
四倍目标且最慢轮达到 75% 才进入正常完成制动。该检查不证明每台实物电机
一定为该减速比，也不能排除滑移、轮毂松动或电气计数异常。

因此本次不修改轮径、每圈计数或盲目倍增距离。核实每圈计数必须在获得相应
硬件授权后，对车轮实体标记的一整圈读取前后原始计数差，不能以程序运行
1040 计数后停下反证“一圈就是1040”。当前未做此项实物测量。

## 确认的软件问题与修改

原绕障测距在转动时也触发超声，近障结果缓存最多 250 ms。转向完毕开始
横移时，这个面向旧朝向的近障结果仍能立即触发 fixed_obstacle_fallback，
使固定 240-mm 段提前变为自适应短段。这是可复现的软件行为；没有现场日志
证明它就是这次“不到一半”的唯一原因。

新增 line_bypass_range 模块：只在 BYPASS_DRIVING 发起可用于前方避障的
测距；转动/停止期间继续服务驱动并丢弃结果。每次进入前进段清除旧近障标记，
只接受该段发起的测距，跨转向尚未返回的旧请求也被丢弃。保持 60-ms 发射
间隔和 250-ms 新鲜度限制。模式切换与两种绕障启动入口均重置状态。

超声阈值调整：

| 项目 | 原值 | 本候选 |
|---|---:|---:|
| 接近障碍基础触发距离 | 10 cm | 20 cm |
| 按速度计算的触发距离上限 | 16 cm | 30 cm |
| 清空距离 | 18 cm | 35 cm |
| 速度前瞻时间 | 70 ms | 160 ms |
| 单次原始近距回波立即制动 | 5 cm | 10 cm |
| 绕障直行段的新回波近障阈值 | 10 cm | 15 cm |

普通提前触发仍需两次连续新回波。入口触发与绕障中近距保护分开，避免
入口较远阈值无差别作用于所有拐角。红外仍关闭，遥控 STOP 保留。

## 验证和交接

files_changed: Core/Inc/line_bypass_range.h、Core/Src/line_bypass_range.c、
Core/Src/main.c、Core/Src/ultrasonic_avoid.c、tests/gyro_turn/run.cmd、
test_bypass_range.c、test_ultrasonic_recovery.c、
tests/line_recovery/check_mode12_integration.py、本说明。

verification_completed:

- 完整 gyro_turn 回归：旧朝向近回波、跨段在途回波、有效新回波、15-cm
  边界、复位、过期和计时回绕；30-cm 双回波触发与10-cm 单回波制动。
- 完整 line_recovery 两种搜索速度及应用绑定检查通过。
- 正式 ARM 编译通过：text/data/bss=102852/64/12032，BIN=102920 字节。
- 项目状态、git diff --check 及隔离索引对综合 9ba87c3 的三方应用检查通过。

BIN SHA256: DB8EEDC37F3A7F3BB4777D22D88D388FCC25765AC55C6EAC34862E91C14C87CD

HEX SHA256: 06A0A7A4E14FEDBDEEF3ADBB171FE29966111F4557B7EAE7CB82DA0117ADDD14

not_verified: 未使用串口、未烧录、未移动小车；实际每圈计数、真实段长和
避障间隙尚未验证，不能声称已消除实物位移偏差。未修改 PROJECT_STATE.md。

risks_or_assumptions: 提前触发会改变固定路线的起点，保留的 24/12-cm 几何
并不自动保证越过所有矩形尺寸；须结合障碍长度和超声/轮中心位置实测。
前进段开始后会等待首个新测距，旧朝向数据不再提供保护。若新版本仍短，
先确认 BYP2 的 FIX、FB、AT、FT 是否显示提前回退；若显示正常完成240mm，
而实体仍不足120mm，再重点核对每圈原始计数及地面滑移。

integration_notes: 最新综合已包含此前改动，只 cherry-pick 本提交并运行
寻线/陀螺仪回归、正式编译。新 Core/Src 文件被正式构建脚本自动纳入。
保留综合模式 3/4 代码，不整体覆盖。工作树 BIN 仅为编译证据，不能直接
替代完整综合固件。回退本提交可恢复原阈值及测距流程。
