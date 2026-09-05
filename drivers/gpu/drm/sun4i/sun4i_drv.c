// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2015 Free Electrons
 * Copyright (C) 2015 NextThing Co
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 */

#include "sun4i_backend.h"
#include <linux/component.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "sun4i_drv.h"
#include "sun4i_frontend.h"
#include "sun4i_framebuffer.h"
#include "sun4i_tcon.h"
#include "sunxi_engine.h"
#include "sun8i_tcon_top.h"
#include "uapi/drm/srgn_drm.h"

#include <linux/types.h>


static int drm_sun4i_gem_dumb_create(struct drm_file *file_priv,
				     struct drm_device *drm,
				     struct drm_mode_create_dumb *args)
{
	/* The hardware only allows even pitches for YUV buffers. */
	args->pitch = ALIGN(DIV_ROUND_UP(args->width * args->bpp, 8), 2);

	return drm_gem_cma_dumb_create_internal(file_priv, drm, args);
}

DEFINE_DRM_GEM_CMA_FOPS(sun4i_drv_fops);

#if 0
	#define SRGN_DEBUG_PRINT(fmt, ...) printk(KERN_INFO "srgn: " fmt, ##__VA_ARGS__)
#else
	#define SRGN_DEBUG_PRINT(fmt, ...)
#endif

static int srgn_get_user_first_phys(unsigned long uaddr, phys_addr_t *out_phys)
{
    struct page *page;
    int ret;
    unsigned long addr = uaddr & PAGE_MASK;
    unsigned long offset = uaddr & ~PAGE_MASK;
	struct vm_area_struct *vma;
	unsigned long original_flags;

	vma = find_vma(current->mm, addr);
	if (!vma || addr < vma->vm_start) {
		SRGN_DEBUG_PRINT("no vma for addr 0x%lx\n", addr);
	} else {
		SRGN_DEBUG_PRINT("vma: start=0x%lx end=0x%lx flags=0x%lx\n",
				vma->vm_start, vma->vm_end, vma->vm_flags);
	}

	// some very,very,very dirty hack to get the physical address from the user address
	// because drm buffer was allocate with VM_IO and VM_PFNMAP flags,
	// which is not allowed to GUP.
	original_flags = vma->vm_flags;
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags &= ~VM_IO;


    if (!out_phys)
        return -EINVAL;


    ret = get_user_pages_unlocked(addr, 1, &page, 0);
    if (ret < 0){
		SRGN_DEBUG_PRINT("srgn_get_user_first_phys get_user_pages failed(%d)\n", ret);
        return ret;
	}
    if (ret != 1){
		SRGN_DEBUG_PRINT("srgn_get_user_first_phys get_user_pages failed length not corr:%d\n",ret);
        return -EFAULT;
	}

    *out_phys = page_to_phys(page) + offset;

    put_page(page);
	vma->vm_flags = original_flags;

    return 0;
}


static unsigned long uaddr_cache[128];
static phys_addr_t phys_addr_cache[128];
static int cache_count = 0;

static int srgn_get_phy_from_cache(unsigned long uaddr, phys_addr_t *out_phys){
	int i;

	for (i = 0; i < cache_count; i++){
		if (uaddr_cache[i] == uaddr){
			SRGN_DEBUG_PRINT("get from cache: uaddr=%px, phys_addr=%px\n", uaddr, phys_addr_cache[i]);
			*out_phys = phys_addr_cache[i];
			return 0;
		}
	}
	return -EINVAL;

}

static int srgn_add_to_cache(unsigned long uaddr, phys_addr_t phys_addr){
	if (cache_count >= 128){
		return -ENOMEM;
	}
	uaddr_cache[cache_count] = uaddr;
	phys_addr_cache[cache_count] = phys_addr;
	cache_count++;
	SRGN_DEBUG_PRINT("add to cache: uaddr=%px, phys_addr=%px, cache count=%d\n", uaddr, phys_addr, cache_count);
	return 0;
}

static int srgn_get_phy_addr(unsigned long uaddr, phys_addr_t *out_phys)
{
	int ret;
	ret = srgn_get_phy_from_cache(uaddr, out_phys);
	if (ret == 0){
		return 0;
	}
	ret = srgn_get_user_first_phys(uaddr, out_phys);
	if (ret == 0){
		srgn_add_to_cache(uaddr, *out_phys);
	}
	return ret;
}

static int srgn_reset_cache(struct drm_device *drm, void *data, struct drm_file *file)
{
	printk(KERN_INFO "srgn: reset cache\n");
	cache_count = 0;
	return 0;
}


