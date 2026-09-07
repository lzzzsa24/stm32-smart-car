# 模式 5：K210 曲线视觉寻线 v4

`main.py` 来自用户提供的
`F:\myproject\jidian\line_tracking_curve_v4.zip`，源压缩包 SHA-256 为
`C847B29105AC24EEDBCE71FE54463BC16489B0F9407AE3E9A3F7B70A766856D6`。
它不加载 KPU 模型，也不做路标识别。

K210 使用 RGB565/QVGA 画面识别整条黑线，每 50 ms 通过 UART1 发送：

```text
$status,off,angle,bottom,obs,obs_bottom,obs_left,obs_right#
```

模式 5 由 STM32 遥控数字 `5` 或调试串口字符 `5` 启动。STM32 使用偏移量
做差速寻线，使用角度提前降速，并在接近横线时用底边中心决定原地转向方向。
丢线后按最后方向最多保持 400 ms；UART 超过 150 ms 没有新合法帧便停车。
视觉障碍下边缘达到 150 时先停车，绕障路径尚未经过实车标定，因此本模式不
自动绕行。

K210 IO8/TX 接 STM32 PD6/RX，IO6/RX 接 STM32 PD5/TX，双方共地，串口为
115200、8-N-1。叠加绘制默认关闭，短按 BOOT 键切换。

原模式 3/4 的路标脚本保存在 `sign_mode34.py`，但模式 5 不调用对应 STM32
路标状态机。部署模式 5 时将本目录的 `main.py` 写入 K210 `/sd/main.py`。
