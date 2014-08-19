#include "ExynosMPP.h"
#include "ExynosHWCUtils.h"

ExynosMPP::ExynosMPP()
{
    ExynosMPP(NULL, 0);
}

ExynosMPP::ExynosMPP(ExynosDisplay *display, int gscIndex)
{
    this->mDisplay = display;
    this->mIndex = gscIndex;
    mNeedReqbufs = false;
    mWaitVsyncCount = 0;
    mCountSameConfig = 0;
    mGscHandle = NULL;
    memset(&mSrcConfig, 0, sizeof(mSrcConfig));
    memset(&mMidConfig, 0, sizeof(mMidConfig));
    memset(&mDstConfig, 0, sizeof(mDstConfig));
    for (uint32_t i = 0; i < NUM_GSC_DST_BUFS; i++) {
        mDstBuffers[i] = NULL;
        mMidBuffers[i] = NULL;
        mDstBufFence[i] = -1;
        mMidBufFence[i] = -1;
    }
    mCurrentBuf = 0;
    mGSCMode = 0;
    mLastGSCLayerHandle = -1;
    mS3DMode = 0;
    mppFact = NULL;
    libmpp = NULL;
}

ExynosMPP::~ExynosMPP()
{
}

bool ExynosMPP::isM2M()
{
    return mGSCMode == exynos5_gsc_map_t::GSC_M2M;
}

bool ExynosMPP::isUsingMSC()
{
    return (AVAILABLE_GSC_UNITS[mIndex] >= 4 && AVAILABLE_GSC_UNITS[mIndex] <= 6);
}

bool ExynosMPP::isOTF()
{
    return mGSCMode == exynos5_gsc_map_t::GSC_LOCAL;
}

void ExynosMPP::setMode(int mode)
{
    mGSCMode = mode;
}

void ExynosMPP::free()
{
    if (mNeedReqbufs) {
        if (mWaitVsyncCount > 0) {
            //if (!exynos_gsc_free_and_close(mGscHandle))
            if (!freeMPP(mGscHandle))
                mGscHandle = NULL;
            mNeedReqbufs = false;
            mWaitVsyncCount = 0;
            if (mDisplay->mOtfMode != OTF_TO_M2M && mDisplay->mOtfMode != SEC_M2M)
                mDisplay->mOtfMode = OTF_OFF;
        } else {
            mWaitVsyncCount++;
        }
    }
}

bool ExynosMPP::isSrcConfigChanged(exynos_mpp_img &c1, exynos_mpp_img &c2)
{
    return isDstConfigChanged(c1, c2) ||
            c1.fw != c2.fw ||
            c1.fh != c2.fh;
}

bool ExynosMPP::isFormatSupportedByGsc(int format)
{

    switch (format) {
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
        return true;

    default:
        return false;
    }
}

bool ExynosMPP::formatRequiresGsc(int format)
{
    return (isFormatSupportedByGsc(format) &&
           (format != HAL_PIXEL_FORMAT_RGBX_8888) && (format != HAL_PIXEL_FORMAT_RGB_565));
}

int ExynosMPP::getDownscaleRatio(int xres, int yres)
{
    return 0;
}

bool ExynosMPP::isFormatSupportedByGscOtf(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
        return true;
    default:
        return false;
    }
}

