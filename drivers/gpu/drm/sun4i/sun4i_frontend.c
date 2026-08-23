// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 Free Electrons
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <drm/drm_device.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_plane.h>

#include "sun4i_drv.h"
#include "sun4i_frontend.h"
#include "sunxi_detab.h"

static const u32 sun4i_frontend_vert_coef[32] = {
	0x00004000, 0x000140ff, 0x00033ffe, 0x00043ffd,
	0x00063efc, 0xff083dfc, 0x000a3bfb, 0xff0d39fb,
	0xff0f37fb, 0xff1136fa, 0xfe1433fb, 0xfe1631fb,
	0xfd192ffb, 0xfd1c2cfb, 0xfd1f29fb, 0xfc2127fc,
	0xfc2424fc, 0xfc2721fc, 0xfb291ffd, 0xfb2c1cfd,
	0xfb2f19fd, 0xfb3116fe, 0xfb3314fe, 0xfa3611ff,
	0xfb370fff, 0xfb390dff, 0xfb3b0a00, 0xfc3d08ff,
	0xfc3e0600, 0xfd3f0400, 0xfe3f0300, 0xff400100,
};

static const u32 sun4i_frontend_horz_coef[64] = {
	0x40000000, 0x00000000, 0x40fe0000, 0x0000ff03,
	0x3ffd0000, 0x0000ff05, 0x3ffc0000, 0x0000ff06,
	0x3efb0000, 0x0000ff08, 0x3dfb0000, 0x0000ff09,
	0x3bfa0000, 0x0000fe0d, 0x39fa0000, 0x0000fe0f,
	0x38fa0000, 0x0000fe10, 0x36fa0000, 0x0000fe12,
	0x33fa0000, 0x0000fd16, 0x31fa0000, 0x0000fd18,
	0x2ffa0000, 0x0000fd1a, 0x2cfa0000, 0x0000fc1e,
	0x29fa0000, 0x0000fc21, 0x27fb0000, 0x0000fb23,
	0x24fb0000, 0x0000fb26, 0x21fb0000, 0x0000fb29,
	0x1ffc0000, 0x0000fa2b, 0x1cfc0000, 0x0000fa2e,
	0x19fd0000, 0x0000fa30, 0x16fd0000, 0x0000fa33,
	0x14fd0000, 0x0000fa35, 0x11fe0000, 0x0000fa37,
	0x0ffe0000, 0x0000fa39, 0x0dfe0000, 0x0000fa3b,
	0x0afe0000, 0x0000fa3e, 0x08ff0000, 0x0000fb3e,
	0x06ff0000, 0x0000fb40, 0x05ff0000, 0x0000fc40,
	0x03ff0000, 0x0000fd41, 0x01ff0000, 0x0000fe42,
};

/*
 * These coefficients are taken from the A33 BSP from Allwinner.
 *
 * The first three values of each row are coded as 13-bit signed fixed-point
 * numbers, with 10 bits for the fractional part. The fourth value is a
 * constant coded as a 14-bit signed fixed-point number with 4 bits for the
 * fractional part.
 *
 * The values in table order give the following colorspace translation:
 * G = 1.164 * Y - 0.391 * U - 0.813 * V + 135
 * R = 1.164 * Y + 1.596 * V - 222
 * B = 1.164 * Y + 2.018 * U + 276
 *
 * This seems to be a conversion from Y[16:235] UV[16:240] to RGB[0:255],
 * following the BT601 spec.
 */
// const u32 sunxi_bt601_yuv2rgb_coef[12] = {
// 	0x000004a7, 0x00001e6f, 0x00001cbf, 0x00000877,
// 	0x000004a7, 0x00000000, 0x00000662, 0x00003211,
// 	0x000004a7, 0x00000812, 0x00000000, 0x00002eb1,
// };

const u32 sunxi_bt601_yuv2rgb_coef[12] = {
	0x04a8,0x1e70,0x1cbf,0x0878,0x04a8,0x0000,0x0662,0x3211,0x04a8,0x0812,0x0000,0x2eb1
};
EXPORT_SYMBOL(sunxi_bt601_yuv2rgb_coef);

typedef struct _SCAL_SCAN_MOD
{
    __u8    field;    //0:frame scan; 1:field scan
    __u8    bottom;      //0:top field; 1:bottom field
}__scal_scan_mod_t;
typedef struct __SCAL_SRC_SIZE
{
    __u32   src_width;
	  __u32   src_height;
    __u32   x_off;
    __u32   y_off;
    __u32   scal_width;
    __u32   scal_height;
}__scal_src_size_t;

typedef struct __SCAL_SRC_TYPE
{
    __u8    sample_method; //for yuv420, 0: uv_hphase=-0.25, uv_vphase=-0.25; other : uv_hphase = 0, uv_vphase = -0.25
    __u8    byte_seq;  //0:D0D1D2D3; 1:D3D2D1D0
    __u8    mod;       //0:plannar; 1: interleaved; 2: plannar uv combined; 4: plannar mb; 6: uv combined mb
    __u8    fmt;       //0:yuv444; 1: yuv422; 2: yuv420; 3:yuv411; 4: csi rgb; 5:rgb888
    __u8    ps;        //
}__scal_src_type_t;
typedef struct __SCAL_OUT_SIZE
{
    __u32   width;
    __u32   height;  //when ouput interlace enable,  the height is the 2x height of scale, for example, ouput is 480i, this value is 480
    __u32   x_off;
    __u32   y_off;
    __u32   fb_width;
    __u32   fb_height;
}__scal_out_size_t;

typedef struct __SCAL_OUT_TYPE
{
    __u8    byte_seq;  //0:D0D1D2D3; 1:D3D2D1D0
    __u8    fmt;       //0:plannar rgb; 1: argb(byte0,byte1, byte2, byte3); 2:bgra; 4:yuv444; 5:yuv420; 6:yuv422; 7:yuv411
}__scal_out_type_t;


typedef enum __SCAL_INFMT
{
	DE_SCAL_INYUV444=0,
	DE_SCAL_INYUV422, 
	DE_SCAL_INYUV420, 
	DE_SCAL_INYUV411, 
	DE_SCAL_INCSIRGB, 
	DE_SCAL_INRGB888
}__scal_infmt_t;

typedef enum __SCAL_OUTFMT
{
	DE_SCAL_OUTPRGB888=0, 
	DE_SCAL_OUTI0RGB888, 
	DE_SCAL_OUTI1RGB888,
	DE_SCAL_OUTPYUV444=4, 
	DE_SCAL_OUTPYUV420, 
	DE_SCAL_OUTPYUV422, 
	DE_SCAL_OUTPYUV411 
}__scal_outfmt_t;
typedef enum __SCAL_INMODE
{
	DE_SCAL_PLANNAR=0, 
	DE_SCAL_INTER_LEAVED, 
	DE_SCAL_UVCOMBINED, 
	DE_SCAL_PLANNARMB=4, 
	DE_SCAL_UVCOMBINEDMB=6
}__scal_inmode_t;

#define D0D1D2D3 0
#define D3D2D1D0 1