static int srgn_atomic_commit_mount_fb_normal(struct sun4i_backend *backend, struct drm_srgn_atomic_commit_data *data)
{
	phys_addr_t phys_addr;
	u32 lo_paddr, hi_paddr;
	uint32_t layer = data->layer_id;

	int ret;

	ret = srgn_get_phy_addr(data->arg0, &phys_addr);
	if (ret < 0) {
		return ret;
	}

	/* Write the 32 lower bits of the address (in bits) */
	phys_addr -= PHYS_OFFSET;
	lo_paddr = phys_addr << 3;
	SRGN_DEBUG_PRINT("Setting address lower bits to 0x%x\n", lo_paddr);
	regmap_write(backend->engine.regs,
		     SUN4I_BACKEND_LAYFB_L32ADD_REG(layer),
		     lo_paddr);

	/* And the upper bits */
	hi_paddr = phys_addr >> 29;
	SRGN_DEBUG_PRINT("Setting address high bits to 0x%x\n", hi_paddr);
	regmap_update_bits(backend->engine.regs, SUN4I_BACKEND_LAYFB_H4ADD_REG,
			   SUN4I_BACKEND_LAYFB_H4ADD_MSK(layer),
			   SUN4I_BACKEND_LAYFB_H4ADD(layer, hi_paddr));

	return 0;
}

static bool srgn_yuv_crop_enabled;
static u32 srgn_yuv_crop_y_offset;
static u32 srgn_yuv_crop_uv_offset;

static u32 srgn_mb32_offset(u32 pitch, u32 x, u32 y)
{
	return (y / 32) * pitch * 32 + (x / 32) * 1024 + (y & 31) * 32 + (x & 31);
}

static int srgn_atomic_commit_mount_fb_yuv(struct sun4i_frontend *frontend, struct drm_srgn_atomic_commit_data *data)
{
	phys_addr_t phys_addr_y, phys_addr_uv;
	phys_addr_t phys_addr;

	int ret;

	ret = srgn_get_phy_addr(data->arg0, &phys_addr_y);
	if (ret < 0) {
		return ret;
	}
	ret = srgn_get_phy_addr(data->arg1, &phys_addr_uv);
	if (ret < 0) {
		return ret;
	}

	if (srgn_yuv_crop_enabled) {
		phys_addr_y += srgn_yuv_crop_y_offset;
		phys_addr_uv += srgn_yuv_crop_uv_offset;
	}

	phys_addr = phys_addr_y;
	phys_addr -= PHYS_OFFSET;
	SRGN_DEBUG_PRINT("Setting buffer #0 address to %pad\n", &phys_addr);
	regmap_write(frontend->regs, SUN4I_FRONTEND_BUF_ADDR0_REG, phys_addr);

	phys_addr = phys_addr_uv;
	phys_addr -= PHYS_OFFSET;
	SRGN_DEBUG_PRINT("Setting buffer #1 address to %pad\n", &phys_addr);
	regmap_write(frontend->regs, SUN4I_FRONTEND_BUF_ADDR1_REG,
		phys_addr);

	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
		SUN4I_FRONTEND_FRM_CTRL_REG_RDY,
		SUN4I_FRONTEND_FRM_CTRL_REG_RDY);
	
	return 0;
}

static int srgn_atomic_commit_mount_set_coord(struct sun4i_backend *backend, struct drm_srgn_atomic_commit_data *data){
	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYCOOR_REG(data->layer_id), data->arg0);
	return 0;
}

static int srgn_atomic_commit_mount_set_alpha(struct sun4i_backend *backend, struct drm_srgn_atomic_commit_data *data){
	uint32_t alpha = data->arg0 & 0xff;
	uint32_t layer = data->layer_id;
	uint32_t val = SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA(alpha);
	if (alpha != 255)
		val |= SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_EN;
	regmap_update_bits(backend->engine.regs,
			   SUN4I_BACKEND_ATTCTL_REG0(layer),
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_MASK |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_EN,
			   val);
	return 0;
}

/*
 * Cedar owns frontend/YUV layer 0. Keep the LVGL RGB565 OSD on layer 1 so
 * page flips only update a normal backend framebuffer address.
 */
static int srgn_atomic_commit_mount_config_rgb565_osd(
	struct sun4i_backend *backend,
	struct drm_srgn_atomic_commit_data *data)
{
	u32 layer = data->layer_id;
	u32 width = data->arg0 & 0xffff;
	u32 height = (data->arg0 >> 16) & 0xffff;
	u32 stride = data->arg2;

	if (!backend || layer != 1)
		return -EINVAL;

	if (!width && !height) {
		sun4i_backend_layer_enable(backend, layer, false);
		return 0;
	}

