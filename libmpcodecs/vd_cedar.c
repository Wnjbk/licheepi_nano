/* Cedar H.264 decoder adapter for the F1C200S DRM video output. */

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "libmpcodecs/vd_internal.h"
#include "libmpcodecs/img_format.h"
#include <memoryAdapter.h>
#include <vdecoder.h>

static const vd_info_t info = {
    "Allwinner Cedar H.264 decoder",
    "cedar",
    "F1C200S",
    "F1C200S",
    "CedarX"
};

typedef struct {
    VideoDecoder *decoder;
    struct ScMemOpsS *memops;
    int configured;
    int avcc_nal_length_size;
    int64_t fallback_pts;
} vd_cedar_ctx;

/* One MPlayer instance owns the Cedar display queue. */
static VideoDecoder *cedar_display_decoder;
static int cedar_plugins_loaded;

void vd_cedar_return_picture(void *picture)
{
    if (cedar_display_decoder && picture)
        ReturnPicture(cedar_display_decoder, picture);
}

LIBVD_EXTERN(cedar)

static int control(sh_video_t *sh, int cmd, void *arg, ...)
{
    (void)sh;
    (void)arg;
    if (cmd == VDCTRL_QUERY_FORMAT)
        return *((int *)arg) == IMGFMT_YV12;
    return CONTROL_UNKNOWN;
}