static int DE_SCAL_Set_Scaling_Coef(__u8 sel, __scal_scan_mod_t *in_scan, __scal_src_size_t *in_size,
                               __scal_src_type_t *in_type, __scal_scan_mod_t *out_scan, 
                               __scal_out_size_t *out_size, __scal_out_type_t *out_type, __u8 smth_mode,struct sun4i_frontend *frontend)  
{
    __s32 in_w0, in_h0, in_w1, in_h1, out_w0, out_h0, out_w1, out_h1;
    __s32 ch0h_smth_level, ch0v_smth_level, ch1h_smth_level, ch1v_smth_level;
    __u32 int_part, float_part;
    __u32 zoom0_size, zoom1_size, zoom2_size, zoom3_size, zoom4_size, zoom5_size, al1_size;
    __u32 ch0h_sc, ch0v_sc, ch1h_sc, ch1v_sc;
    __u32 ch0v_fir_coef_addr, ch0h_fir_coef_addr, ch1v_fir_coef_addr, ch1h_fir_coef_addr;
    __u32 ch0v_fir_coef_ofst, ch0h_fir_coef_ofst, ch1v_fir_coef_ofst, ch1h_fir_coef_ofst;
    __s32 fir_ofst_tmp;
    __u32 i;

	int ret;
	uint32_t val;

    __u32 loop_count = 0;
    in_w0 = in_size->scal_width;
    in_h0 = in_size->scal_height;

    out_w0 = out_size->width;
    out_h0 = out_size->height;

    zoom0_size = 1;
    zoom1_size = 8;
    zoom2_size = 4;
    zoom3_size = 1;
    zoom4_size = 1;
    zoom5_size = 1;
    al1_size = zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size + zoom5_size;
    
    if((in_type->mod == DE_SCAL_INTER_LEAVED) && (in_type->fmt == DE_SCAL_INYUV422))
    {
        in_w0 &=0xfffffffe;
    }
    
    //channel 1,2 size 
    if((in_type->fmt == DE_SCAL_INYUV420) || (in_type->fmt == DE_SCAL_INYUV422))
    {
        in_w1 = (in_w0 + 0x1)>>0x1;
    }
    else if(in_type->fmt == DE_SCAL_INYUV411)
    {
        in_w1 = (in_w0 + 0x3)>>0x2;
    }
    else
    {
        in_w1 = in_w0;
    }
    if((in_type->fmt == DE_SCAL_INYUV420) || (in_type->fmt == DE_SCAL_INCSIRGB))
    {
        in_h1 = (in_h0 + 0x1)>>0x1;
    }
    else
    {
        in_h1 = in_h0;
    }
    if((out_type->fmt == DE_SCAL_OUTPYUV420) || (out_type->fmt == DE_SCAL_OUTPYUV422))
    {
        out_w1 = (out_w0 + 0x1)>>0x1;
    }
    else if(out_type->fmt == DE_SCAL_OUTPYUV411)
    {
        out_w1 = (out_w0 + 0x3)>>0x2;
    }
    else
    {
        out_w1 = out_w0;
    }
    if(out_type->fmt == DE_SCAL_OUTPYUV420)
    {
        out_h1 = (out_h0+ 0x1)>>0x1;
    }
    else
    {
        out_h1 = out_h0;
    }
    
    //added no-zero limited
    in_h0 = (in_h0!=0) ? in_h0 : 1;
	in_h1 = (in_h1!=0) ? in_h1 : 1;
	in_w0 = (in_w0!=0) ? in_w0 : 1;
	in_w1 = (in_w1!=0) ? in_w1 : 1;
	out_h0 = (out_h0!=0) ? out_h0 : 1;
	out_h1 = (out_h1!=0) ? out_h1 : 1;
	out_w0 = (out_w0!=0) ? out_w0 : 1;
	out_w1 = (out_w1!=0) ? out_w1 : 1;
	    
    //smooth level for channel 0,1 in vertical and horizontal direction
    ch0h_smth_level = (smth_mode&0x40)  ?  0 - (smth_mode&0x3f) : smth_mode&0x3f;
    ch0v_smth_level = ch0h_smth_level;
    if((smth_mode>>7) &0x01)  
    {
      ch0v_smth_level = (smth_mode&0x4000) ? 0 - ((smth_mode&0x3f00)>>8) : ((smth_mode&0x3f00)>>8);
    }
    if((smth_mode>>31)&0x01)
    {
      ch1h_smth_level = (smth_mode&0x400000) ? 0 - ((smth_mode&0x3f0000)>>16) : ((smth_mode&0x3f0000)>>16);
      ch1v_smth_level = ch1h_smth_level;
      if((smth_mode >> 23)&0x1)
      {
        ch1v_smth_level = (smth_mode&0x40000000) ? 0 - ((smth_mode&0x3f000000)>>24) : ((smth_mode&0x3f000000)>>24);
      }
    }
    //
    ch0h_sc = (in_w0<<3)/out_w0;
    ch0v_sc = (in_h0<<(3-in_scan->field))/(out_h0);
    ch1h_sc = (in_w1<<3)/out_w1;
    ch1v_sc = (in_h1<<(3-in_scan->field))/(out_h1);

    //modify ch1 smooth level according to ratio to ch0
    if(((smth_mode>>31)&0x01)==0x0)
    {
      if(!ch1h_sc)
      {
        ch1h_smth_level = 0;
      }
      else
      {
        ch1h_smth_level = ch0h_smth_level>>(ch0h_sc/ch1h_sc);
      }

      if(!ch1v_sc)
      {
        ch1v_smth_level = 0;
      }
      else
      {
        ch1v_smth_level = ch0v_smth_level>>(ch0v_sc/ch1v_sc);
      }
    }
      
      //comput the fir coefficient offset in coefficient table
      int_part = ch0v_sc>>3;
      float_part = ch0v_sc & 0x7;
      ch0v_fir_coef_ofst = (int_part==0)  ? zoom0_size : 
                           (int_part==1)  ? zoom0_size + float_part :
                           (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                           (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                           (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                           zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
      int_part = ch0h_sc>>3;
      float_part = ch0h_sc & 0x7;
      ch0h_fir_coef_ofst = (int_part==0)  ? zoom0_size : 
                           (int_part==1)  ? zoom0_size + float_part :
                           (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                           (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                           (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                           zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
      int_part = ch1v_sc>>3;
      float_part = ch1v_sc & 0x7;
      ch1v_fir_coef_ofst = (int_part==0)  ? zoom0_size : 
                           (int_part==1)  ? zoom0_size + float_part :
                           (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                           (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                           (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                           zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
      int_part = ch1h_sc>>3;
      float_part = ch1h_sc & 0x7;
      ch1h_fir_coef_ofst =  (int_part==0)  ? zoom0_size : 
                            (int_part==1)  ? zoom0_size + float_part :
                            (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                            (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                            (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                            zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
    //added smooth level for each channel in horizontal and vertical direction
    fir_ofst_tmp = ch0v_fir_coef_ofst + ch0v_smth_level;
    ch0v_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    fir_ofst_tmp = ch0h_fir_coef_ofst + ch0h_smth_level;
    ch0h_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    fir_ofst_tmp = ch1v_fir_coef_ofst + ch1v_smth_level;
    ch1v_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    fir_ofst_tmp = ch1h_fir_coef_ofst + ch1h_smth_level;
    ch1h_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    //modify coefficient offset
    ch0v_fir_coef_ofst = (ch0v_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch0v_fir_coef_ofst;
    ch1v_fir_coef_ofst = (ch1v_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch1v_fir_coef_ofst;
    ch0h_fir_coef_ofst = (ch0h_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch0h_fir_coef_ofst; 
    ch1h_fir_coef_ofst = (ch1h_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch1h_fir_coef_ofst;                                           
    
    /* for  single buffer */
    ch0v_fir_coef_addr = (ch0v_fir_coef_ofst<<7);
    ch0h_fir_coef_addr = (ch0h_fir_coef_ofst<<7);
    ch1v_fir_coef_addr = (ch1v_fir_coef_ofst<<7);
    ch1h_fir_coef_addr = (ch1h_fir_coef_ofst<<7);
    // scal_dev[sel]->frm_ctrl.bits.coef_access_ctrl= 1;
    // while((scal_dev[sel]->status.bits.coef_access_status == 0) && (loop_count < 20)) {
    //     loop_count ++;
    // }

	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			SUN4I_FRONTEND_FRM_CTRL_COEF_ACCESS_CTRL,
			SUN4I_FRONTEND_FRM_CTRL_COEF_ACCESS_CTRL);
	/* Wait for coef access to be granted */
	ret = regmap_read_poll_timeout(frontend->regs,
						SUN4I_FRONTEND_STATUS_REG,
						val,
						val & SUN4I_FRONTEND_STATUS_COEF_ACCESS_STATUS,
						50, 500);
	if (ret) {
		printk(KERN_ERR "srgn: sun4i_frontend_scaler_init timeout waiting for coef access?!\n");
	}

    for(i=0; i<32; i++)
    {
    //   scal_dev[sel]->ch0_horzcoef0[i].dwval = fir_tab[(ch0h_fir_coef_addr>>2) + i];
    //   scal_dev[sel]->ch0_vertcoef[i].dwval  = fir_tab[(ch0v_fir_coef_addr>>2) + i];
    //   scal_dev[sel]->ch1_horzcoef0[i].dwval = fir_tab[(ch1h_fir_coef_addr>>2) + i];
    //   scal_dev[sel]->ch1_vertcoef[i].dwval  = fir_tab[(ch1v_fir_coef_addr>>2) + i];

		// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF0_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF0_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF1_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i + 1]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF1_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i + 1]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTCOEF_REG(i),
	// 		     sun4i_frontend_vert_coef[i]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTCOEF_REG(i),
	// 		     sun4i_frontend_vert_coef[i]);

		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF0_REG(i),
			     fir_tab[(ch0h_fir_coef_addr>>2) + i]);

		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTCOEF_REG(i),
			     fir_tab[(ch0v_fir_coef_addr>>2) + i]);
		
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF0_REG(i),
				 fir_tab[(ch1h_fir_coef_addr>>2) + i]);
		
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTCOEF_REG(i),
			     fir_tab[(ch1v_fir_coef_addr>>2) + i]);
		
		
    }

    // scal_dev[sel]->frm_ctrl.bits.coef_access_ctrl = 0x0;
	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			SUN4I_FRONTEND_FRM_CTRL_COEF_ACCESS_CTRL,
			0);

    return 0;
}
//*********************************************************************************************
// description      : set scaler scaling filter coefficients for video playback
// parameters       :
//                 sel <scaler select>
//                 in_scan <scale src data scan mode, if deinterlaceing open, the scan mode is progressive for scale>
//                 in_size <scale region define,  src size, offset, scal size>
//                 in_type <src data type>
//                 out_scan <scale output data scan mode>
//                 out_size <scale out size>
//                 out_type <output data format>
//                 smth_mode <scaler filter effect select>
//***********************************************************************************************                                        
static int DE_SCAL_Set_Scaling_Coef_for_video(__u8 sel, __scal_scan_mod_t *in_scan, __scal_src_size_t *in_size,
                               __scal_src_type_t *in_type, __scal_scan_mod_t *out_scan, 
                               __scal_out_size_t *out_size, __scal_out_type_t *out_type, __u32 smth_mode,struct sun4i_frontend *frontend)  
{
    __s32 in_w0, in_h0, in_w1, in_h1, out_w0, out_h0, out_w1, out_h1;
    __s32 ch0h_smth_level=0, ch0v_smth_level=0, ch1h_smth_level=0, ch1v_smth_level=0;
    __u32 int_part, float_part;
    __u32 zoom0_size, zoom1_size, zoom2_size, zoom3_size, zoom4_size, zoom5_size, al1_size;
    __u32 ch0h_sc, ch0v_sc, ch1h_sc, ch1v_sc;
    __u32 ch0v_fir_coef_addr, ch0h_fir_coef_addr, ch1v_fir_coef_addr, ch1h_fir_coef_addr;
    __u32 ch0v_fir_coef_ofst, ch0h_fir_coef_ofst, ch1v_fir_coef_ofst, ch1h_fir_coef_ofst;
    __s32 fir_ofst_tmp;
    __u32 i;

	int ret;
	uint32_t val;
    
    in_w0 = in_size->scal_width;
    in_h0 = in_size->scal_height;

    out_w0 = out_size->width;
    out_h0 = out_size->height;

    zoom0_size = 1;
    zoom1_size = 8;
    zoom2_size = 4;
    zoom3_size = 1;
    zoom4_size = 1;
    zoom5_size = 1;
    al1_size = zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size + zoom5_size;
    
    if((in_type->mod == DE_SCAL_INTER_LEAVED) && (in_type->fmt == DE_SCAL_INYUV422))
    {
        in_w0 &=0xfffffffe;
    }
    
    //channel 1,2 size 
    if((in_type->fmt == DE_SCAL_INYUV420) || (in_type->fmt == DE_SCAL_INYUV422))
    {
        in_w1 = (in_w0 + 0x1)>>0x1;
    }
    else if(in_type->fmt == DE_SCAL_INYUV411)
    {
        in_w1 = (in_w0 + 0x3)>>0x2;
    }
    else
    {
        in_w1 = in_w0;
    }
    if((in_type->fmt == DE_SCAL_INYUV420) || (in_type->fmt == DE_SCAL_INCSIRGB))
    {
        in_h1 = (in_h0 + 0x1)>>0x1;
    }
    else
    {
        in_h1 = in_h0;
    }
    if((out_type->fmt == DE_SCAL_OUTPYUV420) || (out_type->fmt == DE_SCAL_OUTPYUV422))
    {
        out_w1 = (out_w0 + 0x1)>>0x1;
    }
    else if(out_type->fmt == DE_SCAL_OUTPYUV411)
    {
        out_w1 = (out_w0 + 0x3)>>0x2;
    }
    else
    {
        out_w1 = out_w0;
    }
    if(out_type->fmt == DE_SCAL_OUTPYUV420)
    {
        out_h1 = (out_h0+ 0x1)>>0x1;
    }
    else
    {
        out_h1 = out_h0;
    }
    
    //added no-zero limited
    in_h0 = (in_h0!=0) ? in_h0 : 1;
	in_h1 = (in_h1!=0) ? in_h1 : 1;
	in_w0 = (in_w0!=0) ? in_w0 : 1;
	in_w1 = (in_w1!=0) ? in_w1 : 1;
	out_h0 = (out_h0!=0) ? out_h0 : 1;
	out_h1 = (out_h1!=0) ? out_h1 : 1;
	out_w0 = (out_w0!=0) ? out_w0 : 1;
	out_w1 = (out_w1!=0) ? out_w1 : 1;
	    
    //smooth level for channel 0,1 in vertical and horizontal direction
    ch0h_smth_level = (smth_mode&0x40)  ?  0 - (smth_mode&0x3f) : smth_mode&0x3f;
    ch0v_smth_level = ch0h_smth_level;
    if((smth_mode>>7) &0x01)  
    {
      ch0v_smth_level = (smth_mode&0x4000) ? 0 - ((smth_mode&0x3f00)>>8) : ((smth_mode&0x3f00)>>8);
    }
    if((smth_mode>>31)&0x01)
    {
      ch1h_smth_level = (smth_mode&0x400000) ? 0 - ((smth_mode&0x3f0000)>>16) : ((smth_mode&0x3f0000)>>16);
      ch1v_smth_level = ch1h_smth_level;
      if((smth_mode >> 23)&0x1)
      {
        ch1v_smth_level = (smth_mode&0x40000000) ? 0 - ((smth_mode&0x3f000000)>>24) : ((smth_mode&0x3f000000)>>24);
      }
    }
    //
    ch0h_sc = (in_w0<<3)/out_w0;
    ch0v_sc = (in_h0<<(3-in_scan->field))/(out_h0);
    ch1h_sc = (in_w1<<3)/out_w1;
    ch1v_sc = (in_h1<<(3-in_scan->field))/(out_h1);

    //modify ch1 smooth level according to ratio to ch0
    if(((smth_mode>>31)&0x01)==0x0)
    {
      if(!ch1h_sc)
      {
        ch1h_smth_level = 0;
      }
      else
      {
        ch1h_smth_level = ch0h_smth_level>>(ch0h_sc/ch1h_sc);
      }

      if(!ch1v_sc)
      {
        ch1v_smth_level = 0;
      }
      else
      {
        ch1v_smth_level = ch0v_smth_level>>(ch0v_sc/ch1v_sc);
      }
    }
      
      //comput the fir coefficient offset in coefficient table
      int_part = ch0v_sc>>3;
      float_part = ch0v_sc & 0x7;
      ch0v_fir_coef_ofst = (int_part==0)  ? 0 : 					//modify by vito 12-04-16
                           (int_part==1)  ? zoom0_size + float_part :
                           (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                           (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                           (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                           zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
      int_part = ch0h_sc>>3;
      float_part = ch0h_sc & 0x7;
      ch0h_fir_coef_ofst = (int_part==0)  ? 0 : 					//modify by vito 12-04-16
                           (int_part==1)  ? zoom0_size + float_part :
                           (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                           (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                           (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                           zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
      int_part = ch1v_sc>>3;
      float_part = ch1v_sc & 0x7;
      ch1v_fir_coef_ofst = (int_part==0)  ? 0 : 					//modify by vito 12-04-16
                           (int_part==1)  ? zoom0_size + float_part :
                           (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                           (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                           (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                           zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
      int_part = ch1h_sc>>3;
      float_part = ch1h_sc & 0x7;
      ch1h_fir_coef_ofst =  (int_part==0)  ? 0 : 					//modify by vito 12-04-16
                            (int_part==1)  ? zoom0_size + float_part :
                            (int_part==2)  ? zoom0_size + zoom1_size + (float_part>>1) : 
                            (int_part==3)  ? zoom0_size + zoom1_size + zoom2_size : 
                            (int_part==4)  ? zoom0_size + zoom1_size + zoom2_size +zoom3_size : 
                            zoom0_size + zoom1_size + zoom2_size + zoom3_size + zoom4_size;
    //added smooth level for each channel in horizontal and vertical direction
    fir_ofst_tmp = ch0v_fir_coef_ofst + ch0v_smth_level;
    ch0v_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    fir_ofst_tmp = ch0h_fir_coef_ofst + ch0h_smth_level;
    ch0h_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    fir_ofst_tmp = ch1v_fir_coef_ofst + ch1v_smth_level;
    ch1v_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    fir_ofst_tmp = ch1h_fir_coef_ofst + ch1h_smth_level;
    ch1h_fir_coef_ofst = (fir_ofst_tmp<0) ? 0 : fir_ofst_tmp;
    //modify coefficient offset
    ch0v_fir_coef_ofst = (ch0v_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch0v_fir_coef_ofst;
    ch1v_fir_coef_ofst = (ch1v_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch1v_fir_coef_ofst;
    ch0h_fir_coef_ofst = (ch0h_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch0h_fir_coef_ofst; 
    ch1h_fir_coef_ofst = (ch1h_fir_coef_ofst > (al1_size - 1)) ? (al1_size - 1) : ch1h_fir_coef_ofst;                                           
    
    //compute the fir coeficient address for each channel in horizontal and vertical direction
    ch0v_fir_coef_addr = (ch0v_fir_coef_ofst<<7);
    ch0h_fir_coef_addr = ((al1_size)<<7) + (ch0h_fir_coef_ofst<<8);
    ch1v_fir_coef_addr = (ch1v_fir_coef_ofst<<7);
    ch1h_fir_coef_addr = ((al1_size)<<7) + (ch1h_fir_coef_ofst<<8);

    for(i=0; i<32; i++)
    {
	//     scal_dev[sel]->ch0_horzcoef0[i].dwval = fir_tab_video[(ch0h_fir_coef_addr>>2) + 2*i];
	// 	  scal_dev[sel]->ch0_horzcoef1[i].dwval = fir_tab_video[(ch0h_fir_coef_addr>>2) + 2*i + 1];
    //   scal_dev[sel]->ch0_vertcoef[i].dwval  = fir_tab_video[(ch0v_fir_coef_addr>>2) + i];
	// 	  scal_dev[sel]->ch1_horzcoef0[i].dwval = fir_tab_video[(ch1h_fir_coef_addr>>2) + 2*i];
	// 	  scal_dev[sel]->ch1_horzcoef1[i].dwval = fir_tab_video[(ch1h_fir_coef_addr>>2) + 2*i + 1];
    //   scal_dev[sel]->ch1_vertcoef[i].dwval  = fir_tab_video[(ch1v_fir_coef_addr>>2) + i];

			// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF0_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF0_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF1_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i + 1]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF1_REG(i),
	// 		     sun4i_frontend_horz_coef[2 * i + 1]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTCOEF_REG(i),
	// 		     sun4i_frontend_vert_coef[i]);
	// 	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTCOEF_REG(i),
	// 		     sun4i_frontend_vert_coef[i]);

		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF0_REG(i),
			     fir_tab_video[(ch0h_fir_coef_addr>>2) + 2*i]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF1_REG(i),
				 fir_tab_video[(ch0h_fir_coef_addr>>2) + 2*i + 1]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTCOEF_REG(i),
			     fir_tab_video[(ch0v_fir_coef_addr>>2) + i]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF0_REG(i),
				 fir_tab_video[(ch1h_fir_coef_addr>>2) + 2*i]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF1_REG(i),
				 fir_tab_video[(ch1h_fir_coef_addr>>2) + 2*i + 1]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTCOEF_REG(i),
			     fir_tab_video[(ch1v_fir_coef_addr>>2) + i]);
    }

    // scal_dev[sel]->frm_ctrl.bits.coef_rdy_en = 0x1;
	regmap_write_bits(frontend->regs,
			SUN4I_FRONTEND_FRM_CTRL_REG,
			SUN4I_FRONTEND_FRM_CTRL_COEF_RDY,
			SUN4I_FRONTEND_FRM_CTRL_COEF_RDY);
			
      
    return 0;
}

static void sun4i_frontend_recalc_scale_coefs(struct sun4i_frontend *frontend,
					      struct drm_plane *plane,
					      u32 src_w, u32 src_h,
					      u32 dst_w, u32 dst_h)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = state->fb;
	const struct drm_format_info *format = fb->format;
	__scal_scan_mod_t src_scan = { 0 };
	__scal_scan_mod_t out_scan = { 0 };
	__scal_src_size_t src_size = { 0 };
	__scal_src_type_t src_type = { 0 };
	__scal_out_size_t out_size = { 0 };
	__scal_out_type_t out_type = { 0 };
	int ret;
	int i;

	/*
	 * RGB buffers already scale correctly with the generic coefficients.
	 * The broken path here is hardware-decoded YUV video, where ch1
	 * coefficients must be recomputed from the real source/destination
	 * geometry like the bare-metal DEFE code does.
	 */
	if (!format->is_yuv) {
		for (i = 0; i < 32; i++) {
			regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF0_REG(i),
				     sun4i_frontend_horz_coef[2 * i]);
			regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF0_REG(i),
				     sun4i_frontend_horz_coef[2 * i]);
			regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF1_REG(i),
				     sun4i_frontend_horz_coef[2 * i + 1]);
			regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF1_REG(i),
				     sun4i_frontend_horz_coef[2 * i + 1]);
			regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTCOEF_REG(i),
				     sun4i_frontend_vert_coef[i]);
			regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTCOEF_REG(i),
				     sun4i_frontend_vert_coef[i]);
		}

		if (frontend->data->has_coef_rdy)
			regmap_write_bits(frontend->regs,
					  SUN4I_FRONTEND_FRM_CTRL_REG,
					  SUN4I_FRONTEND_FRM_CTRL_COEF_RDY,
					  SUN4I_FRONTEND_FRM_CTRL_COEF_RDY);
		return;
	}

	src_size.src_width = src_w;
	src_size.src_height = src_h;
	src_size.scal_width = src_w;
	src_size.scal_height = src_h;

	if (drm_format_info_is_yuv_sampling_411(format))
		src_type.fmt = DE_SCAL_INYUV411;
	else if (drm_format_info_is_yuv_sampling_420(format))
		src_type.fmt = DE_SCAL_INYUV420;
	else if (drm_format_info_is_yuv_sampling_422(format))
		src_type.fmt = DE_SCAL_INYUV422;
	else
		src_type.fmt = DE_SCAL_INYUV444;

	switch (format->num_planes) {
	case 1:
		src_type.mod = DE_SCAL_INTER_LEAVED;
		break;
	case 2:
		src_type.mod = (fb->modifier == DRM_FORMAT_MOD_ALLWINNER_TILED) ?
			       DE_SCAL_UVCOMBINEDMB : DE_SCAL_UVCOMBINED;
		break;
	case 3:
	default:
		src_type.mod = (fb->modifier == DRM_FORMAT_MOD_ALLWINNER_TILED) ?
			       DE_SCAL_PLANNARMB : DE_SCAL_PLANNAR;
		break;
	}

	src_type.byte_seq = D0D1D2D3;
	src_type.sample_method = 0;
	src_type.ps = 0;

	out_type.byte_seq = D3D2D1D0;
	out_type.fmt = DE_SCAL_OUTI0RGB888;
	out_size.width = dst_w;
	out_size.height = dst_h;
	out_size.fb_width = dst_w;
	out_size.fb_height = dst_h;

	ret = DE_SCAL_Set_Scaling_Coef_for_video(0, &src_scan, &src_size,
						 &src_type, &out_scan,
						 &out_size, &out_type, 0,
						 frontend);
	if (ret)
		DRM_DEBUG_DRIVER("Frontend video coef recalc failed: %d\n", ret);

	if (frontend->data->has_coef_rdy)
		regmap_write_bits(frontend->regs,
				  SUN4I_FRONTEND_FRM_CTRL_REG,
				  SUN4I_FRONTEND_FRM_CTRL_COEF_RDY,
				  SUN4I_FRONTEND_FRM_CTRL_COEF_RDY);
}

static void sun4i_frontend_set_dynamic_phase(struct sun4i_frontend *frontend,
					     struct drm_plane *plane)
{
	struct drm_framebuffer *fb = plane->state->fb;
	const struct drm_format_info *format = fb->format;
	u32 ch0_hphase = 0;
	u32 ch0_vphase0 = 0;
	u32 ch0_vphase1 = 0;
	u32 ch1_hphase = 0;
	u32 ch1_vphase0 = 0;
	u32 ch1_vphase1 = 0;

	if (drm_format_info_is_yuv_sampling_420(format)) {
		ch1_hphase = 0xfc000;
		ch1_vphase0 = 0xfc000;
		ch1_vphase1 = 0xfc000;
	}

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZPHASE_REG, ch0_hphase);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZPHASE_REG, ch1_hphase);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTPHASE0_REG, ch0_vphase0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTPHASE1_REG, ch0_vphase1);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTPHASE0_REG, ch1_vphase0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTPHASE1_REG, ch1_vphase1);
}

static void sun4i_frontend_scaler_init(struct sun4i_frontend *frontend)
{
	int i;

	if (frontend->data->has_coef_access_ctrl)
		regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
				  SUN4I_FRONTEND_FRM_CTRL_COEF_ACCESS_CTRL,
				  SUN4I_FRONTEND_FRM_CTRL_COEF_ACCESS_CTRL);

	for (i = 0; i < 32; i++) {
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF0_REG(i),
			     sun4i_frontend_horz_coef[2 * i]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF0_REG(i),
			     sun4i_frontend_horz_coef[2 * i]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZCOEF1_REG(i),
			     sun4i_frontend_horz_coef[2 * i + 1]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZCOEF1_REG(i),
			     sun4i_frontend_horz_coef[2 * i + 1]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTCOEF_REG(i),
			     sun4i_frontend_vert_coef[i]);
		regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTCOEF_REG(i),
			     sun4i_frontend_vert_coef[i]);
	}

	if (frontend->data->has_coef_rdy)
		regmap_write_bits(frontend->regs,
				  SUN4I_FRONTEND_FRM_CTRL_REG,
				  SUN4I_FRONTEND_FRM_CTRL_COEF_RDY,
				  SUN4I_FRONTEND_FRM_CTRL_COEF_RDY);
}

int sun4i_frontend_init(struct sun4i_frontend *frontend)
{
	return pm_runtime_get_sync(frontend->dev);
}
EXPORT_SYMBOL(sun4i_frontend_init);

void sun4i_frontend_prepare_mb32_yuv(struct sun4i_frontend *frontend)
{
	/* Match the YUV420 chroma phase used by the normal DRM plane path. */
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZPHASE_REG, 0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZPHASE_REG, 0xfc000);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTPHASE0_REG, 0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTPHASE1_REG, 0);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTPHASE0_REG, 0xfc000);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTPHASE1_REG, 0xfc000);
	sun4i_frontend_scaler_init(frontend);
}
EXPORT_SYMBOL(sun4i_frontend_prepare_mb32_yuv);

void sun4i_frontend_exit(struct sun4i_frontend *frontend)
{
	pm_runtime_put(frontend->dev);
}
EXPORT_SYMBOL(sun4i_frontend_exit);

static bool sun4i_frontend_format_chroma_requires_swap(uint32_t fmt)
{
	switch (fmt) {
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YVU444:
		return true;

	default:
		return false;
	}
}

static bool sun4i_frontend_format_supports_tiling(uint32_t fmt)
{
	switch (fmt) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YVU411:
		return true;

	default:
		return false;
	}
}

void sun4i_frontend_update_buffer(struct sun4i_frontend *frontend,
				  struct drm_plane *plane)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = state->fb;
	unsigned int strides[3] = {};

	dma_addr_t paddr;
	bool swap;

	if (fb->modifier == DRM_FORMAT_MOD_ALLWINNER_TILED) {
		unsigned int width = state->src_w >> 16;
		unsigned int offset;

		strides[0] = SUN4I_FRONTEND_LINESTRD_TILED(fb->pitches[0]);

		/*
		 * The X1 offset is the offset to the bottom-right point in the
		 * end tile, which is the final pixel (at offset width - 1)
		 * within the end tile (with a 32-byte mask).
		 */
		offset = (width - 1) & (32 - 1);

		regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF0_REG,
			     SUN4I_FRONTEND_TB_OFF_X1(offset));

		if (fb->format->num_planes > 1) {
			strides[1] =
				SUN4I_FRONTEND_LINESTRD_TILED(fb->pitches[1]);

			regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF1_REG,
				     SUN4I_FRONTEND_TB_OFF_X1(offset));
		}

		if (fb->format->num_planes > 2) {
			strides[2] =
				SUN4I_FRONTEND_LINESTRD_TILED(fb->pitches[2]);

			regmap_write(frontend->regs, SUN4I_FRONTEND_TB_OFF2_REG,
				     SUN4I_FRONTEND_TB_OFF_X1(offset));
		}
	} else {
		strides[0] = fb->pitches[0];

		if (fb->format->num_planes > 1)
			strides[1] = fb->pitches[1];

		if (fb->format->num_planes > 2)
			strides[2] = fb->pitches[2];
	}

	/* Set the line width */
	DRM_DEBUG_DRIVER("Frontend stride: %d bytes\n", fb->pitches[0]);
	regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD0_REG,
		     strides[0]);

	if (fb->format->num_planes > 1)
		regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD1_REG,
			     strides[1]);

	if (fb->format->num_planes > 2)
		regmap_write(frontend->regs, SUN4I_FRONTEND_LINESTRD2_REG,
			     strides[2]);

	/* Some planar formats require chroma channel swapping by hand. */
	swap = sun4i_frontend_format_chroma_requires_swap(fb->format->format);

	/* Set the physical address of the buffer in memory */
	paddr = drm_fb_cma_get_gem_addr(fb, state, 0);
	paddr -= PHYS_OFFSET;
	DRM_DEBUG_DRIVER("Setting buffer #0 address to %pad\n", &paddr);
	regmap_write(frontend->regs, SUN4I_FRONTEND_BUF_ADDR0_REG, paddr);

	if (fb->format->num_planes > 1) {
		paddr = drm_fb_cma_get_gem_addr(fb, state, swap ? 2 : 1);
		paddr -= PHYS_OFFSET;
		DRM_DEBUG_DRIVER("Setting buffer #1 address to %pad\n", &paddr);
		regmap_write(frontend->regs, SUN4I_FRONTEND_BUF_ADDR1_REG,
			     paddr);
	}

	if (fb->format->num_planes > 2) {
		paddr = drm_fb_cma_get_gem_addr(fb, state, swap ? 1 : 2);
		paddr -= PHYS_OFFSET;
		DRM_DEBUG_DRIVER("Setting buffer #2 address to %pad\n", &paddr);
		regmap_write(frontend->regs, SUN4I_FRONTEND_BUF_ADDR2_REG,
			     paddr);
	}
}
EXPORT_SYMBOL(sun4i_frontend_update_buffer);