	if (!width || !height ||
	    width > 4096 || height > 4096 ||
	    stride < width * 2 || stride > 8192 || (stride & 1))
		return -EINVAL;

	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYSIZE_REG(layer),
		     SUN4I_BACKEND_LAYSIZE(width, height));
	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYCOOR_REG(layer),
		     data->arg1);
	regmap_write(backend->engine.regs,
		     SUN4I_BACKEND_LAYLINEWIDTH_REG(layer), stride * 8);

	regmap_update_bits(backend->engine.regs,
			   SUN4I_BACKEND_ATTCTL_REG0(layer),
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_VDOEN |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_YUVEN |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_PIPESEL_MASK |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_PRISEL_MASK |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_MASK |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_EN,
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_PIPESEL(1) |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_PRISEL(1) |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA(255));
	regmap_update_bits(backend->engine.regs,
			   SUN4I_BACKEND_ATTCTL_REG1(layer),
			   SUN4I_BACKEND_ATTCTL_REG1_LAY_FBFMT,
			   SUN4I_BACKEND_LAY_FBFMT_RGB565);
	sun4i_backend_layer_enable(backend, layer, true);

	return 0;
}


static int srgn_atomic_commit_mount_set_yuv_view(struct sun4i_frontend *frontend,
	struct sun4i_backend *backend, struct drm_srgn_atomic_commit_data *data)
{
	srgn_yuv_crop_enabled = false;
	u32 layer = data->layer_id;
	u32 src_w = data->arg0 & 0xffff;
	u32 src_h = (data->arg0 >> 16) & 0xffff;
	u32 out_w = data->arg1 & 0xffff;
	u32 out_h = (data->arg1 >> 16) & 0xffff;
	u32 chroma_src_w, chroma_src_h;
	u32 chroma_out_w, chroma_out_h;
	u32 pitch;
	int i;
	int ret;

	if (!frontend || !backend || layer >= SUN4I_BACKEND_NUM_LAYERS ||
	    !src_w || !src_h || !out_w || !out_h) {
		printk(KERN_ERR "srgn: invalid yuv view layer=%u src=%ux%u out=%ux%u\n",
			layer, src_w, src_h, out_w, out_h);
		return -EINVAL;
	}

	ret = sun4i_frontend_init(frontend);
	if (ret < 0) {
		printk(KERN_ERR "srgn: frontend init failed %d\n", ret);
		return ret;
	}

	chroma_src_w = DIV_ROUND_UP(src_w, 2);
	chroma_src_h = DIV_ROUND_UP(src_h, 2);
	chroma_out_w = DIV_ROUND_UP(out_w, 2);
	chroma_out_h = DIV_ROUND_UP(out_h, 2);
	pitch = ALIGN(src_w, 2);
	if (src_w == 384 && out_w == 384 && src_h == 360)
		pitch = 640; /* cropped 640-wide MB32 source keeps original pitch */
	else if (src_w == 384 && out_w == 384 && src_h == 600)
		pitch = 800; /* cropped 800-wide MB32 source keeps original pitch */

	for (i = 0; i < ARRAY_SIZE(sunxi_bt601_yuv2rgb_coef); i++)
		regmap_write(frontend->regs, SUN4I_FRONTEND_CSC_COEF_REG(i),
			     sunxi_bt601_yuv2rgb_coef[i]);

	regmap_update_bits(frontend->regs, SUN4I_FRONTEND_BYPASS_REG,
			   SUN4I_FRONTEND_BYPASS_CSC_EN, 0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_INPUT_FMT_REG,
		     SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_MB32_SEMIPLANAR |
		     SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_YUV420 |
		     SUN4I_FRONTEND_INPUT_FMT_DATA_PS_UV);
	regmap_write(frontend->regs, SUN4I_FRONTEND_OUTPUT_FMT_REG,
		     SUN4I_FRONTEND_OUTPUT_FMT_DATA_FMT_XRGB8888);

	regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD0_REG,
		     SUN4I_FRONTEND_LINESTRD_TILED(pitch));
	regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD1_REG,
		     SUN4I_FRONTEND_LINESTRD_TILED(pitch));
	regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF0_REG,
		     SUN4I_FRONTEND_TB_OFF_X1((src_w - 1) & 31));
	regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF1_REG,
		     SUN4I_FRONTEND_TB_OFF_X1((src_w - 1) & 31));

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_INSIZE_REG,
		     SUN4I_FRONTEND_INSIZE(src_h, src_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_INSIZE_REG,
		     SUN4I_FRONTEND_INSIZE(chroma_src_h, chroma_src_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_OUTSIZE_REG,
		     SUN4I_FRONTEND_OUTSIZE(out_h, out_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_OUTSIZE_REG,
		     SUN4I_FRONTEND_OUTSIZE(chroma_out_h, chroma_out_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZFACT_REG,
		     (src_w << 16) / out_w);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZFACT_REG,
		     (chroma_src_w << 16) / chroma_out_w);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTFACT_REG,
		     (src_h << 16) / out_h);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTFACT_REG,
		     (chroma_src_h << 16) / chroma_out_h);

	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYSIZE_REG(layer),
		     SUN4I_BACKEND_LAYSIZE(out_w, out_h));
	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYCOOR_REG(layer),
		     data->arg2);
	regmap_update_bits(backend->engine.regs, SUN4I_BACKEND_ATTCTL_REG0(layer),
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_VDOEN |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_MASK,
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_VDOEN |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA(255));
	regmap_update_bits(backend->engine.regs, SUN4I_BACKEND_ATTCTL_REG1(layer),
			   SUN4I_BACKEND_ATTCTL_REG1_LAY_FBFMT,
			   SUN4I_BACKEND_LAY_FBFMT_XRGB8888);
	sun4i_backend_layer_enable(backend, layer, true);

	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			  SUN4I_FRONTEND_FRM_CTRL_REG_RDY,
			  SUN4I_FRONTEND_FRM_CTRL_REG_RDY);
	sun4i_frontend_enable(frontend);

	printk(KERN_INFO "srgn: yuv view layer=%u src=%ux%u out=%ux%u coord=0x%08x\n",
		layer, src_w, src_h, out_w, out_h, data->arg2);
	return 0;
}


static int srgn_atomic_commit_mount_set_yuv_center_crop_view(struct sun4i_frontend *frontend,
	struct sun4i_backend *backend, struct drm_srgn_atomic_commit_data *data)
{
	u32 layer = data->layer_id;
	u32 full_w = data->arg0 & 0xffff;
	u32 full_h = (data->arg0 >> 16) & 0xffff;
	u32 crop_w = data->arg1 & 0xffff;
	u32 crop_h = (data->arg1 >> 16) & 0xffff;
	u32 out_w = data->arg2 & 0xffff;
	u32 out_h = (data->arg2 >> 16) & 0xffff;
	u32 crop_x, crop_y;
	u32 chroma_crop_w, chroma_crop_h;
	u32 chroma_out_w, chroma_out_h;
	u32 pitch;
	int i;
	int ret;

	if (!frontend || !backend || layer >= SUN4I_BACKEND_NUM_LAYERS ||
	    !full_w || !full_h || !crop_w || !crop_h || !out_w || !out_h ||
	    crop_w > full_w || crop_h > full_h) {
		printk(KERN_ERR "srgn: invalid yuv center crop layer=%u full=%ux%u crop=%ux%u out=%ux%u\n",
			layer, full_w, full_h, crop_w, crop_h, out_w, out_h);
		return -EINVAL;
	}

	ret = sun4i_frontend_init(frontend);
	if (ret < 0) {
		printk(KERN_ERR "srgn: frontend init failed %d\n", ret);
		return ret;
	}

	pitch = ALIGN(full_w, 2);
	crop_x = ((full_w - crop_w) / 2) & ~31U;
	crop_y = 0;
	srgn_yuv_crop_y_offset = srgn_mb32_offset(pitch, crop_x, crop_y);
	srgn_yuv_crop_uv_offset = srgn_mb32_offset(pitch, crop_x, crop_y / 2);
	srgn_yuv_crop_enabled = true;

	chroma_crop_w = DIV_ROUND_UP(crop_w, 2);
	chroma_crop_h = DIV_ROUND_UP(crop_h, 2);
	chroma_out_w = DIV_ROUND_UP(out_w, 2);
	chroma_out_h = DIV_ROUND_UP(out_h, 2);

	for (i = 0; i < ARRAY_SIZE(sunxi_bt601_yuv2rgb_coef); i++)
		regmap_write(frontend->regs, SUN4I_FRONTEND_CSC_COEF_REG(i),
			     sunxi_bt601_yuv2rgb_coef[i]);

	regmap_update_bits(frontend->regs, SUN4I_FRONTEND_BYPASS_REG,
			   SUN4I_FRONTEND_BYPASS_CSC_EN, 0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_INPUT_FMT_REG,
		     SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_MB32_SEMIPLANAR |
		     SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_YUV420 |
		     SUN4I_FRONTEND_INPUT_FMT_DATA_PS_UV);
	regmap_write(frontend->regs, SUN4I_FRONTEND_OUTPUT_FMT_REG,
		     SUN4I_FRONTEND_OUTPUT_FMT_DATA_FMT_XRGB8888);

	regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD0_REG,
		     SUN4I_FRONTEND_LINESTRD_TILED(pitch));
	regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD1_REG,
		     SUN4I_FRONTEND_LINESTRD_TILED(pitch));
	regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF0_REG,
		     SUN4I_FRONTEND_TB_OFF_X1((crop_w - 1) & 31));
	regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF1_REG,
		     SUN4I_FRONTEND_TB_OFF_X1((crop_w - 1) & 31));

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_INSIZE_REG,
		     SUN4I_FRONTEND_INSIZE(crop_h, crop_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_INSIZE_REG,
		     SUN4I_FRONTEND_INSIZE(chroma_crop_h, chroma_crop_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_OUTSIZE_REG,
		     SUN4I_FRONTEND_OUTSIZE(out_h, out_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_OUTSIZE_REG,
		     SUN4I_FRONTEND_OUTSIZE(chroma_out_h, chroma_out_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZFACT_REG,
		     (crop_w << 16) / out_w);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZFACT_REG,
		     (chroma_crop_w << 16) / chroma_out_w);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTFACT_REG,
		     (crop_h << 16) / out_h);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTFACT_REG,
		     (chroma_crop_h << 16) / chroma_out_h);

	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYSIZE_REG(layer),
		     SUN4I_BACKEND_LAYSIZE(out_w, out_h));
	regmap_write(backend->engine.regs, SUN4I_BACKEND_LAYCOOR_REG(layer), 0);
	regmap_update_bits(backend->engine.regs, SUN4I_BACKEND_ATTCTL_REG0(layer),
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_VDOEN |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA_MASK,
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_VDOEN |
			   SUN4I_BACKEND_ATTCTL_REG0_LAY_GLBALPHA(255));
	regmap_update_bits(backend->engine.regs, SUN4I_BACKEND_ATTCTL_REG1(layer),
			   SUN4I_BACKEND_ATTCTL_REG1_LAY_FBFMT,
			   SUN4I_BACKEND_LAY_FBFMT_XRGB8888);
	sun4i_backend_layer_enable(backend, layer, true);

	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			  SUN4I_FRONTEND_FRM_CTRL_REG_RDY,
			  SUN4I_FRONTEND_FRM_CTRL_REG_RDY);
	sun4i_frontend_enable(frontend);

	printk(KERN_INFO "srgn: yuv center crop layer=%u full=%ux%u crop=%ux%u at %u,%u out=%ux%u yoff=%u uvoff=%u\n",
		layer, full_w, full_h, crop_w, crop_h, crop_x, crop_y, out_w, out_h,
		srgn_yuv_crop_y_offset, srgn_yuv_crop_uv_offset);
	return 0;
}

int srgn_atomic_commit(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_srgn_atomic_commit *kreq = data;
	struct sun4i_drv *drv = drm->dev_private;
	static struct sunxi_engine *engine;
	static struct sun4i_frontend *frontend;
	static struct sun4i_backend *backend;
	static struct sun4i_tcon *tcon;

	// 3 io req max for 4 layers,
	struct drm_srgn_atomic_commit_data kcmd[12] = {0};
	struct drm_srgn_atomic_commit_data* arg;

	static bool first_time = true;
	int ret;
	int i;


	struct list_head *entry;

	// theres only 1 engine,1 fe 1 tcon in f1c100s.
	// we only need to get the pointers once, and keep them for the lifetime of the driver
	if (first_time) {
		list_for_each(entry, &drv->engine_list) {
			engine = container_of(entry, struct sunxi_engine, list);
			backend = engine_to_sun4i_backend(engine);
		}
		list_for_each(entry, &drv->frontend_list) {
			frontend = container_of(entry, struct sun4i_frontend, list);
		}
		list_for_each(entry, &drv->tcon_list) {
			tcon = container_of(entry, struct sun4i_tcon, list);
		}
		first_time = false;
	}

	SRGN_DEBUG_PRINT("ioctl_backend: %px\n", backend);
	SRGN_DEBUG_PRINT("ioctl_frontend: %px\n", frontend);
	SRGN_DEBUG_PRINT("ioctl_tcon: %px\n", tcon);

	SRGN_DEBUG_PRINT("sizeof(arg)=%ld\n", kreq->size);

	if(kreq->size > 12){
		printk(KERN_ERR "srgn: size > 12\n");
		return -EINVAL;
	}
	if (copy_from_user(kcmd, (void __user *)kreq->data, kreq->size * sizeof(struct drm_srgn_atomic_commit_data))) {
		printk(KERN_ERR "srgn: copy_from_user failed\n");
        return -EFAULT;
    }
	
	for (i=0; i<kreq->size; i++) {
		arg = &kcmd[i];
		SRGN_DEBUG_PRINT("srgn_atomic_commit: layer_id=%d, type=%d, arg0=0x%x, arg1=0x%x, arg2=0x%x\n",
			arg->layer_id, arg->type, arg->arg0, arg->arg1, arg->arg2);
		switch(arg->type){
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL:
				ret = srgn_atomic_commit_mount_fb_normal(backend, arg);
				break;
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV:
				ret = srgn_atomic_commit_mount_fb_yuv(frontend, arg);
				break;
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_COORD:
				ret = srgn_atomic_commit_mount_set_coord(backend, arg);
				break;
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA:
				ret = srgn_atomic_commit_mount_set_alpha(backend, arg);
				break;
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_CONFIG_RGB565_OSD:
				ret = srgn_atomic_commit_mount_config_rgb565_osd(backend, arg);
				break;
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_YUV_VIEW:
				ret = srgn_atomic_commit_mount_set_yuv_view(frontend, backend, arg);
				break;
			case DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_YUV_CENTER_CROP_VIEW:
				ret = srgn_atomic_commit_mount_set_yuv_center_crop_view(frontend, backend, arg);
				break;
			default:
				ret = -EINVAL;
				break;
		}
		if (ret < 0) {
			printk(KERN_ERR "Error: srgn_atomic_commit: layer_id=%d, type=%d, arg0=%d, arg1=%d, arg2=%d failed\n",
				arg->layer_id, arg->type, arg->arg0, arg->arg1, arg->arg2);
			return ret;
		}
		
	}

	return 0;
}

