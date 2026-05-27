#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>

#define SUNIV_TVD_NAME                   "suniv-f1c100s-tvd"

#define SUNIV_TVD_REG_CTRL               0x0000
#define SUNIV_TVD_REG_SYSTEM             0x0008
#define SUNIV_TVD_REG_LUMA               0x000c
#define SUNIV_TVD_REG_CHROMA             0x0010
#define SUNIV_TVD_REG_COMB_LINE          0x0014
#define SUNIV_TVD_REG_DTO                0x0018
#define SUNIV_TVD_REG_ACT_START          0x001c
#define SUNIV_TVD_REG_CLAMP_AGC          0x0040
#define SUNIV_TVD_REG_3D_COMB_CTRL       0x0048
#define SUNIV_TVD_REG_3D_COMB_Y          0x004c
#define SUNIV_TVD_REG_3D_COMB_C          0x0050
#define SUNIV_TVD_REG_3D_COMB_SIZE       0x0054
#define SUNIV_TVD_REG_ADDR_Y             0x0080
#define SUNIV_TVD_REG_ADDR_C             0x0084
#define SUNIV_TVD_REG_CAPTURE            0x0088
#define SUNIV_TVD_REG_SIZE               0x008c
#define SUNIV_TVD_REG_WIDTH_JUMP         0x0090
#define SUNIV_TVD_REG_INT_STATUS         0x0094
#define SUNIV_TVD_REG_INT_ENABLE         0x009c
#define SUNIV_TVD_REG_INPUT_SELECT       0x0e04
#define SUNIV_TVD_REG_ANALOG             0x0e2c
#define SUNIV_TVD_REG_STATUS             0x0e40
#define SUNIV_TVD_REG_YCFG1              0x0f08
#define SUNIV_TVD_REG_YCFG2              0x0f0c
#define SUNIV_TVD_REG_SYNC               0x0f10
#define SUNIV_TVD_REG_BLUE_SCREEN        0x0f14
#define SUNIV_TVD_REG_CHROMA_AGC         0x0f1c
#define SUNIV_TVD_REG_AGC_KILL           0x0f24
#define SUNIV_TVD_REG_AGC_BACKPORCH      0x0f28
#define SUNIV_TVD_REG_KILL_LEVEL         0x0f2c
#define SUNIV_TVD_REG_BURST_GATE         0x0f44
#define SUNIV_TVD_REG_CLAMP              0x0f4c
#define SUNIV_TVD_REG_Y_DELAY            0x0f54
#define SUNIV_TVD_REG_C_DELAY            0x0f58
#define SUNIV_TVD_REG_YC_SEP1            0x0f6c
#define SUNIV_TVD_REG_YC_SEP2            0x0f70
#define SUNIV_TVD_REG_CHROMA_EDGE        0x0f74
#define SUNIV_TVD_REG_HACTIVE            0x0f80
#define SUNIV_TVD_REG_VACTIVE            0x0f84

#define SUNIV_TVD_INT_FRAME_DONE         BIT(24)
#define SUNIV_TVD_CAPTURE_EN             BIT(0)
#define SUNIV_TVD_CAPTURE_ADDR_VALID     BIT(28)
#define SUNIV_TVD_FMT_MB_YUV420          BIT(24)
#define SUNIV_TVD_FMT_PL_YUV422          BIT(4)

#define SUNIV_TVD_MAX_WIDTH              720U
#define SUNIV_TVD_MAX_HEIGHT             576U
#define SUNIV_TVD_MAX_FRAME_SIZE         (SUNIV_TVD_MAX_WIDTH * SUNIV_TVD_MAX_HEIGHT * 2U)

#define SUNIV_CCU_BASE                   0x01c20000
#define SUNIV_CCU_SIZE                   0x400
#define SUNIV_CCU_PLL_VIDEO              0x0010
#define SUNIV_CCU_DRAM_GATING            0x0100
#define SUNIV_CCU_TVD_CLK                0x0124
#define SUNIV_CCU_DRAM_GATING_TVD        BIT(3)
#define SUNIV_CCU_PLL_VIDEO_270MHZ       (BIT(31) | BIT(28) | 0x00006200)
#define SUNIV_CCU_TVD_CLK_27MHZ          (BIT(31) | 0x00000009)

enum suniv_tvd_std {
	SUNIV_TVD_STD_NTSC = 0,
	SUNIV_TVD_STD_PAL  = 1,
};

struct suniv_tvd_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

struct suniv_tvd_dev {
	struct device *dev;
	void __iomem *base;
	void __iomem *ccu_base;
	int irq;

	struct clk *clk_bus;
	struct clk *clk_mod;
	struct clk *clk_ram;
	struct reset_control *rst_bus;