static int
sun4i_frontend_drm_format_to_input_fmt(const struct drm_format_info *format,
				       u32 *val)
{
	if (!format->is_yuv)
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_RGB;
	else if (drm_format_info_is_yuv_sampling_411(format))
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_YUV411;
	else if (drm_format_info_is_yuv_sampling_420(format))
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_YUV420;
	else if (drm_format_info_is_yuv_sampling_422(format))
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_YUV422;
	else if (drm_format_info_is_yuv_sampling_444(format))
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_FMT_YUV444;
	else
		return -EINVAL;

	return 0;
}

static int
sun4i_frontend_drm_format_to_input_mode(const struct drm_format_info *format,
					uint64_t modifier, u32 *val)
{
	bool tiled = (modifier == DRM_FORMAT_MOD_ALLWINNER_TILED);

	switch (format->num_planes) {
	case 1:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_PACKED;
		return 0;

	case 2:
		*val = tiled ? SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_MB32_SEMIPLANAR
			     : SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_SEMIPLANAR;
		return 0;

	case 3:
		*val = tiled ? SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_MB32_PLANAR
			     : SUN4I_FRONTEND_INPUT_FMT_DATA_MOD_PLANAR;
		return 0;

	default:
		return -EINVAL;
	}
}

static int
sun4i_frontend_drm_format_to_input_sequence(const struct drm_format_info *format,
					    u32 *val)
{
	/* Planar formats have an explicit input sequence. */
	if (drm_format_info_is_yuv_planar(format)) {
		*val = 0;
		return 0;
	}

	switch (format->format) {
	case DRM_FORMAT_BGRX8888:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_BGRX;
		return 0;

	case DRM_FORMAT_NV12:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_UV;
		return 0;

	case DRM_FORMAT_NV16:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_UV;
		return 0;

	case DRM_FORMAT_NV21:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_VU;
		return 0;

	case DRM_FORMAT_NV61:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_VU;
		return 0;

	case DRM_FORMAT_UYVY:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_UYVY;
		return 0;

	case DRM_FORMAT_VYUY:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_VYUY;
		return 0;

	case DRM_FORMAT_XRGB8888:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_XRGB;
		return 0;

	case DRM_FORMAT_YUYV:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_YUYV;
		return 0;

	case DRM_FORMAT_YVYU:
		*val = SUN4I_FRONTEND_INPUT_FMT_DATA_PS_YVYU;
		return 0;

	default:
		return -EINVAL;
	}
}