static const struct drm_ioctl_desc srgn_sun4i_ioctls[] = {
	DRM_IOCTL_DEF_DRV(SRGN_ATOMIC_COMMIT, srgn_atomic_commit,
		DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(SRGN_RESET_FB_CACHE, srgn_reset_cache,
		DRM_RENDER_ALLOW),
};

static struct drm_driver sun4i_drv_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,

	/* Generic Operations */
	.fops			= &sun4i_drv_fops,
	.name			= "sun4i-drm",
	.desc			= "Allwinner sun4i Display Engine",
	.date			= "20150629",
	.major			= 1,
	.minor			= 0,

	/* GEM Operations */
	DRM_GEM_CMA_VMAP_DRIVER_OPS,
	.dumb_create		= drm_sun4i_gem_dumb_create,
	
	.ioctls		= srgn_sun4i_ioctls,
	.num_ioctls = ARRAY_SIZE(srgn_sun4i_ioctls),
};

static int sun4i_drv_bind(struct device *dev)
{
	struct drm_device *drm;
	struct sun4i_drv *drv;
	int ret;

	drm = drm_dev_alloc(&sun4i_drv_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);
	if (!drv) {
		ret = -ENOMEM;
		goto free_drm;
	}

	dev_set_drvdata(dev, drm);
	drm->dev_private = drv;
	INIT_LIST_HEAD(&drv->frontend_list);
	INIT_LIST_HEAD(&drv->engine_list);
	INIT_LIST_HEAD(&drv->tcon_list);

	ret = of_reserved_mem_device_init(dev);
	if (ret && ret != -ENODEV) {
		dev_err(drm->dev, "Couldn't claim our memory region\n");
		goto free_drm;
	}

	drm_mode_config_init(drm);

	ret = component_bind_all(drm->dev, drm);
	if (ret) {
		dev_err(drm->dev, "Couldn't bind all pipelines components\n");
		goto cleanup_mode_config;
	}

	/* drm_vblank_init calls kcalloc, which can fail */
	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto cleanup_mode_config;

	drm->irq_enabled = true;

	/* Remove early framebuffers (ie. simplefb) */
	drm_fb_helper_remove_conflicting_framebuffers(NULL, "sun4i-drm-fb", false);

	sun4i_framebuffer_init(drm);

	/* Enable connectors polling */
	drm_kms_helper_poll_init(drm);

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto finish_poll;

	drm_fbdev_generic_setup(drm, 32);

	return 0;

finish_poll:
	drm_kms_helper_poll_fini(drm);
cleanup_mode_config:
	drm_mode_config_cleanup(drm);
	of_reserved_mem_device_release(dev);
free_drm:
	drm_dev_put(drm);
	return ret;
}

static void sun4i_drv_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);
	drm_kms_helper_poll_fini(drm);
	drm_atomic_helper_shutdown(drm);
	drm_mode_config_cleanup(drm);

	component_unbind_all(dev, NULL);
	of_reserved_mem_device_release(dev);

	drm_dev_put(drm);
}

static const struct component_master_ops sun4i_drv_master_ops = {
	.bind	= sun4i_drv_bind,
	.unbind	= sun4i_drv_unbind,
};

static bool sun4i_drv_node_is_connector(struct device_node *node)
{
	return of_device_is_compatible(node, "hdmi-connector");
}