	struct mutex lock;
	spinlock_t qlock;
	struct list_head queued_bufs;
	struct suniv_tvd_buffer *active;

	struct vb2_queue vb_queue;
	struct video_device vdev;
	struct v4l2_device v4l2_dev;

	enum suniv_tvd_std std;
	unsigned int width;
	unsigned int height;
	unsigned int field_height;
	unsigned int input_channel;
	unsigned int sequence;
	unsigned int blue_screen_mode;
	bool auto_detect;
	bool streaming;
};

static inline u32 suniv_tvd_read(struct suniv_tvd_dev *tvd, u32 reg)
{
	return readl(tvd->base + reg);
}

static inline void suniv_tvd_write(struct suniv_tvd_dev *tvd, u32 reg, u32 value)
{
	writel(value, tvd->base + reg);
}

static inline void suniv_tvd_update_bits(struct suniv_tvd_dev *tvd, u32 reg,
					 u32 mask, u32 value)
{
	u32 tmp;

	tmp = suniv_tvd_read(tvd, reg);
	tmp &= ~mask;
	tmp |= value & mask;
	suniv_tvd_write(tvd, reg, tmp);
}

static inline unsigned int suniv_tvd_payload_size(struct suniv_tvd_dev *tvd)
{
	return tvd->width * tvd->height * 2U;
}

static inline struct suniv_tvd_buffer *
suniv_tvd_next_buffer_locked(struct suniv_tvd_dev *tvd)
{
	struct suniv_tvd_buffer *buf;

	if (list_empty(&tvd->queued_bufs))
		return NULL;

	buf = list_first_entry(&tvd->queued_bufs, struct suniv_tvd_buffer, list);
	list_del(&buf->list);
	return buf;
}

static void suniv_tvd_set_blue_screen_mode(struct suniv_tvd_dev *tvd, unsigned int mode)
{
	mode &= 0x3;
	suniv_tvd_update_bits(tvd, SUNIV_TVD_REG_BLUE_SCREEN, 0x3 << 4, mode << 4);
}

static void suniv_tvd_enable_dram_gate(struct suniv_tvd_dev *tvd)
{
	u32 value;

	if (!tvd->ccu_base)
		return;

	value = readl(tvd->ccu_base + SUNIV_CCU_DRAM_GATING);
	if (value & SUNIV_CCU_DRAM_GATING_TVD)
		return;

	writel(value | SUNIV_CCU_DRAM_GATING_TVD,
	       tvd->ccu_base + SUNIV_CCU_DRAM_GATING);
	dev_info(tvd->dev, "enabled TVD DRAM gate, dram_gating=%08x\n",
		 readl(tvd->ccu_base + SUNIV_CCU_DRAM_GATING));
}

static void suniv_tvd_enable_clock_registers(struct suniv_tvd_dev *tvd)
{
	u32 pll_video;
	u32 tvd_clk;

	if (!tvd->ccu_base)
		return;

	pll_video = readl(tvd->ccu_base + SUNIV_CCU_PLL_VIDEO);
	tvd_clk = readl(tvd->ccu_base + SUNIV_CCU_TVD_CLK);
	writel(SUNIV_CCU_PLL_VIDEO_270MHZ, tvd->ccu_base + SUNIV_CCU_PLL_VIDEO);
	writel(SUNIV_CCU_TVD_CLK_27MHZ, tvd->ccu_base + SUNIV_CCU_TVD_CLK);

	dev_info(tvd->dev,
		 "forced TVD clock: pll_video %08x->%08x tvd_clk %08x->%08x\n",
		 pll_video, readl(tvd->ccu_base + SUNIV_CCU_PLL_VIDEO),
		 tvd_clk, readl(tvd->ccu_base + SUNIV_CCU_TVD_CLK));
}

static void suniv_tvd_input_select(struct suniv_tvd_dev *tvd, unsigned int input)
{
	if (input)
		suniv_tvd_update_bits(tvd, SUNIV_TVD_REG_INPUT_SELECT, BIT(0), BIT(0));
	else
		suniv_tvd_update_bits(tvd, SUNIV_TVD_REG_INPUT_SELECT, BIT(0), 0);
}

static void suniv_tvd_program_buffer(struct suniv_tvd_dev *tvd, dma_addr_t dma)
{
	dma_addr_t chroma = dma + (tvd->width * tvd->height);

	suniv_tvd_write(tvd, SUNIV_TVD_REG_ADDR_Y, dma);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_ADDR_C, chroma);
	suniv_tvd_update_bits(tvd, SUNIV_TVD_REG_CAPTURE,
			      SUNIV_TVD_CAPTURE_ADDR_VALID,
			      SUNIV_TVD_CAPTURE_ADDR_VALID);
}

