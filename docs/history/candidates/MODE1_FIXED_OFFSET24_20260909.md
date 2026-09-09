# 固定绕障横移改为 24 cm

role: 独立绕障参数修改，交由“黑线避障”集成。
start_commit: 74c0d66cdebaa87854fb97ac5f1be8eb83630a99
result_commit: 本说明所在提交，完整 SHA 见交接包。

仅把 FIXED_OFFSET_MM 从 180 改为 240。同步代码注释、定长请求断言及
真实模块仿真的位移边界。旁侧段 120 mm、回线 45°、速度、回线捕获、
异常回退与 STOP 行为不变。此参数覆盖前一份固定路线说明中的 18 cm；
前一份记录的旧测试与产物哈希仍属于旧提交。

理想轮中心路径现在为 (0,0) -> (240,0) -> (240,120) -> (0,360) mm；
最后斜行仍以真实探头见线结束，不使用理想终点作为停止条件。

files_changed: Core/Src/line_obstacle_bypass.c、Core/Inc/line_obstacle_bypass.h、
Core/Src/main.c、tests/gyro_turn/test_real_drive.c、test_return_gate.c、本说明。

verification_completed:

- gyro_turn 全套通过，含左右镜像 240/120-mm 连续段、5 秒斜行、外侧捕获、
  精确距离请求、角度修正、所有阶段 STOP、异常回退及计时回绕。
- line_recovery 全套通过，包括两种搜索速度及模式 1/2 集成检查。
- 正式 ARM 编译通过，text/data/bss=103344/64/12032，BIN=103412 字节。
- git diff --check、项目状态检查通过。
- 增量对最新综合 5fdb84a 的只读 git apply --check 通过；相关绕障源文件
  与当前工作树修改前一致，不引入旧模式 3/4 源码。

BIN SHA256: A84C39692620E1CC5D0AB811737E2B0663BCF43D02AE80EA84EE4B05229790BE

HEX SHA256: 171BC4150B0D413D002FB26CF556FA3A4F15BB6C9C63470E100A59BB2CCB1AB5

not_verified: 未使用串口、未烧录、未移动小车、未验证真实轨迹或障碍间隙。
未修改 PROJECT_STATE.md、未推送远程。

risks_or_assumptions: 24 cm 是从入口停车位置计量的轮中心位移，轮胎滑移
仍可能导致实际距离偏差。最后 45°回线的理想交点也会相应向前移动 6 cm。

integration_notes: 最新综合已包含原固定路线，仅 cherry-pick 此提交后
运行寻线/陀螺仪回归并重新编译综合固件。工作树 BIN 只作编译证据，勿直接
当成完整最新综合固件烧录。回退此提交即可恢复 18 cm，其他功能保持。