static int sun4i_frontend_drm_format_to_output_fmt(uint32_t fmt, u32 *val)
{
	switch (fmt) {
	case DRM_FORMAT_BGRX8888:
		*val = SUN4I_FRONTEND_OUTPUT_FMT_DATA_FMT_BGRX8888;
		return 0;

	case DRM_FORMAT_XRGB8888:
		*val = SUN4I_FRONTEND_OUTPUT_FMT_DATA_FMT_XRGB8888;
		return 0;

	default:
		return -EINVAL;
	}
}

static const uint32_t sun4i_frontend_formats[] = {
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV16,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV61,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_YUV411,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV444,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVU411,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_YVU422,
	DRM_FORMAT_YVU444,
	DRM_FORMAT_YVYU,
};

bool sun4i_frontend_format_is_supported(uint32_t fmt, uint64_t modifier)
{
	unsigned int i;

	if (modifier == DRM_FORMAT_MOD_ALLWINNER_TILED)
		return sun4i_frontend_format_supports_tiling(fmt);
	else if (modifier != DRM_FORMAT_MOD_LINEAR)
		return false;

	for (i = 0; i < ARRAY_SIZE(sun4i_frontend_formats); i++)
		if (sun4i_frontend_formats[i] == fmt)
			return true;

	return false;
}
EXPORT_SYMBOL(sun4i_frontend_format_is_supported);


