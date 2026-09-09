# MPU6050 角度控制分支

> 综合整合说明：本文原本只描述 `842fc1 → 74526f4` 的模式1候选。
> `test/comprehensive-v6-gyro-20260908` 已把同一套 MpuYaw 服务同时接入
> 模式1绕障和模式3/4圆环导航；最终组合边界以
> `COMPREHENSIVE_V6_GYRO_CANDIDATE.md` 为准。

## 版本与范围

- 分支：`feature/mpu6050-angle`。
- 工作目录：`F:/myproject/jidian/worktrees/mpu6050-angle`。
- 起点：当前 canonical main `49288f108acddc15e2eae8b5085f6382670fb947`。
- 回滚基线：`rollback/2026-09-08-before-mpu6050-angle`，指向同一起点。
- 完成范围：模式 1 正在使用的 `LineBypassTurn` 绕障定角度动作改用 MPU6050 相对偏航角；四轮轮速、限幅、编码器故障仍由 DriveBase 管理。
- 保留的 `EncoderTurn` 历史圆弧/后轴支点接口，以及循迹丢线搜索、模式 3/4 的路线估计，没有统一替换为陀螺仪。这版不新增遥控运动模式。
- 起点不是车上综合测试 `0a02d3d`：该测试分支独有的十秒识别等待、KEY1/KEY2 共享入口增量未混入本分支。
- 未合并 main、未推送、未修改共享 `PROJECT_STATE.md`、未访问串口、未烧录、未进行悬空或落地测试。

## 硬件依据

用户 2026-09-08 照片中模块插在专用 IMU 位置，外观为元件面朝上的平放安装。
本地 `F:/myproject/jidian/资料/开发板原理图.pdf` 的 U7 和 MCU 网络给出：

| 接口 | STM32 / 电源 |
|---|---|
| SCL | PB10，I2C2_SCL 网络 |
| SDA | PB11，I2C2_SDA 网络 |
| AD0 | PE0，本版初始化拉低，7 位地址 0x68 |
| INT | PE1，本版不用中断脚 |
| VCC/GND | 主板模块接口 5V/GND |

原理图页脚是历史板图，照片不能证明实物所有连线；以上是实现依据，实际通信仍需读 `WHO_AM_I` 验证。驱动默认 Z 朝上、左转为正。若整车安装翻面，须先核实后改 `MPU6050_YAW_SIGN`，不得靠调电机极性补偿。

使用 PB10/PB11 开漏软件 I2C，主板原理图有 4.7k 上拉；不占用 OLED 的 PB6/PB7。复用 DWT 计时但不清零，保留红外接收的计时基准。每次事务有 12ms 总期限；SCL 卡低、SDA 卡低、NACK 均返回错误。实际总线频率受 GPIO 调用耗时影响，需上机测量，不声称达到某一硬件 I2C 标称速率。

