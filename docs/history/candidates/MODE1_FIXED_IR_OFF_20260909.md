# 固定绕障：临时关闭红外避障

role: 独立故障隔离候选，交由“黑线避障”集成。

start_commit: c7f68e63954a8c5c72b7d84a7530a47ab0900019

result_commit: 本说明所在提交，完整 SHA 见外部交接包。

用户观察到检测障碍后转向超过 90°且偏离固定路径。源码中红外既能提前触发
绕障，也能在固定直行段判断内侧过近而进入自适应回退、追加外转。因此可以
造成这种轨迹变化，但没有现场角度/阶段记录，尚不能确定它是本次唯一原因。

## 本候选

- main 的 EXP7_IR_AVOID_ENABLED=0。启动时关闭红外避障使能，PE5/PE6
  拉高关闭低电平有效的发射管，跳过红外预热和基线标定。
- 保留 ir_avoid_init 对 ADC3 的初始化，因为电池测量共享 ADC3。
  禁用后的 ir_avoid_read 直接返回无障碍，不执行障碍通道采样。
- 禁止红外提前启动绕障及左右状态灯更新。自动恢复读取红外时同样只能得到
  禁用结果，不能改变恢复方向；障碍触发保留前方超声。
- LineObstacleBypassConfig 新增 infrared_enabled，main 将其设为 0。
  控制器在单一输入入口屏蔽全部侧面 IR 数据，涵盖固定路线、自适应回退和
  无效输入检查。刻意关闭不等同于传感器故障，不会触发“红外无效”停车。
- IRON=0 明示策略关闭；此时 BYP2 的侧面 IR 数值是内部中性占位值，不能
  用于判断现场是否存在障碍。前方超声与黑线数据原样保留。
- 固定 240/120-mm 路线、三次转角、45°回线、速度、陀螺仪、编码器和
  遥控 STOP 保持。四路寻线模块及红外遥控属于独立接口，没有关闭。
- 前方超声仍可触发额外避让，陀螺仪/编码器异常仍有原来的恢复路径，因此
  不能把关闭红外表述为所有情况下都不再转过 90°。

files_changed: Core/Inc/line_obstacle_bypass.h、Core/Src/line_obstacle_bypass.c、
Core/Src/main.c、tests/gyro_turn/test_real_drive.c、test_return_gate.c、
tests/line_recovery/check_mode12_integration.py、本说明。

verification_completed:

- gyro_turn 全套，包括 IR 开/关的左右固定路线整链仿真；关时输入无效、
  近距离、饱和与跳变数据，不触发固定路线回退，末段仍能见线接管。
- 隔离输入测试确认原输入不被修改、超声持续阻挡仍触发避让、STOP 有效、
  空指针仍按输入异常处理；IR 开启时的原回退测试继续通过。
- line_recovery 两种搜索速度全套；应用绑定检查涵盖发射管关闭、跳过标定、
  禁用触发/显示、保留电池初始化与遥控中断。
- sign_line、dfplayer、vision_line_v4 回归通过。
- 正式 ARM 编译通过：text/data/bss=102652/64/12032，BIN=102720 字节。
  项目状态及 git diff --check 通过。
- 对最新综合 ec2dd2f 的隔离索引三方补丁应用成功，只有本增量文件改变。
  直接文本应用曾因模式 3/4 RGB 注释的上下文差异不匹配；三方应用无冲突，
  未覆盖综合任务的源文件、索引或分支。

BIN SHA256: 5C1C19616ABB7BDD243AB95D3CF6774D09E316A6836C6F67DF5A592163CEEE93

HEX SHA256: BE30CC50DF59ECA7D222D6FE3700428823A125D0B62291071818945C57D7D72D

not_verified: 未使用串口、未烧录、未移动小车、未验证真实转角或轨迹。
未修改 PROJECT_STATE.md、未推送。软件写出关闭电平不等于测量过实际发射光。

risks_or_assumptions: 这是用户要求的红外隔离测试版本。侧面障碍不再由红外
触发避让；超声覆盖之外的障碍不具备原来的侧面检测能力。保持同一赛道条件
比较超转是否消失；若仍存在，应检查陀螺仪实测转角、编码器降级与超声回退。

integration_notes: 在最新综合分支 cherry-pick 本次单个提交，再跑回归和
编译综合固件。不要整体替换为此 main 派生工作树；工作树 BIN 只作编译证据。
恢复红外时将 EXP7_IR_AVOID_ENABLED 改为 1 并重编译，或撤销本次提交。
24-cm 固定路线及之前的提速可保留。