static void suniv_tvd_capture_on(struct suniv_tvd_dev *tvd)
{
	suniv_tvd_update_bits(tvd, SUNIV_TVD_REG_CAPTURE,
			      SUNIV_TVD_CAPTURE_EN,
			      SUNIV_TVD_CAPTURE_EN);
}

static void suniv_tvd_capture_off(struct suniv_tvd_dev *tvd)
{
	suniv_tvd_update_bits(tvd, SUNIV_TVD_REG_CAPTURE,
			      SUNIV_TVD_CAPTURE_EN, 0);
}

static void suniv_tvd_set_nv16_format(struct suniv_tvd_dev *tvd)
{
	u32 value;

	value = suniv_tvd_read(tvd, SUNIV_TVD_REG_CAPTURE);
	value &= ~SUNIV_TVD_FMT_MB_YUV420;
	value |= SUNIV_TVD_FMT_PL_YUV422;
	suniv_tvd_write(tvd, SUNIV_TVD_REG_CAPTURE, value);
}

static void suniv_tvd_set_size(struct suniv_tvd_dev *tvd,
			       unsigned int width, unsigned int height)
{
	u32 value;

	tvd->width = width;
	tvd->height = height;
	tvd->field_height = height / 2U;

	value = width & 0xfff;
	value |= (tvd->field_height & 0x7ff) << 16;
	suniv_tvd_write(tvd, SUNIV_TVD_REG_SIZE, value);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_WIDTH_JUMP, width);
}

static int suniv_tvd_detect_signal(struct suniv_tvd_dev *tvd,
				   enum suniv_tvd_std *detected)
{
	u32 status = suniv_tvd_read(tvd, SUNIV_TVD_REG_STATUS);

	if (status & BIT(0))
		return -ENOLINK;

	if (detected)
		*detected = (status & BIT(18)) ? SUNIV_TVD_STD_PAL : SUNIV_TVD_STD_NTSC;

	return 0;
}

static void suniv_tvd_hw_common_init(struct suniv_tvd_dev *tvd)
{
	suniv_tvd_write(tvd, SUNIV_TVD_REG_CAPTURE, 0x04000000);

	suniv_tvd_write(tvd, SUNIV_TVD_REG_INPUT_SELECT, 0x8002AAA8);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_ANALOG, 0x00110000);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_CLAMP_AGC, 0x04000310);

	suniv_tvd_write(tvd, SUNIV_TVD_REG_CTRL, 0x00000000);
	msleep(1000);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_CTRL, 0x00001f00);

	suniv_tvd_write(tvd, SUNIV_TVD_REG_COMB_LINE, 0x20000000);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_AGC_KILL, 0x0682810a);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_AGC_BACKPORCH, 0x00006440);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_CLAMP, 0x0e70106c);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_Y_DELAY, 0x00000000);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_C_DELAY, 0x00000082);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_YC_SEP1, 0x00fffad0);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_YC_SEP2, 0x0000a010);

	suniv_tvd_input_select(tvd, tvd->input_channel);
}

static void suniv_tvd_apply_standard(struct suniv_tvd_dev *tvd, enum suniv_tvd_std std)
{
	suniv_tvd_hw_common_init(tvd);

	if (std == SUNIV_TVD_STD_PAL) {
		suniv_tvd_write(tvd, SUNIV_TVD_REG_SYSTEM, 0x01101001);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_LUMA, 0x00202068);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CHROMA, 0x00300050);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_DTO, 0x2a098acb);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_ACT_START, 0x0087002a);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_YCFG1, 0x11590902);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_YCFG2, 0x00000016);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_SYNC, 0x008a32ec);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_BLUE_SCREEN, 0x800000a0);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CHROMA_AGC, 0x00930000);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_KILL_LEVEL, 0x00000d74);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_BURST_GATE, 0x0000412d);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CHROMA_EDGE, 0x00000343);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_HACTIVE, 0x00500000);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_VACTIVE, 0x00c10000);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CTRL, 0x00000001);
		suniv_tvd_set_size(tvd, 720, 576);
	} else {
		suniv_tvd_write(tvd, SUNIV_TVD_REG_SYSTEM, 0x00010001);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_LUMA, 0x00202068);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CHROMA, 0x00300080);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_DTO, 0x21f07c1f);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_ACT_START, 0x00820022);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_YCFG1, 0x00590100);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_YCFG2, 0x00000010);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_SYNC, 0x008A32DD);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_BLUE_SCREEN, 0x800000a0);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CHROMA_AGC, 0x008A0000);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_KILL_LEVEL, 0x0000CB74);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_BURST_GATE, 0x00004632);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CHROMA_EDGE, 0x000003c3);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_HACTIVE, 0x00500000);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_VACTIVE, 0x00610000);
		suniv_tvd_write(tvd, SUNIV_TVD_REG_CTRL, 0x00000001);
		suniv_tvd_set_size(tvd, 720, 480);
	}

	tvd->std = std;
	suniv_tvd_set_nv16_format(tvd);
	suniv_tvd_set_blue_screen_mode(tvd, tvd->blue_screen_mode);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_STATUS, SUNIV_TVD_INT_FRAME_DONE);
}