寄存器配置依据：[MPU6050 原厂寄存器资料入口](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)（本次入口重定向无法完整读取），以及可读取的 [Adafruit 官方驱动寄存器/量程定义](https://github.com/adafruit/Adafruit_MPU6050/blob/master/Adafruit_MPU6050.h)。实现没有引入这些库的运行依赖。

## 采样和标定

- 非阻塞阶段等待：复位后等待 100ms、唤醒后等待 100ms；有限 I2C 事务仍会占用主循环。
- 校验设备 ID，并回读时钟、采样率、滤波、量程和 FIFO 配置。
- 100Hz、44Hz 数字低通、陀螺仪 ±500°/s、加速度 ±2g。
- FIFO 保存 AX/AY/AZ/GX/GY/GZ，每帧 12 字节；按每帧 10ms 积分，不用主循环间隔积分。
- 每轮最多取 8 帧；FIFO 溢出或超出处理上限会故障，不跳过历史后继续转弯。半帧暂留，不刷新采样时间；无完整样本超过 30ms 锁存故障，主循环停顿超过 80ms 也拒绝续算。
- 200 个连续静止样本约 2 秒完成零偏标定。要求 STOP、零电机输出、轮速接近零、重力方向正确、三轴角速度和波动在阈值内；运动或明显倾斜清空当前标定窗口。10 秒未完成则故障。
- 每次上电或停车 `c` 重新标定；不写校准 Flash 页。运行中不自动修改零偏。
- MPU6050 没有本版可用的绝对航向基准，偏航来自积分，存在温漂和累积漂移。本版仅测短时间转弯相对角，不承诺长期绝对航向。

## 转弯行为

`GyroTurn_Start(angle_mdeg, maximum_cps)` 中正角度左转、负角度右转。模块必须在 STOP、无驱动故障、陀螺仪 READY 且样本新鲜时启动。接口允许非零 ±360000mdeg、1412..3600CPS；本车绕障通常请求 15° 等短段，极小角度没有精度保证。

记录每次动作起始 yaw，使用实测角度差作为终点。距离目标超过 20° 时使用请求轮速，接近目标逐步降到当前底盘连续转动下限 1412CPS。制动预测采用当前角速度乘 25ms，限于 0.5..4°。停止后必须检测低角速度及 DriveBase STOP 连续 120ms，最多等待 700ms；定角度动作最终误差超出 4° 报故障，不伪报到位。4° 是软件验收窗口，不是实车精度结论。

红外边界仍可提前结束绕障转弯，此时按实测转角返回，不要求达到原定角度。电机无故障但车身 1.2 秒没有至少 1° 新进展、反向偏航超过 3°、总动作超时、样本失效、外部接管或驱动故障都会退出。没有自动反复倒向修正。

转角故障保留到显式停车重标定。主循环在自动等待恢复前把模式 1 的传感器/角度故障转换为 STOP，避免旧的 800ms 等待恢复重新发出转动命令。模式 2..5 不以 MPU6050 READY 为启动条件。

`MPU6050_BYPASS_ENABLED=0` 仅供编译时选择历史编码器端点进行对比；默认是 1。运行中不会在陀螺仪掉线后偷偷切回编码器角度。直线绕障段仍按编码器距离结束；当前净转角是各转弯段的实测角度累加，尚未补偿直线段中的偏航。

## 用户后续验证

本次仅交付源码和编译产物。用户自行部署该确切分支之后：

1. 平放整车并保持静止，等待启动和标定完成。上电始终 STOP。
2. 在 115200 8N1 发送 `g`，停车时输出：`IMU S=2 F=0 CAL=200 ...`；`S=0/1/2/3` 依次为初始化、标定、可用、故障。
3. 保持停车，手动左转车身约 90°，再发 `g`：`YAW_MDEG` 应增加约 90000；右转应减小。该显示按一圈取余，过零/一圈时需考虑取余。先确认方向和量级，再尝试电机动作。
4. 停稳后发 `c`，等待约 2 秒，再发 `g` 确认 READY。运动中的 `c` 会被拒绝。`g` 在运动中只登记请求，到 STOP 才输出。
5. 用户控制模式 1 的绕障测试，并保留遥控 `0`。记录实际角度、`TURN_MDEG`、电压、地面和是否侧滑；先测试小段，再测试连续绕障。悬空轮转不证明车身偏航。

`F` 传感器故障：1 总线/配置回读，2 ID，3 FIFO，4 样本过期，5 标定未完成，6 量程饱和。
`TURN_F`：1 传感器不可用，2 驱动故障/外部接管，3 超时/无法静止，4 反向，5 无角度进展，6 停稳后误差超限。

## 构建和验证证据

在分支根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_unified_motion.ps1
cmd /c tests\gyro_turn\run.cmd
cmd /c tests\line_recovery\run.cmd
cmd /c tests\sign_line\run.cmd
cmd /c tests\vision_line_v4\run.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check_project_state.ps1
git diff --check
```

本次以上构建和四组主机测试均通过。gyro_turn 测试使用真实 FIFO 解码、标定、积分、转弯和绕障包装代码，模拟总线及 DriveBase；另外独立测试真实 GPIO 总线代码的地址/寄存器位序、重复起始、ACK/NACK、最长读取、卡线超时及 DWT 回绕。未把模拟 DriveBase 当成真实车轮验证。

原 line_recovery 套件的底盘/旧绕障部分显式以 `MPU6050_BYPASS_ENABLED=0` 验证历史路径；新的默认陀螺仪端点由 gyro_turn 套件验证。循迹两档速度、模式 3/4 以及模式 5 回归通过。未进行完整传感器-底盘硬件闭环测试。

最终 ARM text/data/bss：`88680/64/11696` 字节。

- BIN：`manual-build-unified-motion/exp7_unified_motion.bin`，88748 字节。
- BIN SHA256：`E35A5DEE664F2475A284506BF47AC4067B306B4A5CD0D6C202FB4866ED76E007`。
- HEX：`manual-build-unified-motion/exp7_unified_motion.hex`。
- HEX SHA256：`5669B6A64795246C106A7ED7A8A62A8A88FA7F281CA6A2A9E3A293E643786090`。

构建产物是忽略文件，换电脑应从提交重新构建。烧录、读回、悬空、落地证据均为未执行。

## 完成包

```text
role: independent feature implementation
start_commit: 49288f108acddc15e2eae8b5085f6382670fb947
result_commit: resolve with git rev-parse feature/mpu6050-angle
files_changed: mpu6050_yaw.[ch], mpu6050_bus.c, gyro_turn.[ch], line_bypass_turn.[ch], main.c, tests/gyro_turn/*, tests/line_recovery/run.cmd, this document
verification_completed: ARM build, four host suites, state checker, diff whitespace check
not_verified: hardware I2C, physical orientation/readings, wheel or ground behavior, braking calibration
risks_or_assumptions: dedicated socket matches schematic, Z up, fixed sensor sample period, bias drift, candidate speed/braking/tolerance settings
integration_notes: based on current main, independent branch only; do not overwrite flashed comprehensive source or publish this as integrated state without deliberate integration
```
