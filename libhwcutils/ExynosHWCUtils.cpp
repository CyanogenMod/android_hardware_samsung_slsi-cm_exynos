#include "ExynosHWCUtils.h"
#include "ExynosMPPModule.h"

#ifndef V4L2_DV_720P60_SB_HALF
#define V4L2_DV_720P60_SB_HALF 23
#endif

#ifndef V4L2_DV_720P50_SB_HALF
#define V4L2_DV_720P50_SB_HALF 29
#endif

#ifndef V4L2_DV_1080P60_SB_HALF
#define V4L2_DV_1080P60_SB_HALF 40
#endif

#ifndef V4L2_DV_1080P30_SB_HALF
#define V4L2_DV_1080P30_SB_HALF 43
#endif

#ifndef V4L2_DV_720P60_TB
#define V4L2_DV_720P60_TB 24
#endif

#ifndef V4L2_DV_720P50_TB
#define V4L2_DV_720P50_TB 30
#endif

#ifndef V4L2_DV_1080P60_TB
#define V4L2_DV_1080P60_TB 41
#endif

#ifndef V4L2_DV_1080P30_TB
#define V4L2_DV_1080P30_TB 44
#endif

void dumpHandle(private_handle_t *h)
{
    ALOGV("\t\tformat = %d, width = %u, height = %u, stride = %u, vstride = %u",
            h->format, h->width, h->height, h->stride, h->vstride);
}

void dumpLayer(hwc_layer_1_t const *l)
{
    ALOGV("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
            "{%7.1f,%7.1f,%7.1f,%7.1f}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform,
            l->blending,
            l->sourceCropf.left,
            l->sourceCropf.top,
            l->sourceCropf.right,
            l->sourceCropf.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);

    if(l->handle && !(l->flags & HWC_SKIP_LAYER))
        dumpHandle(private_handle_t::dynamicCast(l->handle));
}

void dumpConfig(fb_win_config &c)
{
    ALOGV("\tstate = %u", c.state);
    if (c.state == WIN_STATE_BUFFER) {
#ifdef DECON_FB
        ALOGV("\t\tfd[0] = %d, fd[1] = %d, fd[2] = %d, "
              "src.x = %d, src.y = %d, src.w = %u, src.h = %u, "
              "src.f_w = %u, src.f_h = %u, "
              "dst.x = %d, dst.y = %d, dst.w = %u, dst.h = %u, "
              "dst.f_w = %u, dst.f_h = %u, "
              "format = %u, blending = %u",
              c.fd_idma[0], c.fd_idma[1], c.fd_idma[2],
              c.src.x, c.src.y, c.src.w, c.src.h,
              c.src.f_w, c.src.f_h,
              c.dst.x, c.dst.y, c.dst.w, c.dst.h,
              c.dst.f_w, c.dst.f_h,
              c.format, c.blending);
#else
        ALOGV("\t\tfd = %d, offset = %u, stride = %u, "
                "x = %d, y = %d, w = %u, h = %u, "
                "format = %u, blending = %u",
                c.fd, c.offset, c.stride,
                c.x, c.y, c.w, c.h,
                c.format, c.blending);
#endif
    }
    else if (c.state == WIN_STATE_COLOR) {
        ALOGV("\t\tcolor = %u", c.color);
    }
}

void dumpMPPImage(exynos_mpp_img &c)
{
    ALOGV("\tx = %u, y = %u, w = %u, h = %u, fw = %u, fh = %u",
            c.x, c.y, c.w, c.h, c.fw, c.fh);
    ALOGV("\tf = %u", c.format);
    ALOGV("\taddr = {%d, %d, %d}, rot = %u, cacheable = %u, drmMode = %u",
            c.yaddr, c.uaddr, c.vaddr, c.rot, c.cacheable, c.drmMode);
    ALOGV("\tnarrowRgb = %u, acquireFenceFd = %d, releaseFenceFd = %d, mem_type = %u",
            c.narrowRgb, c.acquireFenceFd, c.releaseFenceFd, c.mem_type);
}

bool isDstCropWidthAligned(int dest_w)
{
    int dst_crop_w_alignement;

   /* GSC's dst crop size should be aligned 128Bytes */
    dst_crop_w_alignement = GSC_DST_CROP_W_ALIGNMENT_RGB888;

    return (dest_w % dst_crop_w_alignement) == 0;
}