int sun4i_frontend_update_formats(struct sun4i_frontend *frontend,
				  struct drm_plane *plane, uint32_t out_fmt)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = state->fb;
	const struct drm_format_info *format = fb->format;
	uint64_t modifier = fb->modifier;
	u32 out_fmt_val;
	u32 in_fmt_val, in_mod_val, in_ps_val;
	u32 in_byte_seq_val = 0;
	unsigned int i;
	u32 bypass;
	int ret;

	// u32 in_br_swap = 0;
	// u32 out_br_swap = 0;
	// u32 in_csc_mode = csc_YUV;

	// u32 csc_coef_addr = (((in_csc_mode&0x3)<<7) + ((in_csc_mode&0x3)<<6));

	// printk(KERN_INFO "srgn: sun4i_frontend_update_formats csc_coef_addr=0x%x\n", csc_coef_addr);



	ret = sun4i_frontend_drm_format_to_input_fmt(format, &in_fmt_val);
	if (ret) {
		DRM_DEBUG_DRIVER("Invalid input format\n");
		return ret;
	}

	ret = sun4i_frontend_drm_format_to_input_mode(format, modifier,
						      &in_mod_val);
	if (ret) {
		DRM_DEBUG_DRIVER("Invalid input mode\n");
		return ret;
	}

	ret = sun4i_frontend_drm_format_to_input_sequence(format, &in_ps_val);
	if (ret) {
		DRM_DEBUG_DRIVER("Invalid pixel sequence\n");
		return ret;
	}

	ret = sun4i_frontend_drm_format_to_output_fmt(out_fmt, &out_fmt_val);
	if (ret) {
		DRM_DEBUG_DRIVER("Invalid output format\n");
		return ret;
	}

	switch (format->format) {
	case DRM_FORMAT_BGRX8888:
		in_byte_seq_val = SUN4I_FRONTEND_INPUT_FMT_BYTE_SEQ_P3210;
		break;

	case DRM_FORMAT_XRGB8888:
		in_byte_seq_val = SUN4I_FRONTEND_INPUT_FMT_BYTE_SEQ_P0123;
		break;

	default:
		in_byte_seq_val = 0;
		break;
	}

	/*
	 * I have no idea what this does exactly, but it seems to be
	 * related to the scaler FIR filter phase parameters.
	 */
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZPHASE_REG,
		     frontend->data->ch_phase[0].horzphase);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZPHASE_REG,
		     frontend->data->ch_phase[1].horzphase);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTPHASE0_REG,
		     frontend->data->ch_phase[0].vertphase[0]);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTPHASE0_REG,
		     frontend->data->ch_phase[1].vertphase[0]);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTPHASE1_REG,
		     frontend->data->ch_phase[0].vertphase[1]);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTPHASE1_REG,
		     frontend->data->ch_phase[1].vertphase[1]);

	/*
	 * Checking the input format is sufficient since we currently only
	 * support RGB output formats to the backend. If YUV output formats
	 * ever get supported, an YUV input and output would require bypassing
	 * the CSC engine too.
	 */
	if (format->is_yuv) {
		/* Setup the CSC engine for YUV to RGB conversion. */
		printk(KERN_INFO "srgn: sun4i_frontend_update_formats disable bypass and set csc coef\n");
		bypass = 0;

		for (i = 0; i < ARRAY_SIZE(sunxi_bt601_yuv2rgb_coef); i++)
			regmap_write(frontend->regs,
				     SUN4I_FRONTEND_CSC_COEF_REG(i),
				     sunxi_bt601_yuv2rgb_coef[i]);

	} else {
		bypass = SUN4I_FRONTEND_BYPASS_CSC_EN;
	}

	regmap_update_bits(frontend->regs, SUN4I_FRONTEND_BYPASS_REG,
			   SUN4I_FRONTEND_BYPASS_CSC_EN, bypass);

	regmap_write(frontend->regs, SUN4I_FRONTEND_INPUT_FMT_REG,
		     in_byte_seq_val | in_mod_val | in_fmt_val | in_ps_val);

	/*
	 * TODO: It look like the A31 and A80 at least will need the
	 * bit 7 (ALPHA_EN) enabled when using a format with alpha (so
	 * ARGB8888).
	 */
	regmap_write(frontend->regs, SUN4I_FRONTEND_OUTPUT_FMT_REG,
		     out_fmt_val);


	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			SUN4I_FRONTEND_FRM_CTRL_REG_RDY,
			SUN4I_FRONTEND_FRM_CTRL_REG_RDY);

	return 0;
}
EXPORT_SYMBOL(sun4i_frontend_update_formats);

