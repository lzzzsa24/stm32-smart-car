# 模式 3、4 共用模式 2 循迹

用户要求：模式 3、4 的循迹和转向与模式 2 一致，同时保留标志识别与圆弧导航。
当前主干状态已核对到 `e0df612`，板上记录为综合版 `6894af1`。
本任务延续 `a90ce13` 的独立分支，保留回退点
`rollback/sign-before-mode2-follow-a90ce13`。

## 实际差异与修改

原模式 2 使用 `line_tracking_start_following` 配置、带滤波的位置控制、
1 ms 传感器历史、方向记忆及 `LineRecovery`。模式 3、4 使用
`SimpleLine_StepRoute` 的另一套可见黑线表，再把前进指令当权重缩放，
并只在两侧反转时授权转向助力。这些差异发生在导航之外。

新增 `sign_line_follow` 负责仲裁，普通循迹调用模式 2 的同一配置和
`line_tracking_compute`，最终调用共用的 CPS 输出接口。公共输出中的
电机速度请求与转向助力绑定逻辑只做函数提取，模式 2 仍走原入口。
DriveBase 源码、编码器接口、PI、PWM 限制与电压补偿均未修改。

| 场景 | 控制方式 |
|---|---|
| 无识别减速事件、未由导航接管的普通路段 | 与模式 2 相同的目标、滤波、外侧纠偏和丢线搜索 |
| 四路全黑 / 有一帧识别 | 保留 700 / 500 CPS 前进上限和原保持时间 |
| 两秒识别等待 | 保留原确认、重试与停车优先级 |
| 入口选支、出口对齐和出弧直行 | 原 SignRoute 条件及原导航 CPS 转换保留 |
| 入弧选择期间见错侧或丢线 | 保留选定方向及原陀螺仪半侧搜索范围，搜索速度采用模式 2 |
| 已捕获圆弧的可见黑线 | 公共循迹计算；保留 1200 CPS 前进上限，持续最外侧时保持初始前进枢轴纠偏，禁止升级为原地反转 |
| 圆弧内丢线 | 保留已有陀螺仪搜索范围 |
| 出弧完成或取消导航 | 交还公共循迹，清除此前控制者的搜索历史 |

因此普通路段不再常驻原模式 3、4 的 1200 CPS 限制，而使用模式 2 的速度。
识别减速及导航慢行是有意保留的差异；这不保证实际速度精确达到较低目标。
导航的 0/2200 权重仍转换为原来的 0/1147 CPS，不借这次统一再次提高选支功率。

`LineRecovery` 在丢线时可以直接提交电机目标，所以不能先让它运行、再用
导航覆盖。新仲裁先选择等待、导航、受约束搜索或公共循迹中的一个。
`line_tracking_yield_to_route` 只撤销旧搜索与历史，不额外输出停止或运动命令；
新控制者随后独占输出。模式切换和操作员 STOP 保持权威。

K210 程序、模型、识别阈值、投票、喇叭事件、RGB 关闭和 SignRoute 角度规则
均保留。OLED 标题改为 `M3 LINE` / `M4 LINE`。
SimpleLine 仍用于已有入口与陀螺仪搜索保护；其可见黑线表不再驱动普通循迹。

## 验证

- `tests/sign_line/run.cmd`：完整通过。
- 新增 `test_mode2_follow.c` 链接真实公共循迹、恢复、导航及四轮 DriveBase。
  8000 个模式 3/4 对模式 2 的比较样本，涵盖全部传感器组合、不同采样间隔、
  ISR 历史、时钟回绕及模拟 8.4/7.4/7.0 V；四轮请求转速、控制转速、
  PWM 引脚输出和动作逐样本一致。硬件 GPIO、编码器、电池和时钟由测试替代。
- 实际新仲裁链路测试覆盖左右圆弧、入口选错侧保护、原地反转抑制、
  陀螺仪出口对齐、直行重新见线、迟到方向覆盖公共搜索、暂停压过导航、
  CANCEL 交还循迹、STOP 和时钟回绕。
- `tests/line_recovery/run.cmd`：两种配置及真实驱动/负载/位置/模式 1、2 回归通过。
- `tests/gyro_turn/run.cmd`：陀螺仪、真实驱动、绕障和回归角度测试通过。
- 状态检查及 `git diff --check` 通过。状态检查的本地固件哈希警告仍表示
  主干构建与已烧综合版不同，不是本次部署记录。
- 正式 ARM 构建通过：text/data/bss = 107708/64/18328。
- 分支 HEX SHA256：
  `25E5673E51E49BEF961C1460C5CC448F12434A227E0A6160A082B0835956C3B7`。

这些是源码与电脑回归证据。本次未使用串口、烧录、运行实体车或推送远端。
实际轮胎负载、低速电机死区及出弧稳定性仍需实车验证；不能将模拟 PWM 一致
解释为已经证明实际车速或圆弧通过率一致。

## 集成包

```text
role: 模式 3/4 公共循迹接入，分支实现与本地提交
start_commit: a90ce1391814e39fde078dc07a60e57b7d985306
result_commit: 添加本说明的提交，可用 git log -1 --format=%H -- tests/sign_line/MODE2_FOLLOW_INTEGRATION.md 查询
files_changed: Core/Inc/line_tracking.h, Core/Src/line_tracking.c,
 Core/Inc/sign_line_follow.h, Core/Src/sign_line_follow.c,
 Core/Src/main.c, Core/Src/oled_status.c,
 tests/sign_line/run.cmd, tests/sign_line/check_integration.py,
 tests/sign_line/test_mode2_follow.c, 本说明
verification_completed: 模式 2 输出一致性、新仲裁与圆弧联合测试、完整 sign/line/gyro 回归、ARM 构建、状态与差异检查
not_verified: 烧录、轮胎地面响应、识别成功率、实际出弧成功率
risks_or_assumptions: 普通路段速度跟随模式 2；圆弧及识别仍有专用约束；软件测试不是实测
integration_notes: 只将本提交 cherry-pick 到 6894af1 或其后继综合版，再重新构建；保留独立模式 1 修改
```

此工作分支完整镜像不包含最新模式 1 矩形、红外隔离和超声波修改。
不要用分支整包覆盖综合版。仅提交差异的只读应用检查已在 `6894af1` 通过；
最终集成仍应在综合版重新构建和验证。本任务未修改主干状态或其他任务的工作区。
