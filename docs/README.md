# 项目文档索引

本目录按“当前入口、可复用资料、版本记录、历史证据”组织。移动只改变文档位置，
没有删除历史记录，也没有改变固件源码或已烧录硬件。

## 当前权威入口

以下文件因工具和协作流程依赖而保留在仓库根目录：

- [项目总览](../README.md)
- [共享项目状态](../PROJECT_STATE.md)
- [项目操作约定](../AGENTS.md)
- [分支与工作树流程](../BRANCH_WORKFLOW.md)
- [多模型交接流程](../MULTI_MODEL_WORKFLOW.md)

## 分类目录

- [组件与接口指南](guides/README.md)
- [运动控制重构资料](refactor/README.md)
- [Release 说明](releases/README.md)
- [历史资料总览](history/README.md)

## 与源码相邻的说明

- `../tests/line_recovery/`：循迹、丢线恢复与绕障回归说明。
- `../tests/sign_line/`：模式 3/4 路标循迹回归说明。
- [K210 视觉寻线 v4](../K210/MODE5_VISUAL_LINE_V4.md)
- [K210 路标识别优化](../K210/V2_OPTIMIZATION.md)
- [可复用陀螺仪角度模块](../reusable/gyro_angle/README.md)

历史文档只证明其标题所列日期、提交和测试条件下的结果。判断当前代码和实物状态
时，必须回到根目录的 `PROJECT_STATE.md`。