static int suniv_tvd_enable_clocks(struct suniv_tvd_dev *tvd)
{
	int ret;

	ret = clk_prepare_enable(tvd->clk_bus);
	if (ret)
		return ret;

	ret = clk_set_rate(tvd->clk_mod, 27000000);
	if (ret && ret != -ENOTSUPP)
		goto err_bus;

	ret = clk_prepare_enable(tvd->clk_mod);
	if (ret)
		goto err_bus;

	if (tvd->clk_ram) {
		ret = clk_prepare_enable(tvd->clk_ram);
		if (ret)
			goto err_mod;
	}

	if (tvd->rst_bus) {
		ret = reset_control_deassert(tvd->rst_bus);
		if (ret)
			goto err_ram;
	}

	dev_info(tvd->dev, "clock rates: bus=%lu mod=%lu ram=%lu\n",
		 clk_get_rate(tvd->clk_bus), clk_get_rate(tvd->clk_mod),
		 tvd->clk_ram ? clk_get_rate(tvd->clk_ram) : 0UL);

	return 0;

err_ram:
	if (tvd->clk_ram)
		clk_disable_unprepare(tvd->clk_ram);
err_mod:
	clk_disable_unprepare(tvd->clk_mod);
err_bus:
	clk_disable_unprepare(tvd->clk_bus);
	return ret;
}

static void suniv_tvd_disable_clocks(struct suniv_tvd_dev *tvd)
{
	if (tvd->rst_bus)
		reset_control_assert(tvd->rst_bus);

	if (tvd->clk_ram)
		clk_disable_unprepare(tvd->clk_ram);

	clk_disable_unprepare(tvd->clk_mod);
	clk_disable_unprepare(tvd->clk_bus);
}

static void suniv_tvd_return_all_buffers(struct suniv_tvd_dev *tvd,
					 enum vb2_buffer_state state)
{
	struct suniv_tvd_buffer *buf;
	struct suniv_tvd_buffer *tmp;
	LIST_HEAD(done);
	unsigned long flags;

	spin_lock_irqsave(&tvd->qlock, flags);

	if (tvd->active) {
		list_add_tail(&tvd->active->list, &done);
		tvd->active = NULL;
	}

	list_splice_init(&tvd->queued_bufs, &done);
	spin_unlock_irqrestore(&tvd->qlock, flags);

	list_for_each_entry_safe(buf, tmp, &done, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
}

static irqreturn_t suniv_tvd_irq(int irq, void *data)
{
	struct suniv_tvd_dev *tvd = data;
	struct suniv_tvd_buffer *done;
	struct suniv_tvd_buffer *next;
	unsigned long flags;
	u32 status;

	status = suniv_tvd_read(tvd, SUNIV_TVD_REG_INT_STATUS);
	if (!(status & SUNIV_TVD_INT_FRAME_DONE))
		return IRQ_NONE;

	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_STATUS, SUNIV_TVD_INT_FRAME_DONE);

	spin_lock_irqsave(&tvd->qlock, flags);

	done = tvd->active;
	next = suniv_tvd_next_buffer_locked(tvd);
	tvd->active = next;

	if (next) {
		dma_addr_t dma = vb2_dma_contig_plane_dma_addr(&next->vb.vb2_buf, 0);

		suniv_tvd_program_buffer(tvd, dma);
		suniv_tvd_capture_on(tvd);
	} else {
		suniv_tvd_capture_off(tvd);
	}

	spin_unlock_irqrestore(&tvd->qlock, flags);

	if (done) {
		done->vb.sequence = tvd->sequence++;
		done->vb.field = V4L2_FIELD_INTERLACED;
		done->vb.vb2_buf.timestamp = ktime_get_ns();
		vb2_set_plane_payload(&done->vb.vb2_buf, 0, suniv_tvd_payload_size(tvd));
		vb2_buffer_done(&done->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}

	return IRQ_HANDLED;
}

static int suniv_tvd_queue_setup(struct vb2_queue *vq,
				 unsigned int *num_buffers,
				 unsigned int *num_planes,
				 unsigned int sizes[],
				 struct device *alloc_devs[])
{
	struct suniv_tvd_dev *tvd = vb2_get_drv_priv(vq);

	if (*num_planes) {
		if (sizes[0] < SUNIV_TVD_MAX_FRAME_SIZE)
			return -EINVAL;
		return 0;
	}

	*num_planes = 1;
	sizes[0] = SUNIV_TVD_MAX_FRAME_SIZE;
	alloc_devs[0] = tvd->dev;