static bool sun4i_drv_node_is_frontend(struct device_node *node)
{
	return of_device_is_compatible(node, "allwinner,sun4i-a10-display-frontend") ||
		of_device_is_compatible(node, "allwinner,sun5i-a13-display-frontend") ||
		of_device_is_compatible(node, "allwinner,sun6i-a31-display-frontend") ||
		of_device_is_compatible(node, "allwinner,sun7i-a20-display-frontend") ||
		of_device_is_compatible(node, "allwinner,sun8i-a23-display-frontend") ||
		of_device_is_compatible(node, "allwinner,sun8i-a33-display-frontend") ||
		of_device_is_compatible(node, "allwinner,sun9i-a80-display-frontend");
}

static bool sun4i_drv_node_is_deu(struct device_node *node)
{
	return of_device_is_compatible(node, "allwinner,sun9i-a80-deu");
}

static bool sun4i_drv_node_is_supported_frontend(struct device_node *node)
{
	if (IS_ENABLED(CONFIG_DRM_SUN4I_BACKEND))
		return !!of_match_node(sun4i_frontend_of_table, node);

	return false;
}

static bool sun4i_drv_node_is_tcon(struct device_node *node)
{
	return !!of_match_node(sun4i_tcon_of_table, node);
}

static bool sun4i_drv_node_is_tcon_with_ch0(struct device_node *node)
{
	const struct of_device_id *match;

	match = of_match_node(sun4i_tcon_of_table, node);
	if (match) {
		struct sun4i_tcon_quirks *quirks;

		quirks = (struct sun4i_tcon_quirks *)match->data;

		return quirks->has_channel_0;
	}

	return false;
}

static bool sun4i_drv_node_is_tcon_top(struct device_node *node)
{
	return IS_ENABLED(CONFIG_DRM_SUN8I_TCON_TOP) &&
		!!of_match_node(sun8i_tcon_top_of_table, node);
}

static int compare_of(struct device *dev, void *data)
{
	DRM_DEBUG_DRIVER("Comparing of node %pOF with %pOF\n",
			 dev->of_node,
			 data);

	return dev->of_node == data;
}

/*
 * The encoder drivers use drm_of_find_possible_crtcs to get upstream
 * crtcs from the device tree using of_graph. For the results to be
 * correct, encoders must be probed/bound after _all_ crtcs have been
 * created. The existing code uses a depth first recursive traversal
 * of the of_graph, which means the encoders downstream of the TCON
 * get add right after the first TCON. The second TCON or CRTC will
 * never be properly associated with encoders connected to it.
 *
 * Also, in a dual display pipeline setup, both frontends can feed
 * either backend, and both backends can feed either TCON, we want
 * all components of the same type to be added before the next type
 * in the pipeline. Fortunately, the pipelines are perfectly symmetric,
 * i.e. components of the same type are at the same depth when counted
 * from the frontend. The only exception is the third pipeline in
 * the A80 SoC, which we do not support anyway.
 *
 * Hence we can use a breadth first search traversal order to add
 * components. We do not need to check for duplicates. The component
 * matching system handles this for us.
 */
struct endpoint_list {
	DECLARE_KFIFO(fifo, struct device_node *, 16);
};

static void sun4i_drv_traverse_endpoints(struct endpoint_list *list,
					 struct device_node *node,
					 int port_id)
{
	struct device_node *ep, *remote, *port;

	port = of_graph_get_port_by_id(node, port_id);
	if (!port) {
		DRM_DEBUG_DRIVER("No output to bind on port %d\n", port_id);
		return;
	}

	for_each_available_child_of_node(port, ep) {
		remote = of_graph_get_remote_port_parent(ep);
		if (!remote) {
			DRM_DEBUG_DRIVER("Error retrieving the output node\n");
			continue;
		}

		if (sun4i_drv_node_is_tcon(node)) {
			/*
			 * TCON TOP is always probed before TCON. However, TCON
			 * points back to TCON TOP when it is source for HDMI.
			 * We have to skip it here to prevent infinite looping
			 * between TCON TOP and TCON.
			 */
			if (sun4i_drv_node_is_tcon_top(remote)) {
				DRM_DEBUG_DRIVER("TCON output endpoint is TCON TOP... skipping\n");
				of_node_put(remote);
				continue;
			}

			/*
			 * If the node is our TCON with channel 0, the first
			 * port is used for panel or bridges, and will not be
			 * part of the component framework.
			 */
			if (sun4i_drv_node_is_tcon_with_ch0(node)) {
				struct of_endpoint endpoint;

				if (of_graph_parse_endpoint(ep, &endpoint)) {
					DRM_DEBUG_DRIVER("Couldn't parse endpoint\n");
					of_node_put(remote);
					continue;
				}

				if (!endpoint.id) {
					DRM_DEBUG_DRIVER("Endpoint is our panel... skipping\n");
					of_node_put(remote);
					continue;
				}
			}
		}

		kfifo_put(&list->fifo, remote);
	}
}

