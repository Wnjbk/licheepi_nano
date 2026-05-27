# F1C100S TVD Linux 5.7.1

这个目录是按你的裸机 `F1C100S-TVD` 工程整理出的 Linux 5.7.1 版本移植骨架。

## 目录

- `src/suniv_f1c100s_tvd.c`
  - 外部模块版 V4L2 采集驱动
  - 导出 `/dev/videoX`
  - 当前输出格式固定为 `V4L2_PIX_FMT_NV16`
- `src/Makefile`
  - 外部模块编译入口
- `dts/suniv-f1c100s-tvd.dtsi`
  - 设备树节点片段

## 当前实现范围

- 已移植
  - TVD 时钟/复位获取
  - 寄存器初始化
  - NTSC/PAL 两套配置
  - 帧完成中断
  - vb2 DMA contiguous 缓冲
  - V4L2 capture 接口
  - `VIDIOC_S_STD/G_STD/QUERYSTD`
- 未移植
  - 裸机里的 `DEFE` 上屏链路
  - `PAL_M / PAL_N / SECAM`
  - 亮度/对比度/饱和度/hue 控件
  - 3D comb filter 开关接口

## 为什么输出用 NV16

你的裸机代码实际跑的是：

- `TVD_PL_YUV422`
- 单独 Y 地址 + 单独 C 地址
- DEFE 输入模式是 `UVCOMBINED + INYUV422`

这更接近 Linux 里的 `NV16`，不是 `YUYV`。

## 内核配置

你自己去 menuconfig 开这些，别用脚本改：

- `CONFIG_MODULES`
- `CONFIG_MEDIA_SUPPORT`
- `CONFIG_MEDIA_CAMERA_SUPPORT`
- `CONFIG_VIDEO_DEV`
- `CONFIG_VIDEOBUF2_CORE`
- `CONFIG_VIDEOBUF2_V4L2`
- `CONFIG_VIDEOBUF2_DMA_CONTIG`
- `CONFIG_DMA_SHARED_BUFFER`
- `CONFIG_CMA`
- `CONFIG_DMA_CMA`

如果这些是 `y` 或 `m` 都可以，但外部模块最省事的组合是相关依赖都编进内核。

## 编译

在 Linux 内核源码目录外编译：

```make
cd /path/to/tvd_f1c100s_linux57/src
make KBUILD=/path/to/linux-5.7.1 ARCH=arm CROSS_COMPILE=/path/to/buildroot/output/host/bin/arm-linux-
```

编译成功后会得到：

- `suniv_f1c100s_tvd.ko`

## 设备树

把 `dts/suniv-f1c100s-tvd.dtsi` 的节点并入你的板级 dts。

注意：

- 这里故意没写 `pinctrl`
- 因为 TVIN 是模拟输入，不是普通 GPIO 复用那套写法
- 中断号这里按你裸机工程取 `27`

如果你的内核树已经有 `tvd@1c0b000` 预留节点，就不要重复新建，直接补：

- `compatible`
- `clocks`
- `resets`
- `status = "okay"`

## 加载与测试

加载：

```sh
insmod suniv_f1c100s_tvd.ko
dmesg | tail
```

确认节点：

```sh
ls -l /dev/video*
v4l2-ctl --all -d /dev/video0
```

切 PAL：

```sh
v4l2-ctl -d /dev/video0 --set-standard=pal
```

抓一帧：

```sh
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=720,height=576,pixelformat=NV16 \
  --stream-mmap=3 --stream-count=1 --stream-to=frame.nv16
```

## 重要限制

- 我现在没你的 Linux 内核源码，也没交叉编译器环境
- 所以这里做的是“按 Linux 5.7 API 写到可编译方向”的驱动骨架
- 还没做你内核树上的实编验证

最可能需要你现场微调的地方有三个：

- `devm_clk_get_optional()` / `devm_reset_control_get_optional()` 在你 5.7.1 树里的可用性
- 你的 5.7.1 内核里 `CLK_BUS_TVD / CLK_TVD / RST_BUS_TVD` 的名字是否完全一致
- 中断号 `27` 在你当前树里是否和裸机一致
如果你和之前的 WiFi 模块一样，内核树就在：

```sh
~/LicheePi_Nano/linux
```

那就直接：

```sh
cd ~/LicheePi_Nano/tvd_f1c100s_linux57/src
make KBUILD=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-linux-
```