bool ExynosMPP::isProcessingSupported(hwc_layer_1_t &layer, int format,
        bool local_path, int loc_out_downscale)
{
    if (local_path && loc_out_downscale == 0)
        return false;

    if (isUsingMSC() && local_path)
        return false;

    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    int max_w = maxWidth(layer);
    int max_h = maxHeight(layer);
    int min_w = minWidth(layer);
    int min_h = minHeight(layer);
    int crop_max_w = 0;
    int crop_max_h = 0;

    if (isUsingMSC()) {
        crop_max_w = 8192;
        crop_max_h = 8192;
    } else {
        crop_max_w = isRotated(layer) ? 2016 : 4800;
        crop_max_h = isRotated(layer) ? 2016 : 3344;
    }
    int crop_min_w = isRotated(layer) ? 32: 64;
    int crop_min_h = isRotated(layer) ? 64: 32;

    int srcAlign = sourceAlign(handle->format);
    int dstAlign;
    if (local_path)
        dstAlign = destinationAlign(HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M);
    else
        dstAlign = destinationAlign(HAL_PIXEL_FORMAT_BGRA_8888);

    int maxDstWidth;
    int maxDstHeight;

    bool rot90or270 = !!(layer.transform & HAL_TRANSFORM_ROT_90);
    // n.b.: HAL_TRANSFORM_ROT_270 = HAL_TRANSFORM_ROT_90 |
    //                               HAL_TRANSFORM_ROT_180

    int src_w = WIDTH(layer.sourceCropf), src_h = HEIGHT(layer.sourceCropf);
    int dest_w, dest_h;
    if (rot90or270) {
        dest_w = HEIGHT(layer.displayFrame);
        dest_h = WIDTH(layer.displayFrame);
    } else {
        dest_w = WIDTH(layer.displayFrame);
        dest_h = HEIGHT(layer.displayFrame);
    }

    if (getDrmMode(handle->flags) != NO_DRM)
        alignCropAndCenter(dest_w, dest_h, NULL,
                GSC_DST_CROP_W_ALIGNMENT_RGB888);

    int max_downscale = local_path ? loc_out_downscale : 16;
    maxDstWidth = 2560;
    maxDstHeight = 1600;
    int max_upscale = 8;

    /* check whether GSC can handle with local path */
    if (local_path) {
        /* GSC OTF can't handle rot90 or rot270 */
        if (!rotationSupported(rot90or270))
            return 0;
        /*
         * if display co-ordinates are out of the lcd resolution,
         * skip that scenario to OpenGL.
         * GSC OTF can't handle such scenarios.
         */
        if (layer.displayFrame.left < 0 || layer.displayFrame.top < 0 ||
            layer.displayFrame.right > mDisplay->mXres || layer.displayFrame.bottom > mDisplay->mYres)
            return 0;

        /* GSC OTF can't handle GRALLOC_USAGE_PROTECTED layer */
        if (getDrmMode(handle->flags) != NO_DRM)
            return 0;

        return isFormatSupportedByGsc(format) &&
            isFormatSupportedByGscOtf(format) &&
            mDisplay->mHwc->mS3DMode == S3D_MODE_DISABLED &&
            paritySupported(dest_w, dest_h) &&
            handle->stride <= max_w &&
            src_w <= dest_w * max_downscale &&
            dest_w <= maxDstWidth &&
            dest_w <= src_w * max_upscale &&
            handle->vstride <= max_h &&
            src_h <= dest_h * max_downscale &&
            dest_h <= maxDstHeight &&
            dest_h <= src_h * max_upscale &&
            src_w <= crop_max_w &&
            src_h <= crop_max_h &&
            src_w >= crop_min_w &&
            src_h >= crop_min_h;
     }

    bool need_gsc_op_twice = false;
    if (getDrmMode(handle->flags) != NO_DRM) {
        need_gsc_op_twice = ((dest_w > src_w * max_upscale) ||
                                   (dest_h > src_h * max_upscale)) ? true : false;
        if (need_gsc_op_twice)
            max_upscale = 8 * 8;
    } else {
        if (!mDisplay->mHasDrmSurface) {
            need_gsc_op_twice = false;
            max_upscale = 8;
        }
    }

    if (getDrmMode(handle->flags) != NO_DRM) {
        /* make even for gscaler */
        layer.sourceCropf.top = (unsigned int)layer.sourceCropf.top & ~1;
        layer.sourceCropf.left = (unsigned int)layer.sourceCropf.left & ~1;
        layer.sourceCropf.bottom = (unsigned int)layer.sourceCropf.bottom & ~1;
        layer.sourceCropf.right = (unsigned int)layer.sourceCropf.right & ~1;
    }

    /* check whether GSC can handle with M2M */
    return isFormatSupportedByGsc(format) &&
            src_w >= min_w &&
            src_h >= min_h &&
            isDstCropWidthAligned(dest_w) &&
            handle->stride <= max_w &&
            handle->stride % srcAlign == 0 &&
            src_w < dest_w * max_downscale &&
            dest_w <= src_w * max_upscale &&
            handle->vstride <= max_h &&
            handle->vstride % srcAlign == 0 &&
            src_h < dest_h * max_downscale &&
            dest_h <= src_h * max_upscale &&
            // per 46.2
            (!rot90or270 || (unsigned int)layer.sourceCropf.top % 2 == 0) &&
            (!rot90or270 || (unsigned int)layer.sourceCropf.left % 2 == 0) &&
            src_w <= crop_max_w &&
            src_h <= crop_max_h &&
            src_w >= crop_min_w &&
            src_h >= crop_min_h;
            // per 46.3.1.6
}

bool ExynosMPP::isProcessingRequired(hwc_layer_1_t &layer, int format)
{
    return formatRequiresGsc(format) || isScaled(layer)
            || isTransformed(layer) || !isXAligned(layer, format);
}

