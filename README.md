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

## 当前主干模式（rc.5）

| 按键/遥控数字 | 功能 |
|---|---|
| 0 | 停车 |
| 1 | 原主干循迹＋自适应绕障，保留红外和原超声配置 |
| 2 | 原纯四线循迹 |
| 3 / 4 | 原主干 SL2 循迹＋标志导航（未引入综合测试版的新3/4） |
| 5 | 新固定路线四线绕障：横向25 cm、旁侧30 cm、45°回线，不需要K210 |

模式5来自综合测试版 `58c3744` 的模式1：固定直行/回线目标4000 CPS，
超声基础触发16 cm、动态上限22 cm；侧向红外关闭，不能检测侧边碰撞风险。
这些是轮速/距离目标，不代表实测车体轨迹。旧视觉寻线不再绑定任何模式；
代码和K210历史脚本保留作参考。模式5无需给K210换程序。

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