static int init(sh_video_t *sh)
{
    vd_cedar_ctx *ctx;
    VideoStreamInfo info;
    VConfig config;

    if (sh->disp_w <= 0 || sh->disp_h <= 0) {
        mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[cedar] invalid stream geometry\n");
        return 0;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return 0;
    sh->context = ctx;

    ctx->memops = MemAdapterGetOpsS();
    if (!ctx->memops || CdcMemOpen(ctx->memops) != 0)
        goto fail;

    if (!cedar_plugins_loaded) {
        AddVDPlugin();
        cedar_plugins_loaded = 1;
    }

    ctx->decoder = CreateVideoDecoder();
    if (!ctx->decoder)
        goto fail;

    memset(&info, 0, sizeof(info));
    memset(&config, 0, sizeof(config));
    info.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
    info.nWidth = sh->disp_w;
    info.nHeight = sh->disp_h;
    info.nFrameRate = sh->fps > 0 ? sh->fps : 30;
    info.nFrameDuration = 1000000 / info.nFrameRate;
    info.nAspectRatio = 1000;
    /* The first candidate accepts Annex-B H.264 access units. */
    info.pCodecSpecificData = (sh->bih && sh->bih->biSize > sizeof(*sh->bih)) ? (char *)(sh->bih + 1) : NULL;
    info.nCodecSpecificDataLen = info.pCodecSpecificData ? sh->bih->biSize - sizeof(*sh->bih) : 0;
    if (info.nCodecSpecificDataLen >= 5 &&
        ((unsigned char *)info.pCodecSpecificData)[0] == 1)
        ctx->avcc_nal_length_size =
            (((unsigned char *)info.pCodecSpecificData)[4] & 3) + 1;
    ctx->fallback_pts = 0;

    config.eOutputPixelFormat = PIXEL_FORMAT_YUV_MB32_420;
    config.nDeInterlaceHoldingFrameBufferNum = 1;
    config.nDisplayHoldingFrameBufferNum = 1;
    config.nRotateHoldingFrameBufferNum = 0;
    config.nDecodeSmoothFrameBufferNum = 1;
    config.memops = ctx->memops;
    config.nVbvBufferSize = 2 * 1024 * 1024;

    if (InitializeVideoDecoder(ctx->decoder, &info, &config) != 0)
        goto fail;
    if (!mpcodecs_config_vo(sh, sh->disp_w, sh->disp_h, IMGFMT_YV12))
        goto fail;

    ctx->configured = 1;
    cedar_display_decoder = ctx->decoder;
    return 1;

fail:
    if (ctx->decoder)
        DestroyVideoDecoder(ctx->decoder);
    if (ctx->memops)
        CdcMemClose(ctx->memops);
    free(ctx);
    sh->context = NULL;
    return 0;
}

static void uninit(sh_video_t *sh)
{
    vd_cedar_ctx *ctx = sh->context;
    if (!ctx)
        return;
    if (cedar_display_decoder == ctx->decoder)
        cedar_display_decoder = NULL;
    if (ctx->decoder)
        DestroyVideoDecoder(ctx->decoder);
    if (ctx->memops)
        CdcMemClose(ctx->memops);
    free(ctx);
    sh->context = NULL;
}

static int submit_packet(vd_cedar_ctx *ctx, sh_video_t *sh, void *data, int len)
{
    const unsigned char *input = data;
    const unsigned char *stream = input;
    unsigned char *annexb = NULL;
    char *buf0 = NULL, *buf1 = NULL;
    int len0 = 0, len1 = 0, stream_len = len;
    VideoStreamDataInfo packet;

    if (ctx->avcc_nal_length_size) {
        const unsigned char *src = input;
        unsigned char *dst;
        int remaining = len;
        int converted_len = 0;

        while (remaining > 0) {
            unsigned int nal_len = 0;
            int i;
            if (remaining < ctx->avcc_nal_length_size)
                goto invalid_avcc;
            for (i = 0; i < ctx->avcc_nal_length_size; i++)
                nal_len = (nal_len << 8) | src[i];
            src += ctx->avcc_nal_length_size;
            remaining -= ctx->avcc_nal_length_size;
            if (nal_len > (unsigned int)remaining ||
                nal_len > (unsigned int)(INT_MAX - converted_len - 4))
                goto invalid_avcc;
            converted_len += 4 + nal_len;
            src += nal_len;
            remaining -= nal_len;
        }

        annexb = malloc(converted_len);
        if (!annexb)
            return -1;
        src = input;
        dst = annexb;
        remaining = len;
        while (remaining > 0) {
            unsigned int nal_len = 0;
            int i;
            for (i = 0; i < ctx->avcc_nal_length_size; i++)
                nal_len = (nal_len << 8) | src[i];
            src += ctx->avcc_nal_length_size;
            remaining -= ctx->avcc_nal_length_size;
            dst[0] = 0;
            dst[1] = 0;
            dst[2] = 0;
            dst[3] = 1;
            dst += 4;
            memcpy(dst, src, nal_len);
            dst += nal_len;
            src += nal_len;
            remaining -= nal_len;
        }
        stream = annexb;
        stream_len = converted_len;
    }

    if (RequestVideoStreamBuffer(ctx->decoder, stream_len, &buf0, &len0,
                                 &buf1, &len1, 0) != 0 ||
        len0 + len1 < stream_len)
        goto fail;
    if (stream_len <= len0)
        memcpy(buf0, stream, stream_len);
    else {
        memcpy(buf0, stream, len0);
        memcpy(buf1, stream + len0, stream_len - len0);
    }
    free(annexb);
    memset(&packet, 0, sizeof(packet));
    packet.pData = buf0;
    packet.nLength = stream_len;
    packet.nPts = sh->pts >= 0 ? (int64_t)(sh->pts * 1000000.0) : ctx->fallback_pts;
    ctx->fallback_pts = packet.nPts + (sh->fps > 0 ? (int64_t)(1000000.0 / sh->fps) : 33333);
    packet.nPcr = -1;
    packet.bIsFirstPart = 1;
    packet.bIsLastPart = 1;
    return SubmitVideoStreamData(ctx->decoder, &packet, 0);

invalid_avcc:
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[cedar] invalid AVCC packet\n");
fail:
    free(annexb);
    return -1;
}

static mp_image_t *decode(sh_video_t *sh, void *data, int len, int flags)
{
    vd_cedar_ctx *ctx = sh->context;
    VideoPicture *picture = NULL;
    mp_image_t *mpi;
    int result, attempts;

    if (!ctx || !ctx->configured)
        return NULL;
    if (data && len > 0 && submit_packet(ctx, sh, data, len) != 0)
        return NULL;

    /* Cedar can defer an output picture after accepting an access unit. */
    for (attempts = 0; attempts < 64; attempts++) {
        result = DecodeVideoStream(ctx->decoder, !data, 0, 0, 0);
        if (result == VDECODE_RESULT_NO_BITSTREAM)
            break;
        if (result != VDECODE_RESULT_FRAME_DECODED &&
            result != VDECODE_RESULT_KEYFRAME_DECODED)
            continue;
        if (ValidPictureNum(ctx->decoder, 0) <= 0)
            continue;
        picture = RequestPicture(ctx->decoder, 0);
        if (picture)
            break;
    }
    if (attempts == 64 || !picture)
        return NULL;

    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE,
                             picture->nWidth, picture->nHeight);
    if (!mpi) {
        ReturnPicture(ctx->decoder, picture);
        return NULL;
    }
    /* The VO consumes priv and never dereferences the planes as CPU memory. */
    mpi->planes[0] = (unsigned char *)(uintptr_t)(picture->phyYBufAddr + 0x40000000ULL);
    mpi->planes[1] = (unsigned char *)(uintptr_t)(picture->phyCBufAddr + 0x40000000ULL);
    mpi->stride[0] = picture->nLineStride;
    mpi->stride[1] = picture->nLineStride;
    mpi->priv = picture;
    return mpi;
}
