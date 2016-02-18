#ifndef HWC_UTILS_H
#define HWC_UTILS_H

#include "ExynosHWC.h"

class ExynosMPP;

inline int WIDTH(const hwc_rect &rect) { return rect.right - rect.left; }
inline int HEIGHT(const hwc_rect &rect) { return rect.bottom - rect.top; }
inline int WIDTH(const hwc_frect_t &rect) { return (int)(rect.right - rect.left); }
inline int HEIGHT(const hwc_frect_t &rect) { return (int)(rect.bottom - rect.top); }

template<typename T> inline T max(T a, T b) { return (a > b) ? a : b; }
template<typename T> inline T min(T a, T b) { return (a < b) ? a : b; }

template<typename T> void alignCropAndCenter(T &w, T &h,
        hwc_frect_t *crop, size_t alignment)
{
    double aspect = 1.0 * h / w;
    T w_orig = w, h_orig = h;

    w = ALIGN(w, alignment);
    h = round(aspect * w);
    if (crop) {
        crop->left = (w - w_orig) / 2;
        crop->top = (h - h_orig) / 2;
        crop->right = crop->left + w_orig;
        crop->bottom = crop->top + h_orig;
    }
}

inline bool intersect(const hwc_rect &r1, const hwc_rect &r2)
{
    return !(r1.left > r2.right ||
        r1.right < r2.left ||
        r1.top > r2.bottom ||
        r1.bottom < r2.top);
}

inline hwc_rect intersection(const hwc_rect &r1, const hwc_rect &r2)
{
    hwc_rect i;
    i.top = max(r1.top, r2.top);
    i.bottom = min(r1.bottom, r2.bottom);
    i.left = max(r1.left, r2.left);
    i.right = min(r1.right, r2.right);
    return i;
}

inline bool yuvConfigChanged(video_layer_config &c1, video_layer_config &c2)
{
    return c1.x != c2.x ||
            c1.y != c2.y ||
            c1.w != c2.w ||
            c1.h != c2.h ||
            c1.fw != c2.fw ||
            c1.fh != c2.fh ||
            c1.format != c2.format ||
            c1.rot != c2.rot ||
            c1.cacheable != c2.cacheable ||
            c1.drmMode != c2.drmMode ||
            c1.index != c2.index;
}

void dumpHandle(private_handle_t *h);
void dumpLayer(hwc_layer_1_t const *l);
void dumpConfig(fb_win_config &c);
void dumpMPPImage(exynos_mpp_img &c);
bool isDstCropWidthAligned(int dest_w);
bool isTransformed(const hwc_layer_1_t &layer);
bool isRotated(const hwc_layer_1_t &layer);
bool isScaled(const hwc_layer_1_t &layer);
bool isFormatSupported(int format);
bool isFormatRgb(int format);
bool isFormatYCrCb(int format);
uint8_t formatToBpp(int format);
bool isXAligned(const hwc_layer_1_t &layer, int format);
int getDrmMode(int flags);
int halFormatToV4L2Format(int format);
bool isBlendingSupported(int32_t blending);
bool isOffscreen(hwc_layer_1_t &layer, int xres, int yres);
bool isSrcCropFloat(hwc_frect &frect);
size_t visibleWidth(ExynosMPP *processor, hwc_layer_1_t &layer, int format,
        int xres);
bool compareYuvLayerConfig(int videoLayers, uint32_t index,
        hwc_layer_1_t &layer,
        video_layer_config *pre_src_data, video_layer_config *pre_dst_data);
size_t getRequiredPixels(hwc_layer_1_t &layer, int xres, int yres);
void recalculateDisplayFrame(hwc_layer_1_t &layer, int xres, int yres);

int getS3DFormat(int preset);
#endif