	if (*num_buffers < 3)
		*num_buffers = 3;

	return 0;
}

static int suniv_tvd_buf_prepare(struct vb2_buffer *vb)
{
	if (vb2_plane_size(vb, 0) < SUNIV_TVD_MAX_FRAME_SIZE)
		return -EINVAL;

	vb2_set_plane_payload(vb, 0, 0);
	return 0;
}

static void suniv_tvd_buf_queue(struct vb2_buffer *vb)
{
	struct suniv_tvd_dev *tvd = vb2_get_drv_priv(vb->vb2_queue);
	struct suniv_tvd_buffer *buf;
	unsigned long flags;

	buf = container_of(to_vb2_v4l2_buffer(vb), struct suniv_tvd_buffer, vb);

	spin_lock_irqsave(&tvd->qlock, flags);
	list_add_tail(&buf->list, &tvd->queued_bufs);

	if (tvd->streaming && !tvd->active) {
		struct suniv_tvd_buffer *next;
		dma_addr_t dma;

		next = suniv_tvd_next_buffer_locked(tvd);
		tvd->active = next;
		if (next) {
			dma = vb2_dma_contig_plane_dma_addr(&next->vb.vb2_buf, 0);
			suniv_tvd_program_buffer(tvd, dma);
			suniv_tvd_capture_on(tvd);
		}
	}

	spin_unlock_irqrestore(&tvd->qlock, flags);
}

static int suniv_tvd_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct suniv_tvd_dev *tvd = vb2_get_drv_priv(vq);
	struct suniv_tvd_buffer *first;
	unsigned long flags;
	dma_addr_t dma;
	enum suniv_tvd_std detected;

	suniv_tvd_enable_dram_gate(tvd);

	if (tvd->auto_detect && !suniv_tvd_detect_signal(tvd, &detected) &&
	    detected != tvd->std)
		suniv_tvd_apply_standard(tvd, detected);

	spin_lock_irqsave(&tvd->qlock, flags);
	first = suniv_tvd_next_buffer_locked(tvd);
	tvd->active = first;
	tvd->sequence = 0;
	tvd->streaming = true;
	spin_unlock_irqrestore(&tvd->qlock, flags);

	if (!first) {
		tvd->streaming = false;
		return -EIO;
	}

	dma = vb2_dma_contig_plane_dma_addr(&first->vb.vb2_buf, 0);

	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_STATUS, SUNIV_TVD_INT_FRAME_DONE);
	suniv_tvd_program_buffer(tvd, dma);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_ENABLE, SUNIV_TVD_INT_FRAME_DONE);
	suniv_tvd_capture_on(tvd);

	dev_info(tvd->dev,
		 "stream start: status=%08x capture=%08x int_en=%08x int_st=%08x size=%08x y=%08x c=%08x\n",
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_STATUS),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_CAPTURE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_INT_ENABLE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_INT_STATUS),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_SIZE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_ADDR_Y),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_ADDR_C));

	return 0;
}

static void suniv_tvd_stop_streaming(struct vb2_queue *vq)
{
	struct suniv_tvd_dev *tvd = vb2_get_drv_priv(vq);

	tvd->streaming = false;
	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_ENABLE, 0);
	suniv_tvd_capture_off(tvd);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_STATUS, SUNIV_TVD_INT_FRAME_DONE);
	suniv_tvd_return_all_buffers(tvd, VB2_BUF_STATE_ERROR);
}

static const struct vb2_ops suniv_tvd_vb2_ops = {
	.queue_setup     = suniv_tvd_queue_setup,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
	.buf_prepare     = suniv_tvd_buf_prepare,
	.buf_queue       = suniv_tvd_buf_queue,
	.start_streaming = suniv_tvd_start_streaming,
	.stop_streaming  = suniv_tvd_stop_streaming,
};

static int suniv_tvd_querycap(struct file *file, void *priv,
			      struct v4l2_capability *cap)
{
	strscpy(cap->driver, SUNIV_TVD_NAME, sizeof(cap->driver));
	strscpy(cap->card, "Allwinner F1C100S TVD", sizeof(cap->card));
	strscpy(cap->bus_info, "platform:suniv-f1c100s-tvd", sizeof(cap->bus_info));
	return 0;
}

static int suniv_tvd_enum_input(struct file *file, void *priv,
				struct v4l2_input *input)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);

	if (input->index)
		return -EINVAL;

	strscpy(input->name, "CVBS", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;
	input->std = V4L2_STD_NTSC | V4L2_STD_PAL;
	input->capabilities = V4L2_IN_CAP_STD;
	input->status = 0;

	if (suniv_tvd_detect_signal(tvd, NULL))
		input->status |= V4L2_IN_ST_NO_SIGNAL;

	return 0;
}