void ExynosMPP::setupSource(exynos_mpp_img &src_img, hwc_layer_1_t &layer)
{
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    src_img.x = ALIGN((unsigned int)layer.sourceCropf.left, srcXOffsetAlign(layer));
    src_img.y = ALIGN((unsigned int)layer.sourceCropf.top, srcYOffsetAlign(layer));
    src_img.w = WIDTH(layer.sourceCropf);
    src_img.fw = src_handle->stride;
    src_img.h = HEIGHT(layer.sourceCropf);
    src_img.fh = src_handle->vstride;
    src_img.yaddr = src_handle->fd;
    if (mS3DMode == S3D_SBS)
        src_img.w /= 2;
    if (mS3DMode == S3D_TB)
        src_img.h /= 2;
    if (isFormatYCrCb(src_handle->format)) {
        src_img.uaddr = src_handle->fd2;
        src_img.vaddr = src_handle->fd1;
    } else {
        src_img.uaddr = src_handle->fd1;
        src_img.vaddr = src_handle->fd2;
    }
    if (src_handle->format != HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL)
        src_img.format = src_handle->format;
    else
        src_img.format = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
    src_img.drmMode = !!(getDrmMode(src_handle->flags) == SECURE_DRM);
    src_img.acquireFenceFd = layer.acquireFenceFd;
}

void ExynosMPP::setupOtfDestination(exynos_mpp_img &src_img, exynos_mpp_img &dst_img, hwc_layer_1_t &layer)
{
    dst_img.x = layer.displayFrame.left;
    dst_img.y = layer.displayFrame.top;
    dst_img.fw = mDisplay->mXres;
    dst_img.fh = mDisplay->mYres;
    dst_img.w = WIDTH(layer.displayFrame);
    dst_img.h = HEIGHT(layer.displayFrame);
    dst_img.w = min(dst_img.w, dst_img.fw - dst_img.x);
    dst_img.h = min(dst_img.h, dst_img.fh - dst_img.y);
    dst_img.format = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M;
    dst_img.rot = layer.transform;
    dst_img.drmMode = src_img.drmMode;
    dst_img.yaddr = (uint32_t)NULL;
}

int ExynosMPP::sourceAlign(int format)
{
    return 16;
}

int ExynosMPP::destinationAlign(int format)
{
    return 16;
}

int ExynosMPP::reconfigureOtf(exynos_mpp_img *src_img, exynos_mpp_img *dst_img)
{
    int ret = 0;
    if (mGscHandle) {
        ret = stopMPP(mGscHandle);
        if (ret < 0) {
            ALOGE("failed to stop gscaler %u", mIndex);
            return ret;
        }
        mNeedReqbufs = true;
        mCountSameConfig = 0;
    }

    if (!mGscHandle) {
        mGscHandle = createMPP(AVAILABLE_GSC_UNITS[mIndex],
            GSC_OUTPUT_MODE, GSC_OUT_FIMD, false);
        if (!mGscHandle) {
            ALOGE("failed to create gscaler handle");
            return -1;
        }
    }

    ret = configMPP(mGscHandle, src_img, dst_img);
    if (ret < 0) {
        ALOGE("failed to configure gscaler %u", mIndex);
        return ret;
    }

    return ret;
}

int ExynosMPP::processOTF(hwc_layer_1_t &layer)
{
    ALOGV("configuring gscaler %u for memory-to-fimd-localout", mIndex);

    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    int ret = 0;

    int srcAlign = sourceAlign(src_handle->format);
    int dstAlign;

    exynos_mpp_img src_img, dst_img;
    memset(&src_img, 0, sizeof(src_img));
    memset(&dst_img, 0, sizeof(dst_img));

    setupSource(src_img, layer);
    setupOtfDestination(src_img, dst_img, layer);

    dstAlign = destinationAlign(dst_img.format);

    ALOGV("source configuration:");
    dumpMPPImage(src_img);

    if (!mGscHandle || isSrcConfigChanged(src_img, mSrcConfig) ||
            isDstConfigChanged(dst_img, mDstConfig)) {

        if (!isPerFrameSrcChanged(src_img, mSrcConfig) ||
                !isPerFrameDstChanged(dst_img, mDstConfig)) {
            if (reconfigureOtf(&src_img, &dst_img) < 0)
                goto err_gsc_local;
        }
    }

    ALOGV("destination configuration:");
    dumpMPPImage(dst_img);

    ret = runMPP(mGscHandle, &src_img, &dst_img);
    if (ret < 0) {
        ALOGE("failed to run gscaler %u", mIndex);
        goto err_gsc_local;
    }

    memcpy(&mSrcConfig, &src_img, sizeof(mSrcConfig));
    memcpy(&mDstConfig, &dst_img, sizeof(mDstConfig));

    layer.releaseFenceFd = src_img.releaseFenceFd;
    return 0;

err_gsc_local:
    if (src_img.acquireFenceFd >= 0)
        close(src_img.acquireFenceFd);

    destroyMPP(mGscHandle);
    mGscHandle = NULL;

    memset(&mSrcConfig, 0, sizeof(mSrcConfig));
    memset(&mDstConfig, 0, sizeof(mDstConfig));

    return ret;
}

