# 收近超声触发距离并加快固定绕障

role: 独立调参任务，交由“黑线避障”集成。

start_commit: 078cfdb9125799ca4bb3998dd1809e488ddd67e0

result_commit: 本说明所在提交，完整SHA见外部交接包。

## 参数

| 项目 | 原值 | 新值 |
|---|---:|---:|
| 接近障碍基础触发距离 | 20 cm | 16 cm |
| 动态触发距离上限 | 30 cm | 22 cm |
| 清空距离 | 35 cm | 28 cm |
| 速度前瞻 | 160 ms | 100 ms |
| 固定两段前行的目标CPS及上限 | 1800 | 4000 |
| 固定斜向回线的目标CPS及上限 | 2100 | 4000 |

按用户最新要求，两段固定直行和斜向回线目标均为4000 CPS。新增StartFixed
入口复用原距离控制器，给固定路线的两段正向运动开放4000-CPS上限；较低请求仍被尊重。
普通Start入口保留1800-CPS上限，反向/自适应短探测不继承较快速度。
main前行和回线请求设为4000；退出固定路线后的自适应短探测先将传入旧接口的
请求限制到其接受的3600以内，再由旧接口封顶1800，避免高请求导致启动失败。
退出固定路线后的自适应
连续回线仍封顶2100。位置控制、STOP、故障所有权和按距离计算超时保持。

横向300mm、旁侧360mm、三次转向及45°回线不变；转向目标仍2500 CPS。
10cm单回波立即制动、绕障前进段15cm保护、旧朝向回波隔离和红外关闭保持。
普通提前触发仍需两次连续有效近回波。

files_changed: Core/Inc/line_bypass_travel.h、Core/Src/line_bypass_travel.c、
Core/Src/line_obstacle_bypass.c、Core/Src/main.c、tests/gyro_turn/test_real_drive.c、
test_return_gate.c、test_ultrasonic_recovery.c、tests/line_recovery/
test_line_turn_load.c、check_mode12_integration.py、本说明。

verification_completed:

- 完整gyro_turn回归：IR开/关的左右300/360mm路线，四轮固定直行及5秒
  斜行请求均4000 CPS；转角、路程、外侧捕获、回退和STOP通过。
- 完整line_recovery两种搜索速度；新固定入口的4000上限、较低请求、
  原入口1800封顶、无效参数、外部STOP和位置所有权互斥通过。
- 超声23cm不触发、22/21cm两次新回波触发、10cm单回波制动通过。
- 固定路线4000-CPS配置回退至自适应短探测的入参限幅及连续回线2100上限通过。
- 正式ARM编译通过，text/data/bss=103036/64/12032，BIN103104字节。
- 状态及git diff --check通过；增量对最新综合16b5092的只读应用检查通过。

BIN SHA256: 67D4268EBB5FDAE31EE999CED74FB4BEC3821CEBDC96DE1A2D71571D90D2E85A

HEX SHA256: 9ECEA307F0AEE692F46597E17E428311EE1905BDEED0DB26143803FBB28D24DB

not_verified: 未用串口、未烧录、未移动小车；实际轮速、刹车距离和绕障耗时
未实测。未改PROJECT_STATE.md、未推送。

risks_or_assumptions: 速度增幅是目标CPS，不是实测车速或圈速保证。更快行进
可能增加惯性和滑移，仍需在用户实际赛道比较；本次没有改编码器标定。

integration_notes: 最新综合已含前序改动，只cherry-pick本提交，运行寻线/
陀螺仪回归并重编译综合固件。保留综合模式3/4，不整体覆盖工作树。包内BIN
仅作构建证据，勿直接当作完整最新综合固件烧录。回退本提交恢复20～30cm
触发及原固定绕障速度。
