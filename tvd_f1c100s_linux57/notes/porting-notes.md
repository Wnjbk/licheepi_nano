# 裸机到 Linux 的结构映射

## 裸机入口

- `F1C100S-TVD/Project/main.c:73`

裸机主流程：

1. `TvdDefeConfig(mode)`
2. `Sys_LCD_Init(...)`
3. `f1c100s_tvd_Init(0, mode)`

Linux 里只保留第 3 部分，`DEFE + LCD` 不进采集驱动。

## 裸机 TVD 关键函数

- `Driver/Source/sys_tvd.c:81`
  - `BSP_TVD_init`
- `Driver/Source/sys_tvd.c:87`
  - `BSP_TVD_config`
- `Driver/Source/sys_tvd.c:353`
  - `BSP_TVD_irq_enable`
- `Driver/Source/sys_tvd.c:404`
  - `BSP_TVD_capture_on`
- `Driver/Source/sys_tvd.c:420`
  - `BSP_TVD_set_addr_y`
- `Driver/Source/sys_tvd.c:429`
  - `BSP_TVD_set_addr_c`
- `Driver/Source/sys_tvd.c:439`
  - `BSP_TVD_set_fmt`
- `Driver/Source/sys_tvd.c:473`
  - `BSP_TVD_get_status`
- `Driver/Source/sys_tvd.c:651`
  - `f1c100s_clk_init`
- `Driver/Source/sys_tvd.c:677`
  - `f1c100s_tvd_init`

## 裸机中断逻辑

- `Driver/Source/sys_tvd.c:37`

裸机 ISR 做了两件事：

1. TVD 三缓冲轮转
2. 通知 DEFE 切输入缓冲

Linux 版只保留第 1 件事，改成：

1. 当前 vb2 buffer 完成
2. 编程下一块 vb2 buffer

## 裸机 DEFE 逻辑

- `Driver/Source/sys_defe.c:1710`
  - `Defe_Config_video_uvcombined_yuv422_to_argb_x`
- `Driver/Source/sys_defe.c:1785`
  - `Defe_Config`
- `Driver/Source/sys_defe.c:1795`
  - `Defe_conversion_buff`

这部分说明了一件重要事：

- TVD 输出不是 packed `YUYV`
- 而是 `Y + UVcombined`
- 所以 Linux 导出成 `NV16` 最合适

## 设备树判断

这份移植方案默认：

- 不写 TVD pinctrl
- 只写 `reg / interrupts / clocks / resets`

原因不是偷懒，而是你这块 TVIN 输入更像专用模拟前端，不该按普通 GPIO 功能复用去写。