void sun4i_frontend_update_coord(struct sun4i_frontend *frontend,
				 struct drm_plane *plane)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = state->fb;
	uint32_t luma_width, luma_height;
	uint32_t chroma_width, chroma_height;
	uint32_t chroma_out_width, chroma_out_height;

	/* Set height and width */
	DRM_DEBUG_DRIVER("Frontend size W: %u H: %u\n",
			 state->crtc_w, state->crtc_h);

	luma_width = state->src_w >> 16;
	luma_height = state->src_h >> 16;

	chroma_width = DIV_ROUND_UP(luma_width, fb->format->hsub);
	chroma_height = DIV_ROUND_UP(luma_height, fb->format->vsub);
	chroma_out_width = DIV_ROUND_UP(state->crtc_w, fb->format->hsub);
	chroma_out_height = DIV_ROUND_UP(state->crtc_h, fb->format->vsub);

	sun4i_frontend_set_dynamic_phase(frontend, plane);
	sun4i_frontend_recalc_scale_coefs(frontend, plane,
					  luma_width, luma_height,
					  state->crtc_w, state->crtc_h);

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_INSIZE_REG,
		     SUN4I_FRONTEND_INSIZE(luma_height, luma_width));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_INSIZE_REG,
		     SUN4I_FRONTEND_INSIZE(chroma_height, chroma_width));

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_OUTSIZE_REG,
		     SUN4I_FRONTEND_OUTSIZE(state->crtc_h, state->crtc_w));
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_OUTSIZE_REG,
		     SUN4I_FRONTEND_OUTSIZE(chroma_out_height, chroma_out_width));

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_HORZFACT_REG,
		     (luma_width << 16) / state->crtc_w);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_HORZFACT_REG,
		     (chroma_width << 16) / chroma_out_width);

	regmap_write(frontend->regs, SUN4I_FRONTEND_CH0_VERTFACT_REG,
		     (luma_height << 16) / state->crtc_h);
	regmap_write(frontend->regs, SUN4I_FRONTEND_CH1_VERTFACT_REG,
		     (chroma_height << 16) / chroma_out_height);

	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			  SUN4I_FRONTEND_FRM_CTRL_REG_RDY,
			  SUN4I_FRONTEND_FRM_CTRL_REG_RDY);
}
EXPORT_SYMBOL(sun4i_frontend_update_coord);

