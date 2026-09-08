# 可复用角度模块：GyroTurn API v1

其他模式调用 `GyroTurn_*`，不必经过 `LineBypassTurn_*`，也不必复制绕障状态机。
它提供**相对角度原地转弯**，不是直行航向保持、指定半径圆弧、绝对航向定位或多实例并发控制。

## 交给其他对话

把下面这段发给负责接入的任务，并说明希望接入的模式和转角触发条件：

> 请复用 `F:/myproject/jidian/worktrees/mpu6050-angle/reusable/gyro_angle/README.md` 中的 GyroTurn API v1，为我指定的模式接入相对角度转弯。先按目标项目 AGENTS.md、PROJECT_STATE.md 和状态检查器核对实际 Git 状态。本模块位于 feature/mpu6050-angle，算法源最初提交为 842fc1faf96a69c3e21c2eb61b2c32e04f96eda6；模块包 manifest.json 记录导出时的准确提交和逐文件 SHA256。只接需要的模块与调用点，不直接替换目标项目 main.c 或整套固件。共用一份 MpuYaw 服务和 GyroTurn 实例；转弯期间只有当前模式写电机；模式退出/遥控 STOP 必须取消动作，故障不得触发旧的自动恢复运动。给出构建、测试和提交码，不烧录。

新任务按仓库规则从届时的 current main 建立自己的工作树。当前分支是独立候选，不能仅凭分支名当作已集成版本；本次没有修改共享项目状态或通用记忆。

## V7 调度与恢复约定

本分支在 V6 上修正了延迟采样和标定等待。应以当前导出包的 manifest 和 Git 提交为准；上面的初始分支路径只用于追溯来源。

- 全局仍有一份 `MpuYaw_Task`；`GyroTurn_Start/Task` 在使用角度前调用 `MpuYaw_Refresh`。直接读取 yaw 的模式也应先 Refresh。它只服务已经标定的传感器，不清原点；5 ms 门限及消费式 FIFO 避免重复积分。
- 一次最多读 8 帧（96 字节），剩余帧下次处理，`pending_frames` 非零时不可用于当前角度控制。真正 FIFO 溢出、总线错误、量程异常仍锁定故障。
- 非 STOP 或轮子未静止时，未标定服务进入 `MPU_YAW_WAIT_STATIONARY`（状态 4），持续排空旧数据；回到静止后重新收集 200 帧。运动等待不再消耗 10 秒标定期限；静止但姿态/数据持续无效仍会超时。
- 无完整新帧超过 30 ms 时，当前转弯以 `GYRO_TURN_DATA_GAP`（7）终止；持续无帧超过 250 ms 才锁定传感器 STALE。补读到完整积压数据可以恢复 READY，不能自动接着转。
- 应用只能在整车 STOP 且实际静止后调用 `GyroTurn_ClearTransientFault()`；该接口要求底盘无故障、传感器新鲜，只清 DATA_GAP。还需新的模式/动作指令才启动，不重标定；其他故障仍要求显式处理。

产品工程运行 `tests/gyro_turn/run.cmd`，包含真实 DriveBase/绕障组合回归；导出包运行同名脚本，只包含可独立运行的模块和总线测试。两者验证范围不同。

## 最小文件集合

| 文件 | 用途 | 依赖 |
|---|---|---|
| `Core/Inc/gyro_turn.h` | 模式使用的公开接口及调用约定 | stdint |
| `Core/Src/gyro_turn.c` | 转角闭环、减速、制动、故障 | MpuYaw、DriveBase、HAL_GetTick |
| `Core/Inc/mpu6050_yaw.h` | 传感器快照、状态、平台接口 | stdint |
| `Core/Src/mpu6050_yaw.c` | FIFO、标定、相对 yaw 积分 | MpuBus、string |
| `Core/Src/mpu6050_bus.c` | 本车 PB10/PB11/PE0 通信 | STM32F1 HAL/CMSIS、DWT |