static int suniv_tvd_g_input(struct file *file, void *priv, unsigned int *index)
{
	*index = 0;
	return 0;
}

static int suniv_tvd_s_input(struct file *file, void *priv, unsigned int index)
{
	if (index)
		return -EINVAL;

	return 0;
}

static int suniv_tvd_querystd(struct file *file, void *priv, v4l2_std_id *std_id)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);
	enum suniv_tvd_std detected;

	if (suniv_tvd_detect_signal(tvd, &detected))
		return -ENOLINK;

	*std_id = (detected == SUNIV_TVD_STD_PAL) ? V4L2_STD_PAL : V4L2_STD_NTSC;
	return 0;
}

static int suniv_tvd_g_std(struct file *file, void *priv, v4l2_std_id *std_id)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);

	*std_id = (tvd->std == SUNIV_TVD_STD_PAL) ? V4L2_STD_PAL : V4L2_STD_NTSC;
	return 0;
}

static int suniv_tvd_log_status(struct file *file, void *priv)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);
	u32 status = suniv_tvd_read(tvd, SUNIV_TVD_REG_STATUS);

	dev_info(tvd->dev,
		 "status dump: status=%08x ctrl=%08x system=%08x dto=%08x act=%08x capture=%08x size=%08x width_jump=%08x int_en=%08x int_st=%08x hactive=%08x vactive=%08x pll_video=%08x tvd_clk=%08x dram=%08x\n",
		 status,
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_CTRL),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_SYSTEM),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_DTO),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_ACT_START),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_CAPTURE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_SIZE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_WIDTH_JUMP),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_INT_ENABLE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_INT_STATUS),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_HACTIVE),
		 suniv_tvd_read(tvd, SUNIV_TVD_REG_VACTIVE),
		 tvd->ccu_base ? readl(tvd->ccu_base + SUNIV_CCU_PLL_VIDEO) : 0,
		 tvd->ccu_base ? readl(tvd->ccu_base + SUNIV_CCU_TVD_CLK) : 0,
		 tvd->ccu_base ? readl(tvd->ccu_base + SUNIV_CCU_DRAM_GATING) : 0);
	dev_info(tvd->dev, "status bits: no_signal=%u pal=%u raw=%08x\n",
		 !!(status & BIT(0)), !!(status & BIT(18)), status);

	return 0;
}

static int suniv_tvd_s_std(struct file *file, void *priv, v4l2_std_id std_id)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);
	enum suniv_tvd_std target;

	if (vb2_is_busy(&tvd->vb_queue))
		return -EBUSY;

	if (std_id & V4L2_STD_625_50)
		target = SUNIV_TVD_STD_PAL;
	else if (std_id & V4L2_STD_525_60)
		target = SUNIV_TVD_STD_NTSC;
	else
		return -EINVAL;

	suniv_tvd_apply_standard(tvd, target);
	return 0;
}

static void suniv_tvd_fill_pix_format(struct suniv_tvd_dev *tvd,
				      struct v4l2_pix_format *pix)
{
	memset(pix, 0, sizeof(*pix));
	pix->width = tvd->width;
	pix->height = tvd->height;
	pix->pixelformat = V4L2_PIX_FMT_NV16;
	pix->field = V4L2_FIELD_INTERLACED;
	pix->bytesperline = tvd->width;
	pix->sizeimage = suniv_tvd_payload_size(tvd);
	pix->colorspace = V4L2_COLORSPACE_SMPTE170M;
	pix->ycbcr_enc = V4L2_YCBCR_ENC_601;
	pix->quantization = V4L2_QUANTIZATION_LIM_RANGE;
	pix->xfer_func = V4L2_XFER_FUNC_DEFAULT;
}

static int suniv_tvd_enum_fmt_vid_cap(struct file *file, void *priv,
				      struct v4l2_fmtdesc *f)
{
	if (f->index)
		return -EINVAL;

	f->pixelformat = V4L2_PIX_FMT_NV16;
	strscpy(f->description, "Y/CbCr 4:2:2 semi-planar", sizeof(f->description));
	return 0;
}

static int suniv_tvd_g_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_format *f)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);

	suniv_tvd_fill_pix_format(tvd, &f->fmt.pix);
	return 0;
}

static int suniv_tvd_try_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);

	suniv_tvd_fill_pix_format(tvd, &f->fmt.pix);
	return 0;
}

static int suniv_tvd_s_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_format *f)
{
	struct suniv_tvd_dev *tvd = video_drvdata(file);

	if (vb2_is_busy(&tvd->vb_queue))
		return -EBUSY;

	suniv_tvd_fill_pix_format(tvd, &f->fmt.pix);
	return 0;
}