static int sun4i_drv_add_endpoints(struct device *dev,
				   struct endpoint_list *list,
				   struct component_match **match,
				   struct device_node *node)
{
	int count = 0;

	/*
	 * The frontend has been disabled in some of our old device
	 * trees. If we find a node that is the frontend and is
	 * disabled, we should just follow through and parse its
	 * child, but without adding it to the component list.
	 * Otherwise, we obviously want to add it to the list.
	 */
	if (!sun4i_drv_node_is_frontend(node) &&
	    !of_device_is_available(node))
		return 0;

	/*
	 * The connectors will be the last nodes in our pipeline, we
	 * can just bail out.
	 */
	if (sun4i_drv_node_is_connector(node))
		return 0;

	/*
	 * If the device is either just a regular device, or an
	 * enabled frontend supported by the driver, we add it to our
	 * component list.
	 */
	if (!(sun4i_drv_node_is_frontend(node) ||
	      sun4i_drv_node_is_deu(node)) ||
	    (sun4i_drv_node_is_supported_frontend(node) &&
	     of_device_is_available(node))) {
		/* Add current component */
		DRM_DEBUG_DRIVER("Adding component %pOF\n", node);
		drm_of_component_match_add(dev, match, compare_of, node);
		count++;
	}

	/* each node has at least one output */
	sun4i_drv_traverse_endpoints(list, node, 1);

	/* TCON TOP has second and third output */
	if (sun4i_drv_node_is_tcon_top(node)) {
		sun4i_drv_traverse_endpoints(list, node, 3);
		sun4i_drv_traverse_endpoints(list, node, 5);
	}

	return count;
}

#ifdef CONFIG_PM_SLEEP
static int sun4i_drv_drm_sys_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm);
}

static int sun4i_drv_drm_sys_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm);
}
#endif

static const struct dev_pm_ops sun4i_drv_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sun4i_drv_drm_sys_suspend,
				sun4i_drv_drm_sys_resume)
};

static int sun4i_drv_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	struct device_node *np = pdev->dev.of_node, *endpoint;
	struct endpoint_list list;
	int i, ret, count = 0;

	INIT_KFIFO(list.fifo);

	for (i = 0;; i++) {
		struct device_node *pipeline = of_parse_phandle(np,
								"allwinner,pipelines",
								i);
		if (!pipeline)
			break;

		kfifo_put(&list.fifo, pipeline);
	}

	while (kfifo_get(&list.fifo, &endpoint)) {
		/* process this endpoint */
		ret = sun4i_drv_add_endpoints(&pdev->dev, &list, &match,
					      endpoint);

		/* sun4i_drv_add_endpoints can fail to allocate memory */
		if (ret < 0)
			return ret;

		count += ret;
	}

	if (count)
		return component_master_add_with_match(&pdev->dev,
						       &sun4i_drv_master_ops,
						       match);
	else
		return 0;
}

static int sun4i_drv_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &sun4i_drv_master_ops);

	return 0;
}

static const struct of_device_id sun4i_drv_of_table[] = {
	{ .compatible = "allwinner,sun4i-a10-display-engine" },
	{ .compatible = "allwinner,sun5i-a10s-display-engine" },
	{ .compatible = "allwinner,sun5i-a13-display-engine" },
	{ .compatible = "allwinner,sun6i-a31-display-engine" },
	{ .compatible = "allwinner,sun6i-a31s-display-engine" },
	{ .compatible = "allwinner,sun7i-a20-display-engine" },
	{ .compatible = "allwinner,sun8i-a23-display-engine" },
	{ .compatible = "allwinner,sun8i-a33-display-engine" },
	{ .compatible = "allwinner,sun8i-a83t-display-engine" },
	{ .compatible = "allwinner,sun8i-h3-display-engine" },
	{ .compatible = "allwinner,sun8i-r40-display-engine" },
	{ .compatible = "allwinner,sun8i-v3s-display-engine" },
	{ .compatible = "allwinner,sun9i-a80-display-engine" },
	{ .compatible = "allwinner,sun50i-a64-display-engine" },
	{ .compatible = "allwinner,sun50i-h6-display-engine" },
	{ }
};
MODULE_DEVICE_TABLE(of, sun4i_drv_of_table);

static struct platform_driver sun4i_drv_platform_driver = {
	.probe		= sun4i_drv_probe,
	.remove		= sun4i_drv_remove,
	.driver		= {
		.name		= "sun4i-drm",
		.of_match_table	= sun4i_drv_of_table,
		.pm = &sun4i_drv_drm_pm_ops,
	},
};
module_platform_driver(sun4i_drv_platform_driver);

MODULE_AUTHOR("Boris Brezillon <boris.brezillon@free-electrons.com>");
MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A10 Display Engine DRM/KMS Driver");
MODULE_LICENSE("GPL");