int sun4i_frontend_enable(struct sun4i_frontend *frontend)
{
	regmap_write_bits(frontend->regs, SUN4I_FRONTEND_FRM_CTRL_REG,
			  SUN4I_FRONTEND_FRM_CTRL_FRM_START,
			  SUN4I_FRONTEND_FRM_CTRL_FRM_START);

	return 0;
}
EXPORT_SYMBOL(sun4i_frontend_enable);

static struct regmap_config sun4i_frontend_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0x0a14,
};

static int sun4i_frontend_bind(struct device *dev, struct device *master,
			 void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sun4i_frontend *frontend;
	struct drm_device *drm = data;
	struct sun4i_drv *drv = drm->dev_private;
	struct resource *res;
	void __iomem *regs;

	frontend = devm_kzalloc(dev, sizeof(*frontend), GFP_KERNEL);
	if (!frontend)
		return -ENOMEM;

	dev_set_drvdata(dev, frontend);
	frontend->dev = dev;
	frontend->node = dev->of_node;

	frontend->data = of_device_get_match_data(dev);
	if (!frontend->data)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	frontend->regs = devm_regmap_init_mmio(dev, regs,
					       &sun4i_frontend_regmap_config);
	if (IS_ERR(frontend->regs)) {
		dev_err(dev, "Couldn't create the frontend regmap\n");
		return PTR_ERR(frontend->regs);
	}

	frontend->reset = devm_reset_control_get(dev, NULL);
	if (IS_ERR(frontend->reset)) {
		dev_err(dev, "Couldn't get our reset line\n");
		return PTR_ERR(frontend->reset);
	}

	frontend->bus_clk = devm_clk_get(dev, "ahb");
	if (IS_ERR(frontend->bus_clk)) {
		dev_err(dev, "Couldn't get our bus clock\n");
		return PTR_ERR(frontend->bus_clk);
	}

	frontend->mod_clk = devm_clk_get(dev, "mod");
	if (IS_ERR(frontend->mod_clk)) {
		dev_err(dev, "Couldn't get our mod clock\n");
		return PTR_ERR(frontend->mod_clk);
	}

	frontend->ram_clk = devm_clk_get(dev, "ram");
	if (IS_ERR(frontend->ram_clk)) {
		dev_err(dev, "Couldn't get our ram clock\n");
		return PTR_ERR(frontend->ram_clk);
	}

	list_add_tail(&frontend->list, &drv->frontend_list);
	pm_runtime_enable(dev);

	return 0;
}