bool ExynosMPP::setupDoubleOperation(exynos_mpp_img &src_img, exynos_mpp_img &mid_img, hwc_layer_1_t &layer)
{
    /* check if GSC need to operate twice */
    bool need_gsc_op_twice = false;
    bool need_unscaled_csc = false;
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    const int max_upscale = 8;
    bool rot90or270 = !!(layer.transform & HAL_TRANSFORM_ROT_90);
    int src_w = WIDTH(layer.sourceCropf), src_h = HEIGHT(layer.sourceCropf);
    int dest_w, dest_h;
    if (rot90or270) {
        dest_w = HEIGHT(layer.displayFrame);
        dest_h = WIDTH(layer.displayFrame);
    } else {
        dest_w = WIDTH(layer.displayFrame);
        dest_h = HEIGHT(layer.displayFrame);
    }
    if (getDrmMode(src_handle->flags) != NO_DRM)
        need_gsc_op_twice = ((dest_w > src_w * max_upscale) ||
                             (dest_h > src_h * max_upscale)) ? true : false;

    if (isUsingMSC() && src_handle->format == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED) {
        need_gsc_op_twice = true;
        need_unscaled_csc = true;
    }

    if (need_gsc_op_twice) {
        mid_img.x = 0;
        mid_img.y = 0;

        int mid_w = 0, mid_h = 0;

        if (need_unscaled_csc) {
            mid_img.w = src_w;
            mid_img.h = src_h;
        } else {
            if (rot90or270) {
                mid_w = HEIGHT(layer.displayFrame);
                mid_h = WIDTH(layer.displayFrame);
            } else {
                mid_w = WIDTH(layer.displayFrame);
                mid_h = HEIGHT(layer.displayFrame);
            }

            if (WIDTH(layer.sourceCropf) * max_upscale  < mid_w)
                mid_img.w = (((mid_w + 7) / 8) + 1) & ~1;
            else
                mid_img.w = mid_w;

            if (HEIGHT(layer.sourceCropf) * max_upscale < mid_h)
                mid_img.h = (((mid_h + 7) / 8) + 1) & ~1;
            else
                mid_img.h = mid_h;
        }
        mid_img.drmMode = src_img.drmMode;
        mid_img.format = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
        mid_img.mem_type = GSC_MEM_DMABUF;
        mid_img.narrowRgb = !isFormatRgb(src_handle->format);
    }

    return need_gsc_op_twice;
}

void ExynosMPP::setupM2MDestination(exynos_mpp_img &src_img, exynos_mpp_img &dst_img,
        int dst_format, hwc_layer_1_t &layer, hwc_frect_t *sourceCrop)
{
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    dst_img.x = 0;
    dst_img.y = 0;
    dst_img.w = WIDTH(layer.displayFrame);
    dst_img.h = HEIGHT(layer.displayFrame);
    dst_img.rot = layer.transform;
    dst_img.drmMode = src_img.drmMode;
    dst_img.format = dst_format;
    dst_img.mem_type = GSC_MEM_DMABUF;
    if (src_handle->format == HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL)
        dst_img.narrowRgb = 0;
    else
        dst_img.narrowRgb = !isFormatRgb(src_handle->format);
    if (dst_img.drmMode)
        alignCropAndCenter(dst_img.w, dst_img.h, sourceCrop,
                GSC_DST_CROP_W_ALIGNMENT_RGB888);
}