static const struct v4l2_ioctl_ops suniv_tvd_ioctl_ops = {
	.vidioc_querycap          = suniv_tvd_querycap,
	.vidioc_enum_input        = suniv_tvd_enum_input,
	.vidioc_g_input           = suniv_tvd_g_input,
	.vidioc_s_input           = suniv_tvd_s_input,
	.vidioc_log_status        = suniv_tvd_log_status,
	.vidioc_querystd          = suniv_tvd_querystd,
	.vidioc_g_std             = suniv_tvd_g_std,
	.vidioc_s_std             = suniv_tvd_s_std,
	.vidioc_enum_fmt_vid_cap  = suniv_tvd_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = suniv_tvd_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = suniv_tvd_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = suniv_tvd_s_fmt_vid_cap,
	.vidioc_reqbufs           = vb2_ioctl_reqbufs,
	.vidioc_create_bufs       = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf       = vb2_ioctl_prepare_buf,
	.vidioc_querybuf          = vb2_ioctl_querybuf,
	.vidioc_qbuf              = vb2_ioctl_qbuf,
	.vidioc_dqbuf             = vb2_ioctl_dqbuf,
	.vidioc_expbuf            = vb2_ioctl_expbuf,
	.vidioc_streamon          = vb2_ioctl_streamon,
	.vidioc_streamoff         = vb2_ioctl_streamoff,
};

static const struct v4l2_file_operations suniv_tvd_fops = {
	.owner          = THIS_MODULE,
	.open           = v4l2_fh_open,
	.release        = vb2_fop_release,
	.poll           = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = vb2_fop_mmap,
};

static int suniv_tvd_parse_dt(struct suniv_tvd_dev *tvd)
{
	struct device_node *np = tvd->dev->of_node;
	const char *std_name;
	u32 value;

	tvd->input_channel = 0;
	tvd->std = SUNIV_TVD_STD_NTSC;
	tvd->blue_screen_mode = 2;
	tvd->auto_detect = false;

	if (!np)
		return 0;

	if (!of_property_read_u32(np, "allwinner,input-channel", &value)) {
		if (value > 1)
			return -EINVAL;
		tvd->input_channel = value;
	}

	if (!of_property_read_string(np, "allwinner,default-standard", &std_name)) {
		if (!strcmp(std_name, "pal"))
			tvd->std = SUNIV_TVD_STD_PAL;
		else if (!strcmp(std_name, "ntsc"))
			tvd->std = SUNIV_TVD_STD_NTSC;
		else
			return -EINVAL;
	}

	if (!of_property_read_u32(np, "allwinner,blue-screen-mode", &value))
		tvd->blue_screen_mode = value;

	tvd->auto_detect = of_property_read_bool(np, "allwinner,auto-detect-standard");

	return 0;
}

static struct clk *suniv_tvd_get_clk_compat(struct device *dev,
					    const char *primary,
					    const char *fallback)
{
	struct clk *clk;

	clk = devm_clk_get(dev, primary);
	if (!IS_ERR(clk))
		return clk;

	if (fallback)
		clk = devm_clk_get(dev, fallback);

	return clk;
}

static struct reset_control *
suniv_tvd_get_reset_compat(struct device *dev, const char *primary,
			   const char *fallback)
{
	struct reset_control *rst;

	rst = devm_reset_control_get_optional_exclusive(dev, primary);
	if (!IS_ERR(rst))
		return rst;

	if (fallback)
		rst = devm_reset_control_get_optional_exclusive(dev, fallback);

	return rst;
}

static int suniv_tvd_video_register(struct suniv_tvd_dev *tvd)
{
	int ret;

	memset(&tvd->vdev, 0, sizeof(tvd->vdev));
	strscpy(tvd->vdev.name, "F1C100S TVD", sizeof(tvd->vdev.name));
	tvd->vdev.v4l2_dev = &tvd->v4l2_dev;
	tvd->vdev.fops = &suniv_tvd_fops;
	tvd->vdev.ioctl_ops = &suniv_tvd_ioctl_ops;
	tvd->vdev.release = video_device_release_empty;
	tvd->vdev.lock = &tvd->lock;
	tvd->vdev.queue = &tvd->vb_queue;
	tvd->vdev.device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	tvd->vdev.tvnorms = V4L2_STD_NTSC | V4L2_STD_PAL;

	video_set_drvdata(&tvd->vdev, tvd);

	ret = video_register_device(&tvd->vdev, VFL_TYPE_VIDEO, -1);
	if (ret)
		return ret;

	return 0;
}