bool isTransformed(const hwc_layer_1_t &layer)
{
    return layer.transform != 0;
}

bool isRotated(const hwc_layer_1_t &layer)
{
    return (layer.transform & HAL_TRANSFORM_ROT_90) ||
            (layer.transform & HAL_TRANSFORM_ROT_180);
}

bool isScaled(const hwc_layer_1_t &layer)
{
    return WIDTH(layer.displayFrame) != WIDTH(layer.sourceCropf) ||
            HEIGHT(layer.displayFrame) != HEIGHT(layer.sourceCropf);
}

bool isFormatSupported(int format)
{
    return halFormatToSocFormat(format) < PIXEL_FORMAT_MAX;
}

bool isFormatRgb(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_BGRA_8888:
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
        return true;

    default:
        return false;
    }
}

bool isFormatYCrCb(int format)
{
    return format == HAL_PIXEL_FORMAT_EXYNOS_YV12_M;
}

uint8_t formatToBpp(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
        return 32;

    case HAL_PIXEL_FORMAT_RGB_565:
        return 16;

    default:
        ALOGW("unrecognized pixel format %u", format);
        return 0;
    }
}

bool isXAligned(const hwc_layer_1_t &layer, int format)
{
    if (!isFormatSupported(format))
        return true;

    uint8_t bpp = formatToBpp(format);
    if (!bpp)
        return false;

    uint8_t pixel_alignment = 32 / bpp;

    return (layer.displayFrame.left % pixel_alignment) == 0 &&
            (layer.displayFrame.right % pixel_alignment) == 0;
}

int getDrmMode(int flags)
{
    if (flags & GRALLOC_USAGE_PROTECTED) {
#ifdef GRALLOC_USAGE_PRIVATE_NONSECURE
        if (flags & GRALLOC_USAGE_PRIVATE_NONSECURE)
            return NORMAL_DRM;
        else
#endif
            return SECURE_DRM;
    } else {
        return NO_DRM;
    }
}

int halFormatToV4L2Format(int format)
{
#ifdef EXYNOS_SUPPORT_BGRX_8888
    if (format == HAL_PIXEL_FORMAT_BGRX_8888)
        return HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_RGBX_8888);
    else
#endif
        return HAL_PIXEL_FORMAT_2_V4L2_PIX(format);
}

bool isBlendingSupported(int32_t blending)
{
    return halBlendingToSocBlending(blending) < BLENDING_MAX;
}

bool isOffscreen(hwc_layer_1_t &layer, int xres, int yres)
{
    return (int) layer.sourceCropf.left > xres ||
            (int) layer.sourceCropf.right < 0 ||
            (int) layer.sourceCropf.top > yres ||
            (int) layer.sourceCropf.bottom < 0;
}

bool isSrcCropFloat(hwc_frect &frect)
{
    return (frect.left != (int)frect.left) ||
        (frect.top != (int)frect.top) ||
        (frect.right != (int)frect.right) ||
        (frect.bottom != (int)frect.bottom);
}

size_t visibleWidth(ExynosMPP *processor, hwc_layer_1_t &layer, int format,
        int xres)
{
    int bpp;
    if (processor->isProcessingRequired(layer, format))
        bpp = 32;
    else
        bpp = formatToBpp(format);
    int left = max(layer.displayFrame.left, 0);
    int right = min(layer.displayFrame.right, xres);

    return (right - left) * bpp / 8;
}