int ExynosMPP::reallocateBuffers(private_handle_t *src_handle, exynos_mpp_img &dst_img, exynos_mpp_img &mid_img, bool need_gsc_op_twice)
{
    alloc_device_t* alloc_device = mDisplay->mAllocDevice;
    int ret = 0;
    int dst_stride;
    int usage = GRALLOC_USAGE_SW_READ_NEVER |
            GRALLOC_USAGE_SW_WRITE_NEVER |
#ifdef USE_FB_PHY_LINEAR
            ((mIndex == FIMD_GSC_IDX) ? GRALLOC_USAGE_PHYSICALLY_LINEAR : 0) |
#endif
            GRALLOC_USAGE_HW_COMPOSER;

#ifdef USE_FB_PHY_LINEAR
    usage |= GRALLOC_USAGE_PROTECTED;
    usage &= ~GRALLOC_USAGE_PRIVATE_NONSECURE;
#else
    if (getDrmMode(src_handle->flags) == SECURE_DRM) {
        usage |= GRALLOC_USAGE_PROTECTED;
        usage &= ~GRALLOC_USAGE_PRIVATE_NONSECURE;
    } else if (getDrmMode(src_handle->flags) == NORMAL_DRM) {
        usage |= GRALLOC_USAGE_PROTECTED;
        usage |= GRALLOC_USAGE_PRIVATE_NONSECURE;
    }
#endif

    int w, h;
    {
        int dstAlign = destinationAlign(dst_img.format);
#if !defined(USES_NEW_HDMI)
        if (mIndex == HDMI_GSC_IDX) {
            w = ALIGN(mDisplay->mXres, dstAlign);
            h = ALIGN(mDisplay->mYres, dstAlign);
        } else {
#endif
            w = ALIGN(dst_img.w, dstAlign);
            h = ALIGN(dst_img.h, dstAlign);
#if !defined(USES_NEW_HDMI)
        }
#endif
        /* ext_only andn int_only changes */
        if (getDrmMode(src_handle->flags) == SECURE_DRM) {
            w = ALIGN(mDisplay->mXres, dstAlign);
            h = ALIGN(mDisplay->mYres, dstAlign);
        }
    }

    for (size_t i = 0; i < NUM_GSC_DST_BUFS; i++) {
        if (mDstBuffers[i]) {
            alloc_device->free(alloc_device, mDstBuffers[i]);
            mDstBuffers[i] = NULL;
        }

        if (mDstBufFence[i] >= 0) {
            close(mDstBufFence[i]);
            mDstBufFence[i] = -1;
        }

        if (mMidBuffers[i] != NULL) {
            alloc_device->free(alloc_device, mMidBuffers[i]);
            mMidBuffers[i] = NULL;
        }

        if (mMidBufFence[i] >= 0) {
            close(mMidBufFence[i]);
            mMidBufFence[i] = -1;
        }

        int format = dst_img.format;
        ret = alloc_device->alloc(alloc_device, w, h,
                format, usage, &mDstBuffers[i],
                &dst_stride);
        if (ret < 0) {
            ALOGE("failed to allocate destination buffer(%dx%d): %s", w, h,
                    strerror(-ret));
            return ret;
        }

        if (need_gsc_op_twice) {
            ret = alloc_device->alloc(alloc_device, mid_img.w, mid_img.h,
                     HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M, usage, &mMidBuffers[i],
                     &dst_stride);
            if (ret < 0) {
                ALOGE("failed to allocate intermediate buffer(%dx%d): %s", mid_img.w, mid_img.h,
                        strerror(-ret));
                return ret;
            }
        }
    }
    return ret;
}