同一工程保留各一份，所有模式包含同一个 `gyro_turn.h`。无需复制 `line_bypass_turn.c`、`line_obstacle_bypass.c`、本分支 `main.c` 或其模式 1 启动限制。要移入尚无本模块的分支，导入这五个文件并添加下面的调用点即可；全量 cherry-pick 初始提交还会带入模式 1 的改动，须明确需要时才做。

头文件支持 C++ 调用，`.c` 文件按 C 编译。导出包不包含 STM32 SDK、启动文件、电机实现或完整工程，不能独立链接成小车固件。

## 最短接入流程

1. 电机、编码器、DriveBase 和系统时钟初始化后，在全车 STOP 下执行一次 `MpuYaw_Init(HAL_GetTick())`。
2. 全局主循环所有模式都执行一次 `MpuYaw_Task(now, stationary)`，再分发当前模式。`stationary` 必须是真正 STOP、零输出且轮速接近零；不要传常量 1。在 STOP 连续采样约 2 秒后用 `MpuYaw_IsReady()` 判断可用。
3. 当前模式取得电机控制权并确认 DriveBase STOP 后，**仅在动作入口**调用 `GyroTurn_Start()`，检查是否返回 1。
4. 每轮继续处理遥控/串口 STOP，服务传感器，调用当前模式的 `GyroTurn_Task()`；RUNNING 阶段直接返回，不再执行循迹、直线或其他电机输出。
5. DONE 时读取实际角度并进入下一阶段；FAULT 时保留故障并停车。不要在 FAULT 分支无条件 ClearFault/Start。
6. 模式切换先调用 `GyroTurn_Stop()`，再交接电机。全局手动 STOP 除取消此模块外，还必须取消其他动作并执行整车 STOP。

已有本分支 `main.c` 的工程已经初始化并服务 MpuYaw，不再添加第二套。`GyroTurn_Task` 由当前动作所有者调用；若改成全局统一调用，需删除各模式内重复调用。读传感器不代表获得电机控制权。

```c
/* 在用户/导航事件触发的动作入口选其中一条；不是连续执行四条。 */
uint8_t accepted = GyroTurn_Start( 90000, 2500); /* 左转 90° */
/* GyroTurn_Start(-90000, 2500);  右转 90° */
/* GyroTurn_Start( 45000, 2000);  左转 45° */
/* GyroTurn_Start(180000, 2500);  左转 180° */
/* accepted == 0 时不得把模式阶段标成“已启动”。 */
```

角度单位为 **毫度**（1000 = 1°），轮速单位为 **编码器计数/秒 CPS**，不是 PWM。合法请求为非零 ±360000mdeg、1412..3600CPS；当前终点窗口为 ±4°，小角度精度和电机制动参数需实测。启动立即发布轮速命令，不是只保存目标。

下面的完整调用示例保存为 `reusable/gyro_angle/examples/angle_mode_example.[ch]`，已参加主机测试。它只演示模式层的启动、周期服务、结果与退出，不绑定遥控按键，也不注册任何新运动模式：

```c
/* 全局循环：先处理 STOP 和模式切换，再服务 MpuYaw。 */
MpuYaw_Task(HAL_GetTick(), actual_stationary);
/* 只对当前选中且已经拿到控制权的示例模式调用： */
AngleModeExample_Task();
/* 检查 AngleModeExample_GetState()；完成后读 GetAngleMdeg()。 */
```

应用必须保证从动作发起到退出之间没有其他模式写 DriveBase。模块是静态单实例，不提供线程锁、owner token 或动作队列。Start 会拒绝运行中的第二个动作，但不能阻止其他模块直接写电机。不要从中断调用这些接口。

## 完成、提前结束、取消