bool compareYuvLayerConfig(int videoLayers, uint32_t index,
        hwc_layer_1_t &layer,
        video_layer_config *pre_src_data, video_layer_config *pre_dst_data)
{
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    int ret = 0;
    bool reconfigure = 1;

    video_layer_config new_src_cfg, new_dst_cfg;
    memset(&new_src_cfg, 0, sizeof(new_src_cfg));
    memset(&new_dst_cfg, 0, sizeof(new_dst_cfg));

    new_src_cfg.x = (int)layer.sourceCropf.left;
    new_src_cfg.y = (int)layer.sourceCropf.top;
    new_src_cfg.w = WIDTH(layer.sourceCropf);
    new_src_cfg.fw = src_handle->stride;
    new_src_cfg.h = HEIGHT(layer.sourceCropf);
    new_src_cfg.fh = src_handle->vstride;
    new_src_cfg.format = src_handle->format;
    new_src_cfg.drmMode = !!(getDrmMode(src_handle->flags) == SECURE_DRM);
    new_src_cfg.index = index;

    new_dst_cfg.x = layer.displayFrame.left;
    new_dst_cfg.y = layer.displayFrame.top;
    new_dst_cfg.w = WIDTH(layer.displayFrame);
    new_dst_cfg.h = HEIGHT(layer.displayFrame);
    new_dst_cfg.rot = layer.transform;
    new_dst_cfg.drmMode = new_src_cfg.drmMode;

    /* check to save previous yuv layer configration */
    if (pre_src_data && pre_dst_data) {
         reconfigure = yuvConfigChanged(new_src_cfg, pre_src_data[videoLayers]) ||
            yuvConfigChanged(new_dst_cfg, pre_dst_data[videoLayers]);
    } else {
        ALOGE("Invalid parameter");
        return reconfigure;
    }

    memcpy(&pre_src_data[videoLayers], &new_src_cfg, sizeof(new_src_cfg));
    memcpy(&pre_dst_data[videoLayers], &new_dst_cfg, sizeof(new_dst_cfg));

    return reconfigure;

}

size_t getRequiredPixels(hwc_layer_1_t &layer, int xres, int yres)
{
    uint32_t w = WIDTH(layer.displayFrame);
    uint32_t h = HEIGHT(layer.displayFrame);
    if (layer.displayFrame.left < 0) {
        unsigned int crop = -layer.displayFrame.left;
        w -= crop;
    }

    if (layer.displayFrame.right > xres) {
        unsigned int crop = layer.displayFrame.right - xres;
        w -= crop;
    }

    if (layer.displayFrame.top < 0) {
        unsigned int crop = -layer.displayFrame.top;
        h -= crop;
    }

    if (layer.displayFrame.bottom > yres) {
        int crop = layer.displayFrame.bottom - yres;
        h -= crop;
    }
    return w*h;
}

/* OFF_Screen to ON_Screen changes */
void recalculateDisplayFrame(hwc_layer_1_t &layer, int xres, int yres)
{
    uint32_t x, y;
    uint32_t w = WIDTH(layer.displayFrame);
    uint32_t h = HEIGHT(layer.displayFrame);

    if (layer.displayFrame.left < 0) {
        unsigned int crop = -layer.displayFrame.left;
        ALOGV("layer off left side of screen; cropping %u pixels from left edge",
                crop);
        x = 0;
        w -= crop;
    } else {
        x = layer.displayFrame.left;
    }

    if (layer.displayFrame.right > xres) {
        unsigned int crop = layer.displayFrame.right - xres;
        ALOGV("layer off right side of screen; cropping %u pixels from right edge",
                crop);
        w -= crop;
    }

    if (layer.displayFrame.top < 0) {
        unsigned int crop = -layer.displayFrame.top;
        ALOGV("layer off top side of screen; cropping %u pixels from top edge",
                crop);
        y = 0;
        h -= crop;
    } else {
        y = layer.displayFrame.top;
    }

    if (layer.displayFrame.bottom > yres) {
        int crop = layer.displayFrame.bottom - yres;
        ALOGV("layer off bottom side of screen; cropping %u pixels from bottom edge",
                crop);
        h -= crop;
    }

    layer.displayFrame.left = x;
    layer.displayFrame.top = y;
    layer.displayFrame.right = w + x;
    layer.displayFrame.bottom = h + y;
}

int getS3DFormat(int preset)
{
    switch (preset) {
    case V4L2_DV_720P60_SB_HALF:
    case V4L2_DV_720P50_SB_HALF:
    case V4L2_DV_1080P60_SB_HALF:
    case V4L2_DV_1080P30_SB_HALF:
        return S3D_SBS;
    case V4L2_DV_720P60_TB:
    case V4L2_DV_720P50_TB:
    case V4L2_DV_1080P60_TB:
    case V4L2_DV_1080P30_TB:
        return S3D_TB;
    default:
        return S3D_ERROR;
    }
}