static int suniv_tvd_probe(struct platform_device *pdev)
{
	struct suniv_tvd_dev *tvd;
	struct resource *res;
	int ret;

	tvd = devm_kzalloc(&pdev->dev, sizeof(*tvd), GFP_KERNEL);
	if (!tvd)
		return -ENOMEM;

	tvd->dev = &pdev->dev;
	mutex_init(&tvd->lock);
	spin_lock_init(&tvd->qlock);
	INIT_LIST_HEAD(&tvd->queued_bufs);
	platform_set_drvdata(pdev, tvd);

	ret = suniv_tvd_parse_dt(tvd);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tvd->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tvd->base))
		return PTR_ERR(tvd->base);

	tvd->ccu_base = devm_ioremap(&pdev->dev, SUNIV_CCU_BASE, SUNIV_CCU_SIZE);
	if (!tvd->ccu_base)
		dev_warn(&pdev->dev, "failed to map CCU registers, TVD DRAM gate not forced\n");

	tvd->irq = platform_get_irq(pdev, 0);
	if (tvd->irq < 0)
		return tvd->irq;

	tvd->clk_bus = suniv_tvd_get_clk_compat(&pdev->dev, "bus", "ahb");
	if (IS_ERR(tvd->clk_bus))
		return PTR_ERR(tvd->clk_bus);

	tvd->clk_mod = suniv_tvd_get_clk_compat(&pdev->dev, "mod", NULL);
	if (IS_ERR(tvd->clk_mod))
		return PTR_ERR(tvd->clk_mod);

	tvd->clk_ram = devm_clk_get_optional(&pdev->dev, "ram");
	if (IS_ERR(tvd->clk_ram))
		return PTR_ERR(tvd->clk_ram);

	tvd->rst_bus = suniv_tvd_get_reset_compat(&pdev->dev, "bus", "ahb");
	if (IS_ERR(tvd->rst_bus))
		return PTR_ERR(tvd->rst_bus);

	ret = suniv_tvd_enable_clocks(tvd);
	if (ret)
		return ret;

	suniv_tvd_enable_dram_gate(tvd);
	suniv_tvd_enable_clock_registers(tvd);
	suniv_tvd_apply_standard(tvd, tvd->std);

	ret = devm_request_irq(&pdev->dev, tvd->irq, suniv_tvd_irq, 0,
			       dev_name(&pdev->dev), tvd);
	if (ret)
		goto err_clk;

	ret = v4l2_device_register(&pdev->dev, &tvd->v4l2_dev);
	if (ret)
		goto err_clk;

	memset(&tvd->vb_queue, 0, sizeof(tvd->vb_queue));
	tvd->vb_queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	tvd->vb_queue.io_modes = VB2_MMAP | VB2_DMABUF;
	tvd->vb_queue.dev = &pdev->dev;
	tvd->vb_queue.drv_priv = tvd;
	tvd->vb_queue.buf_struct_size = sizeof(struct suniv_tvd_buffer);
	tvd->vb_queue.ops = &suniv_tvd_vb2_ops;
	tvd->vb_queue.mem_ops = &vb2_dma_contig_memops;
	tvd->vb_queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	tvd->vb_queue.lock = &tvd->lock;
	tvd->vb_queue.min_buffers_needed = 2;
	tvd->vb_queue.alloc_devs[0] = &pdev->dev;

	ret = vb2_queue_init(&tvd->vb_queue);
	if (ret)
		goto err_v4l2;

	ret = suniv_tvd_video_register(tvd);
	if (ret)
		goto err_v4l2;

	dev_info(&pdev->dev, "registered /dev/video node for TVD capture\n");
	return 0;

err_v4l2:
	v4l2_device_unregister(&tvd->v4l2_dev);
err_clk:
	suniv_tvd_disable_clocks(tvd);
	return ret;
}

static int suniv_tvd_remove(struct platform_device *pdev)
{
	struct suniv_tvd_dev *tvd = platform_get_drvdata(pdev);

	video_unregister_device(&tvd->vdev);
	v4l2_device_unregister(&tvd->v4l2_dev);
	suniv_tvd_write(tvd, SUNIV_TVD_REG_INT_ENABLE, 0);
	suniv_tvd_capture_off(tvd);
	suniv_tvd_disable_clocks(tvd);

	return 0;
}

static const struct of_device_id suniv_tvd_of_match[] = {
	{ .compatible = "allwinner,suniv-f1c100s-tvd" },
	{ }
};
MODULE_DEVICE_TABLE(of, suniv_tvd_of_match);

static struct platform_driver suniv_tvd_driver = {
	.probe  = suniv_tvd_probe,
	.remove = suniv_tvd_remove,
	.driver = {
		.name           = SUNIV_TVD_NAME,
		.of_match_table = suniv_tvd_of_match,
	},
};
module_platform_driver(suniv_tvd_driver);

MODULE_DESCRIPTION("Allwinner F1C100S TVD capture driver");
MODULE_AUTHOR("OpenAI");
MODULE_LICENSE("GPL");