#ifdef USES_VIRTUAL_DISPLAY
int ExynosMPP::processM2M(hwc_layer_1_t &layer, int dst_format, hwc_frect_t *sourceCrop, bool isNeedBufferAlloc)
#else
int ExynosMPP::processM2M(hwc_layer_1_t &layer, int dst_format, hwc_frect_t *sourceCrop)
#endif
{
    ALOGV("configuring gscaler %u for memory-to-memory", AVAILABLE_GSC_UNITS[mIndex]);

    alloc_device_t* alloc_device = mDisplay->mAllocDevice;
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    buffer_handle_t mid_buf;
    private_handle_t *mid_handle;
    int ret = 0;
    int dstAlign;
#ifdef USES_VIRTUAL_DISPLAY
    bool need_gsc_op_twice = false;
#endif

    exynos_mpp_img src_img, dst_img;
    memset(&src_img, 0, sizeof(src_img));
    memset(&dst_img, 0, sizeof(dst_img));
    exynos_mpp_img mid_img;
    memset(&mid_img, 0, sizeof(mid_img));

    hwc_frect_t sourceCropTemp;
    if (!sourceCrop)
        sourceCrop = &sourceCropTemp;

    setupSource(src_img, layer);
    src_img.mem_type = GSC_MEM_DMABUF;

#ifdef USES_VIRTUAL_DISPLAY
    need_gsc_op_twice = setupDoubleOperation(src_img, mid_img, layer) && isNeedBufferAlloc;
#else
    bool need_gsc_op_twice = setupDoubleOperation(src_img, mid_img, layer);
#endif

    setupM2MDestination(src_img, dst_img, dst_format, layer, sourceCrop);

#ifdef USES_VIRTUAL_DISPLAY
    if (!isNeedBufferAlloc) {
        dst_img.x = mDisplay->mHwc->mVirtualDisplayRect.left;
        dst_img.y = mDisplay->mHwc->mVirtualDisplayRect.top;
        dst_img.w = mDisplay->mHwc->mVirtualDisplayRect.width;
        dst_img.h = mDisplay->mHwc->mVirtualDisplayRect.height;
    }
#endif

    ALOGV("source configuration:");
    dumpMPPImage(src_img);

    bool reconfigure = isSrcConfigChanged(src_img, mSrcConfig) ||
            isDstConfigChanged(dst_img, mDstConfig);
    bool realloc = true;

#ifdef USES_VIRTUAL_DISPLAY
    if (isNeedBufferAlloc) {
#endif
    /* ext_only andn int_only changes */
    if (!need_gsc_op_twice && getDrmMode(src_handle->flags) == SECURE_DRM) {
        if (dst_img.drmMode != mDstConfig.drmMode)
            realloc = true;
        else
            realloc = false;
    }

    if (reconfigure && realloc) {
        if (reallocateBuffers(src_handle, dst_img, mid_img, need_gsc_op_twice) < 0)
            goto err_alloc;

        mCurrentBuf = 0;
        mLastGSCLayerHandle = 0;
    }

    if (!reconfigure && (mLastGSCLayerHandle == (uint32_t)layer.handle)) {
        ALOGV("[USE] GSC_SKIP_DUPLICATE_FRAME_PROCESSING\n");
        if (layer.acquireFenceFd >= 0)
            close(layer.acquireFenceFd);

        layer.releaseFenceFd = -1;
        layer.acquireFenceFd = -1;
        mDstConfig.releaseFenceFd = -1;

        mCurrentBuf = (mCurrentBuf + NUM_GSC_DST_BUFS - 1) % NUM_GSC_DST_BUFS;
        if (mDstBufFence[mCurrentBuf] >= 0) {
            close (mDstBufFence[mCurrentBuf]);
            mDstBufFence[mCurrentBuf] = -1;
        }
        return 0;
    } else {
        mLastGSCLayerHandle = (uint32_t)layer.handle;
    }
#ifdef USES_VIRTUAL_DISPLAY
    }
#endif

    layer.acquireFenceFd = -1;
    if (need_gsc_op_twice) {
        mid_img.acquireFenceFd = mMidBufFence[mCurrentBuf];
        mMidBufFence[mCurrentBuf] = -1;
        mid_buf = mMidBuffers[mCurrentBuf];
        mid_handle = private_handle_t::dynamicCast(mid_buf);

        mid_img.fw = mid_handle->stride;
        mid_img.fh = mid_handle->vstride;
        mid_img.yaddr = mid_handle->fd;
        if (isFormatYCrCb(mid_handle->format)) {
            mid_img.uaddr = mid_handle->fd2;
            mid_img.vaddr = mid_handle->fd1;
        } else {
            mid_img.uaddr = mid_handle->fd1;
            mid_img.vaddr = mid_handle->fd2;
        }
        //mid_img.acquireFenceFd = -1;

        ALOGV("mid configuration:");
        dumpMPPImage(mid_img);
    }

    dst_buf = mDstBuffers[mCurrentBuf];
    dst_handle = private_handle_t::dynamicCast(dst_buf);

    dst_img.fw = dst_handle->stride;
    dst_img.fh = dst_handle->vstride;
    dst_img.yaddr = dst_handle->fd;
    dst_img.uaddr = dst_handle->fd1;
    dst_img.vaddr = dst_handle->fd2;
    dst_img.acquireFenceFd = mDstBufFence[mCurrentBuf];
    mDstBufFence[mCurrentBuf] = -1;

    ALOGV("destination configuration:");
    dumpMPPImage(dst_img);

    if ((int)dst_img.w != WIDTH(layer.displayFrame))
        ALOGV("padding %u x %u output to %u x %u and cropping to {%7.1f,%7.1f,%7.1f,%7.1f}",
                WIDTH(layer.displayFrame), HEIGHT(layer.displayFrame),
                dst_img.w, dst_img.h, sourceCrop->left, sourceCrop->top,
                sourceCrop->right, sourceCrop->bottom);

    if (mGscHandle) {
        ALOGV("reusing open gscaler %u", AVAILABLE_GSC_UNITS[mIndex]);
    } else {
        ALOGV("opening gscaler %u", AVAILABLE_GSC_UNITS[mIndex]);
        mGscHandle = createMPP(
                AVAILABLE_GSC_UNITS[mIndex], GSC_M2M_MODE, GSC_DUMMY, true);
        if (!mGscHandle) {
            ALOGE("failed to create gscaler handle");
            ret = -1;
            goto err_alloc;
        }
    }

    if (!need_gsc_op_twice)
        memcpy(&mid_img, &dst_img, sizeof(exynos_mpp_img));

    /* src -> mid or src->dest */
    if (reconfigure || need_gsc_op_twice) {
        ret = stopMPP(mGscHandle);
        if (ret < 0) {
            ALOGE("failed to stop gscaler %u", mIndex);
            goto err_gsc_config;
        }

        ret = setCSCProperty(mGscHandle, 0, !mid_img.narrowRgb, 1);
        ret = configMPP(mGscHandle, &src_img, &mid_img);
        if (ret < 0) {
            ALOGE("failed to configure gscaler %u", mIndex);
            goto err_gsc_config;
        }
    }

    ret = runMPP(mGscHandle, &src_img, &mid_img);
    if (ret < 0) {
        ALOGE("failed to run gscaler %u", mIndex);
        goto err_gsc_config;
    }

    /* mid -> dst */
    if (need_gsc_op_twice) {
        ret = stopMPP(mGscHandle);
        if (ret < 0) {
            ALOGE("failed to stop gscaler %u", mIndex);
            goto err_gsc_config;
        }

        mid_img.acquireFenceFd = mid_img.releaseFenceFd;

        ret = setCSCProperty(mGscHandle, 0, !dst_img.narrowRgb, 1);
        ret = configMPP(mGscHandle, &mid_img, &dst_img);
        if (ret < 0) {
            ALOGE("failed to configure gscaler %u", mIndex);
            goto err_gsc_config;
        }

        ret = runMPP(mGscHandle, &mid_img, &dst_img);
        if (ret < 0) {
            ALOGE("failed to run gscaler %u", mIndex);
             goto err_gsc_config;
        }
        mMidBufFence[mCurrentBuf] = mid_img.releaseFenceFd;
    }

    mSrcConfig = src_img;
    mMidConfig = mid_img;

    if (need_gsc_op_twice) {
        mDstConfig = dst_img;
    } else {
        mDstConfig = mid_img;
    }

    layer.releaseFenceFd = src_img.releaseFenceFd;

    return 0;

err_gsc_config:
    destroyMPP(mGscHandle);
    mGscHandle = NULL;
err_alloc:
    if (src_img.acquireFenceFd >= 0)
        close(src_img.acquireFenceFd);
#ifdef USES_VIRTUAL_DISPLAY
    if (isNeedBufferAlloc) {
#endif
    for (size_t i = 0; i < NUM_GSC_DST_BUFS; i++) {
       if (mDstBuffers[i]) {
           alloc_device->free(alloc_device, mDstBuffers[i]);
           mDstBuffers[i] = NULL;
       }
       if (mDstBufFence[i] >= 0) {
           close(mDstBufFence[i]);
           mDstBufFence[i] = -1;
       }
       if (mMidBuffers[i]) {
           alloc_device->free(alloc_device, mMidBuffers[i]);
           mMidBuffers[i] = NULL;
       }
       if (mMidBufFence[i] >= 0) {
           close(mMidBufFence[i]);
           mMidBufFence[i] = -1;
       }
    }
#ifdef USES_VIRTUAL_DISPLAY
    }
#endif
    memset(&mSrcConfig, 0, sizeof(mSrcConfig));
    memset(&mDstConfig, 0, sizeof(mDstConfig));
    memset(&mMidConfig, 0, sizeof(mMidConfig));
    return ret;
}