| 调用/状态 | 含义 | 模式处理 |
|---|---|---|
| `Start(...) == 0` | 没启动；可能忙、参数错、底盘未停或传感器不可用 | 保持未启动状态，检查条件；不忽略返回值 |
| `GYRO_TURN_RUNNING` | 包括转动和制动后的稳定等待 | 持续服务，不启动下一动作 |
| `GYRO_TURN_DONE` | 定角度验收通过，或主动提前结束后已稳定 | 取实际转角，再进入下一阶段 |
| `GYRO_TURN_FAULT` | 传感器/驱动/超时/反向/无进展/误差异常 | 停车保留故障，阻止自动恢复抢电机 |
| `GyroTurn_RequestStop()` | 红外/视觉捕获边界后提前结束；最终可返回 DONE | 仅供业务边界，不作手动 STOP |
| `GyroTurn_Stop()` | 取消动作并进入 IDLE，保留故障及最后角度 | 手动 STOP、模式退出；取消不等于任务成功 |
| `GyroTurn_ClearFault()` | 只清转角模块故障；要求底盘 STOP、动作未运行 | 仅显式恢复，不能代替传感器重标定和驱动故障处理 |

`GetAchievedAngleMdeg()` 是最近一次 Task 更新的相对转角；含稳定等待期间的角度，Stop 后保留，新 Start 成功时归零。FAULT 时不保证最后数值覆盖失联后的运动。不要把 `GetState()!=RUNNING` 当成成功。

任何 `MpuYaw_Init()` 都重设全局 yaw 原点和偏置，必须先退出全部角度动作并作废旧的相对快照。已完成标定后，各模式开始转弯只需 Start，无需重新标定或清零 IMU。

## 移植到其他底盘

本车同系列工程可以复用已有 DriveBase。其他工程需要适配这六个函数，并保持 `drive_base.h` 中的状态和单位约定：

- `DriveBase_GetTelemetry()`：至少提供真实 mode/fault_mask；应用判断静止还需实际轮速及输出。
- `DriveBase_SetSideCps(left,right)`：四轮左右同侧命令，正值向前；发布后必须如实报告 SPEED。
- `DriveBase_Task(now)`：轮速和制动服务，不自行启动动作。
- `DriveBase_Stop(COAST/BRAKE)`：取消输出或有限制动；制动结束后报告 STOPPED。
- `DriveBase_SetLineFaultObservation(0,0,0)`：恢复阻断式驱动故障策略。
- `DriveBase_PrepareLineTurnAssist(left,right)`：本车的轮速负载辅助；无此功能的移植可为空实现，但必须保持其他轮速/故障约束。

SDK 提供 `HAL_GetTick()`，单位 ms、uint32 自然回绕。更换 MCU/接线可替换 `mpu6050_bus.c` 的三个 MpuBus 函数；不得重设其他模块正在使用的计时基准。换 IMU 则实现同等 MpuYaw 快照、新鲜度和故障语义，不能套用 MPU6050 寄存器解码。包中 `porting/drive_base.h` 是接口参考，`Core/Inc/drive_base.h` 是同一头文件的测试用副本，导入时不覆盖目标工程已有头文件。

本车默认 Z 朝上、左转正；设置 `MPU6050_YAW_SIGN=-1` 只适用于经核实的 Z 朝下安装，不适用于竖放。算法只积分 Z 轴，没有任意姿态解算或绝对航向纠偏。

## 导出、构建与证据

分支根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/export_gyro_angle_module.ps1`。
脚本从白名单导出五个模块文件、接口参考、示例、说明和独立主机测试到新目录及 ZIP；不读取或复制已有 main.c，不修改任何目标项目。每次创建唯一包，`manifest.json` 含 Git HEAD、是否有未提交变化、各文件 SHA256 和长度；包内字节以哈希为准。有未提交变化时，不能把 HEAD 当成完整包源码标识。

原工程使用 `build_unified_motion.ps1`；包内运行 `tests/gyro_turn/run.cmd` 执行无硬件主机测试，需要 Visual Studio 2022 BuildTools C 工具链。包的示例不自动加入产品模式，须由目标应用明确接入。包内另附两个 `line_bypass_turn` 文件，只为重现既有包装层测试；其他模式接入不需要它们。

算法源 `842fc1f` 的 ARM 构建、FIFO/转角/总线测试及原有三个模式回归已经通过；本次复用层另验证示例的左右调用、DONE、取消和失败不重试。未烧录、未测试硬件通信或实车精度。更换平台、调用顺序或模式所有权后必须重新构建和验证；旧测试结果不证明新接入正确。
