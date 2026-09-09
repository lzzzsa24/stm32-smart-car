# 模式 4 陀螺仪切线过弧方案

用户给出的轨迹从入口直线斜切到选定半圆，沿半圆前进，再斜切回出口直线。
本方案只绑定模式 4；模式 3 保持原来的传感器优先圆弧状态机。

分支 `feature/mode4-gyro-tangent-exit` 从当前主干 `e9037c2` 建立，先合并
最新综合候选 `5cb1bd3`（基线合并提交 `2a9604f`），因此保留模式 1 的
25/30 cm 绕障参数、模式 3/4 的慢速档和两秒识别停车。

## 轨迹状态

1. `ARM/PROBE`：三帧确认左右方向并完成两秒观察停车。经过入口横线后，
   只有选定侧先接触黑线才启动转弯。
2. 斜向入弧：使用模式 2 慢速纠偏档，内外侧分别为 1412/2357 CPS，
   两侧车轮均向前。陀螺仪累计到 60°且中心线稳定后进入 `ARC`。
3. `ARC`：转弯方向改为入口方向的反向，继续使用 1412/2357 CPS 前进差速。
   此阶段以入弧顶点为角度原点；累计 165°并且编码器前进至少 150 mm 后，
   进入 `EXIT TURN`。全黑、全白或短时错误侧线不会改成原地旋转。
4. `EXIT TURN`：按入口方向前进差速，直到航向回到入弧前的方向 ±10°。
5. `EXIT LINE`：两侧 1412 CPS 直行，至少前进 80 mm 后接受稳定中心线，
   完成出弧。直线搜索上限为 450 mm/3.5 s；失败撤销导航并交还普通循线，
   不产生停车命令。

陀螺仪无效或阶段角度越界会撤销路线控制。操作员 STOP、模式切换、
编码器闭环、PI、电压补偿、电机方向映射、K210 程序、鸣笛和 RGB 关闭均未改变。
OLED 模式 4 标题改为 `M4 GYRO`，串口状态中的 `P=1` 表示新方案；模式 3 为
`P=0`。

## 验证

- 完整 `sign_line` 回归通过。新增测试覆盖左右镜像轨迹、
入口/圆弧/出口角度边界、错误线型不改向、两侧持续正向轮速、陀螺仪失效撤销、
模式 3 隔离、两秒停车、鸣笛、K210 协议和真实 DriveBase 目标。
- 完整 `line_recovery` 回归通过，包括两种搜索速度、模式 1/2、四轮闭环目标、
  转向助力、绕障和 STOP 所有权。
- 正式 ARM 构建通过：text/data/bss = 109212/64/18336，BIN 109280 字节；
  BIN SHA256 `E8316B5D00609EB80783DF91C9787342104B4698EDC5BEAD78F3FCE7D62A4BCD`，
  HEX SHA256 `C249A2C000B398A3F73ADAD7DDE5CB4C8218A987981FB4E1633363A0869CC0A7`。

这些检查只能证明源代码和主机模拟行为，不能证明实际圆弧半径或赛道通过率。

```text
role: 模式 4 独立陀螺仪切线过弧实验
start_commit: e9037c27a053156a186809102afa56881fa1824c
baseline_commit: 2a9604f
result_commit: git log -1 --format=%H -- tests/sign_line/MODE4_GYRO_TANGENT.md
files_changed: sign_route.[ch]/config、sign_line_follow、line_tracking、main/OLED、sign tests/docs
verification_completed: 完整 sign/line 回归、正式 ARM 构建、差异检查
not_verified: 烧录、离地轮测、实车圆弧半径、赛道出弧
risks_or_assumptions: 2200/2400 PWM 等效目标形成的实际半径受地面、载荷和轮胎差异影响
integration_notes: 保留 2a9604f 基线，只将最终功能提交合入 5cb1bd3 或后继综合版
```

本任务不访问串口、不烧录、不运行实体车、不推送远端。