void ExynosMPP::cleanupM2M()
{
    if (!mGscHandle)
        return;

    ALOGV("closing gscaler %u", AVAILABLE_GSC_UNITS[mIndex]);

    for (size_t i = 0; i < NUM_GSC_DST_BUFS; i++) {
#ifndef FORCEFB_YUVLAYER
        if (mDstBufFence[i] >= 0)
            if (sync_wait(mDstBufFence[i], 1000) < 0)
                ALOGE("sync_wait error");
#endif
        if (mMidBufFence[i] >= 0)
            if (sync_wait(mMidBufFence[i], 1000) < 0)
                ALOGE("sync_wait error");
    }

    stopMPP(mGscHandle);
    destroyMPP(mGscHandle);
    for (size_t i = 0; i < NUM_GSC_DST_BUFS; i++) {
        if (mDstBuffers[i])
            mDisplay->mAllocDevice->free(mDisplay->mAllocDevice, mDstBuffers[i]);
        if (mDstBufFence[i] >= 0)
            close(mDstBufFence[i]);
        if (mMidBuffers[i]) {
            mDisplay->mAllocDevice->free(mDisplay->mAllocDevice, mMidBuffers[i]);
            mMidBuffers[i] = NULL;
        }
        if (mMidBufFence[i] >= 0)
            close(mMidBufFence[i]);
    }

    mGscHandle = NULL;
    memset(&mSrcConfig, 0, sizeof(mSrcConfig));
    memset(&mMidConfig, 0, sizeof(mMidConfig));
    memset(&mDstConfig, 0, sizeof(mDstConfig));
    memset(mDstBuffers, 0, sizeof(mDstBuffers));
    memset(mMidBuffers, 0, sizeof(mMidBuffers));
    mCurrentBuf = 0;
    mGSCMode = 0;
    mLastGSCLayerHandle = 0;
    
    for (size_t i = 0; i < NUM_GSC_DST_BUFS; i++) {
        mDstBufFence[i] = -1;
        mMidBufFence[i] = -1;
    }
}

