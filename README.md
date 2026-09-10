# STM32 实验七统一四轮运动控制

这是 YB-DSF01-V1.1 四轮小车（STM32F103ZETx）的统一固件仓库。仓库同时
保留 STM32、K210、循迹、避障、编码器、陀螺仪、OLED、红外遥控和音频模块。

## 从这里开始

- [PROJECT_STATE.md](PROJECT_STATE.md)：当前主干、候选版本、已烧录版本和实物验证边界的唯一状态入口。
- [AGENTS.md](AGENTS.md)：修改、构建、烧录和多任务协作规则。
- [BRANCH_WORKFLOW.md](BRANCH_WORKFLOW.md)：分支、工作树、PR 和回滚流程。
- [MULTI_MODEL_WORKFLOW.md](MULTI_MODEL_WORKFLOW.md)：不同 Codex 任务之间的交接方式。
- [docs/README.md](docs/README.md)：组件指南、Release、历史候选和部署记录总索引。

不要从某一份旧实验或部署记录推断当前程序。源代码状态以 Git 为准，硬件状态
以 `PROJECT_STATE.md` 中最新、明确标注的记录为准。

## 工程结构

- `Core/Inc`、`Core/Src`：STM32 应用和驱动源码。
- `Drivers`：STM32F1 HAL/CMSIS 依赖。
- `K210`：视觉寻线与路标识别程序。
- `tests`：主机端回归测试及与测试紧邻的设计说明。
- `reusable`：可复用模块示例。
- `docs`：整理后的项目文档。
- `test-exp7-unified-motion-v1.ioc`：CubeMX 配置。
- `build_unified_motion.ps1`：正式固件构建脚本。

`Debug/` 和 `manual-build-*` 是本地生成目录，不进入 Git 历史。需要复现固件时，
应检出对应提交后重新构建，并核对生成文件的 SHA-256。

## 当前主干：四模式（v1.2.0-rc.6）

| 按键/遥控数字 | 功能来源与行为 |
|---|---|
| 0 | 停车；上电默认 STOP |
| 1 | 保留原主干模式1：四线循迹＋自适应绕障、原红外/超声配置 |
| 2 | 保留原主干模式2：纯四线循迹 |
| 3 | 综合测试 a217df8 模式3：四线循迹＋K210标志导航 |
| 4 | 综合测试 a217df8 模式5：快速四线循迹＋固定路线绕障，无需K210 |
| 5 | 不分配模式，按下保持当前模式；停车请按0 |

模式3：20%观察阈值，丢线先以1800 CPS搜索任一中间探头再观察2秒；
有符号出弧角默认40°，遥控独立 `+ / −` 每次调整5°，范围30°～90°，
切换模式保留、断电或复位恢复40°。出弧完成要求对应最外侧先灭后亮；
鸣笛标志播放一遍。OLED显示设定角、IMU标定状态和相对航向。

模式4：普通直行上限3600 CPS，三/四探头黑线3600 CPS；
固定绕障为向右偏移250 mm、平行300 mm、45°回线，固定直行/回线目标4000 CPS，
连续衔接；超声基础触发16 cm、动态上限22 cm，侧红外关闭。
轮速、里程与陀螺仪角度不是实际车体轨迹保证，需验证刹车和侧面余量。

模式1、2的原控制器和硬件映射保留；模式3、4使用隔离的 `promoted_*` 控制器，
避免改变模式1、2。原SL2模式4和视觉模式5不再有入口；历史源码保留。
详见 [四模式发布说明](docs/releases/RELEASE_V1.2.0_RC6.md)。

## K210

本版只有模式3使用K210标志程序。仓库 `K210/sign_mode34.py` 是历史文件名，
部署为 `/sd/main.py`；模型位于 `/sd/KPU/road_sign_det/road_sign_det.kmodel`。
Release附配套脚本、模型及哈希。模式4无需K210，勿部署旧视觉寻线脚本。
本次整合没有操作STM32或K210实物，发布候选版不代表地面测试通过。

## 构建

在 Windows PowerShell 中运行：

```powershell
.\build_unified_motion.ps1
```

输出位于 `manual-build-unified-motion/`。构建成功只证明电脑端编译和链接通过；
烧录/读回、四轮悬空测试和地面测试是相互独立的验证层级。

## 文档放置约定

仓库根目录只保留项目入口和协作规则。新增文档按用途放入：

- `docs/guides/`：仍可复用的组件与接口说明；
- `docs/refactor/`：架构重构计划和验证记录；
- `docs/releases/`：版本发布说明；
- `docs/history/candidates/`：候选版本与合并记录；
- `docs/history/deployments/`：与具体提交绑定的烧录证据；
- `docs/history/experiments/`：已被后续实现取代的实验说明；
- `docs/history/baselines/`：早期基线、备份和悬空验证资料。