static void sun4i_frontend_unbind(struct device *dev, struct device *master,
			    void *data)
{
	struct sun4i_frontend *frontend = dev_get_drvdata(dev);

	list_del(&frontend->list);
	pm_runtime_force_suspend(dev);
}

static const struct component_ops sun4i_frontend_ops = {
	.bind	= sun4i_frontend_bind,
	.unbind	= sun4i_frontend_unbind,
};

static int sun4i_frontend_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &sun4i_frontend_ops);
}

static int sun4i_frontend_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &sun4i_frontend_ops);

	return 0;
}

static int sun4i_frontend_runtime_resume(struct device *dev)
{
	struct sun4i_frontend *frontend = dev_get_drvdata(dev);
	int ret;

	clk_set_rate(frontend->mod_clk, 300000000);

	clk_prepare_enable(frontend->bus_clk);
	clk_prepare_enable(frontend->mod_clk);
	clk_prepare_enable(frontend->ram_clk);

	ret = reset_control_reset(frontend->reset);
	if (ret) {
		dev_err(dev, "Couldn't reset our device\n");
		return ret;
	}

	regmap_update_bits(frontend->regs, SUN4I_FRONTEND_EN_REG,
			   SUN4I_FRONTEND_EN_EN,
			   SUN4I_FRONTEND_EN_EN);

	printk(KERN_INFO "srgn: sun4i_frontend_runtime_resume call sun4i_frontend_scaler_init\n");

	regmap_update_bits(frontend->regs, SUN4I_FRONTEND_EN_REG,
			   SUN4I_FRONTEND_EN_BIST,
			   0);

	sun4i_frontend_scaler_init(frontend);

	return 0;
}

static int sun4i_frontend_runtime_suspend(struct device *dev)
{
	struct sun4i_frontend *frontend = dev_get_drvdata(dev);

	clk_disable_unprepare(frontend->ram_clk);
	clk_disable_unprepare(frontend->mod_clk);
	clk_disable_unprepare(frontend->bus_clk);

	reset_control_assert(frontend->reset);

	return 0;
}

static const struct dev_pm_ops sun4i_frontend_pm_ops = {
	.runtime_resume		= sun4i_frontend_runtime_resume,
	.runtime_suspend	= sun4i_frontend_runtime_suspend,
};

static const struct sun4i_frontend_data sun4i_a10_frontend = {
	.ch_phase		= {
		{
			.horzphase = 0,
			.vertphase = { 0, 0 },
		},
		{
			.horzphase = 0xfc000,
			.vertphase = { 0xfc000, 0xfc000 },
		},
	},
	.has_coef_rdy		= true,
	.has_coef_access_ctrl	= true,
};

static const struct sun4i_frontend_data sun8i_a33_frontend = {
	.ch_phase		= {
		{
			.horzphase = 0x400,
			.vertphase = { 0x400, 0x400 },
		},
		{
			.horzphase = 0x400,
			.vertphase = { 0x400, 0x400 },
		},
	},
	.has_coef_access_ctrl	= true,
};

const struct of_device_id sun4i_frontend_of_table[] = {
	{
		.compatible = "allwinner,sun4i-a10-display-frontend",
		.data = &sun4i_a10_frontend
	},
	{
		.compatible = "allwinner,sun7i-a20-display-frontend",
		.data = &sun4i_a10_frontend
	},
	{
		.compatible = "allwinner,sun8i-a23-display-frontend",
		.data = &sun8i_a33_frontend
	},
	{
		.compatible = "allwinner,sun8i-a33-display-frontend",
		.data = &sun8i_a33_frontend
	},
	{ }
};
EXPORT_SYMBOL(sun4i_frontend_of_table);
MODULE_DEVICE_TABLE(of, sun4i_frontend_of_table);

static struct platform_driver sun4i_frontend_driver = {
	.probe		= sun4i_frontend_probe,
	.remove		= sun4i_frontend_remove,
	.driver		= {
		.name		= "sun4i-frontend",
		.of_match_table	= sun4i_frontend_of_table,
		.pm		= &sun4i_frontend_pm_ops,
	},
};
module_platform_driver(sun4i_frontend_driver);

MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A10 Display Engine Frontend Driver");
MODULE_LICENSE("GPL");