void ExynosMPP::cleanupOTF()
{
    stopMPP(mGscHandle);
    mNeedReqbufs = true;
    mCountSameConfig = 0;
    mGSCMode = exynos5_gsc_map_t::GSC_NONE;
    mSrcConfig.fw = -1;
    mSrcConfig.fh = -1;
}

bool ExynosMPP::rotationSupported(bool rotation)
{
    return !rotation;
}

bool ExynosMPP::paritySupported(int w, int h)
{
    return (w % 2 == 0) && (h % 2 == 0);
}

bool ExynosMPP::isDstConfigChanged(exynos_mpp_img &c1, exynos_mpp_img &c2)
{
    return c1.x != c2.x ||
            c1.y != c2.y ||
            c1.w != c2.w ||
            c1.h != c2.h ||
            c1.format != c2.format ||
            c1.rot != c2.rot ||
            c1.narrowRgb != c2.narrowRgb ||
            c1.cacheable != c2.cacheable ||
            c1.drmMode != c2.drmMode;
}

bool ExynosMPP::isPerFrameSrcChanged(exynos_mpp_img &c1, exynos_mpp_img &c2)
{
    return false;
}

bool ExynosMPP::isPerFrameDstChanged(exynos_mpp_img &c1, exynos_mpp_img &c2)
{
    return false;
}

bool ExynosMPP::isReallocationRequired(int w, int h, exynos_mpp_img &c1, exynos_mpp_img &c2)
{
    return ALIGN(w, GSC_W_ALIGNMENT) != ALIGN(c2.fw, GSC_W_ALIGNMENT) ||
            ALIGN(h, GSC_H_ALIGNMENT) != ALIGN(c2.fh, GSC_H_ALIGNMENT) ||
            c1.format != c2.format ||
            c1.drmMode != c2.drmMode;
}

uint32_t ExynosMPP::halFormatToMPPFormat(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return HAL_PIXEL_FORMAT_BGRA_8888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return HAL_PIXEL_FORMAT_RGBA_8888;
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
        return HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
    default:
        return format;
    }
}

int ExynosMPP::minWidth(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
    switch (handle->format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
        return 32;
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        return 64;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
    default:
        return isRotated(layer) ? 32 : 64;
    }
}

int ExynosMPP::minHeight(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
    switch (handle->format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
        return 16;
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        return 32;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
    default:
        return isRotated(layer) ? 32 : 64;
    }
}

int ExynosMPP::maxWidth(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    if (isUsingMSC())
        return 8192;

    switch (handle->format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        return 4800;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
    default:
        return 2047;
    }
}

int ExynosMPP::maxHeight(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    if (isUsingMSC())
        return 8192;

    switch (handle->format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        return 3344;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
    default:
        return 2047;
    }
}

int ExynosMPP::srcXOffsetAlign(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
    switch (handle->format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
        return 2;
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
    default:
        return 4;
    }
}

int ExynosMPP::srcYOffsetAlign(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
    switch (handle->format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        return 1;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
    default:
        return isRotated(layer) ? 4 : 1;
    }
}

void *ExynosMPP::createMPP(int id, int mode, int outputMode, int drm)
{
    mppFact = new MppFactory();
    libmpp = mppFact->CreateMpp(id, mode, outputMode, drm);

    return reinterpret_cast<void *>(libmpp);
}

int ExynosMPP::configMPP(void *handle, exynos_mpp_img *src, exynos_mpp_img *dst)
{
    return libmpp->ConfigMpp(handle, src, dst);
}

int ExynosMPP::runMPP(void *handle, exynos_mpp_img *src, exynos_mpp_img *dst)
{
    return libmpp->RunMpp(handle, src, dst);
}

int ExynosMPP::stopMPP(void *handle)
{
    return libmpp->StopMpp(handle);
}

void ExynosMPP::destroyMPP(void *handle)
{
    libmpp->DestroyMpp(handle);
    delete(mppFact);
}

int ExynosMPP::setCSCProperty(void *handle, unsigned int eqAuto, unsigned int fullRange, unsigned int colorspace)
{
    return libmpp->SetCSCProperty(handle, eqAuto, fullRange, colorspace);
}

int ExynosMPP::freeMPP(void *handle)
{
    return libmpp->FreeMpp(handle);
}

bool ExynosMPP::bufferChanged(hwc_layer_1_t &layer)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
    return mSrcConfig.fw != (uint32_t)handle->stride ||
        mSrcConfig.fh != (uint32_t)handle->vstride ||
        mDstConfig.rot != (uint32_t)layer.transform;
}

bool ExynosMPP::needsReqbufs()
{
    return mNeedReqbufs;
}
