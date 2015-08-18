#ifndef EXYNOS_LEGACY_HDMI_H
#define EXYNOS_LEGACY_HDMI_H

#include "ExynosHWC.h"
#include "ExynosDisplay.h"
#include <linux/videodev2.h>
#include <linux/v4l2-dv-timings.h>

#define NUM_VIRT_OVER_HDMI 5
#define MAX_HDMI_VIDEO_LAYERS 1

/* except background layer */
const size_t NUM_HDMI_WINDOWS = 4;
const size_t NO_HDMI_FB_NEEDED = NUM_HDMI_WINDOWS + 1;
const size_t MAX_NUM_HDMI_DMA_CH = 4;
const int HDMI_DMA_CH_IDX[NUM_HW_WINDOWS] = {0, 1, 2, 3};
const uint32_t HDMI_DMA_CH_BW_SET[MAX_NUM_HDMI_DMA_CH] = {1920 * 1080, 1920 * 1080, 1920 * 1080, 1920 * 1080};
const uint32_t HDMI_DMA_CH_OVERLAP_CNT_SET[MAX_NUM_HDMI_DMA_CH] = {0, 0, 0, 0};

#define SUPPORTED_DV_TIMINGS_NUM        25
struct preset_index_mapping {
	int preset;
	int dv_timings_index;
};

const struct preset_index_mapping preset_index_mappings[SUPPORTED_DV_TIMINGS_NUM] = {
	{V4L2_DV_480P59_94, 0},
	{V4L2_DV_576P50, 1},
	{V4L2_DV_720P50, 2},
	{V4L2_DV_720P60, 3},
	{V4L2_DV_1080I50, 4},
	{V4L2_DV_1080I60, 5},
	{V4L2_DV_1080P24, 6},
	{V4L2_DV_1080P25, 7},
	{V4L2_DV_1080P30, 8},
	{V4L2_DV_1080P50, 9},
	{V4L2_DV_1080P60, 10},
	{V4L2_DV_2160P24, 11},
	{V4L2_DV_720P60_SB_HALF, 12},
	{V4L2_DV_720P60_TB, 13},
	{V4L2_DV_720P50_SB_HALF, 14},
	{V4L2_DV_720P50_TB, 15},
	{V4L2_DV_1080P24_FP, 16},
	{V4L2_DV_1080P24_SB_HALF, 17},
	{V4L2_DV_1080P24_TB, 18},
	{V4L2_DV_1080I60_SB_HALF, 19},
	{V4L2_DV_1080I50_SB_HALF, 20},
	{V4L2_DV_1080P60_SB_HALF, 21},
	{V4L2_DV_1080P60_TB, 22},
	{V4L2_DV_1080P30_SB_HALF, 23},
	{V4L2_DV_1080P30_TB, 24}
};

class ExynosExternalDisplay : public ExynosDisplay {
    public:
        /* Methods */
        ExynosExternalDisplay(struct exynos5_hwc_composer_device_1_t *pdev);
        ~ExynosExternalDisplay();

        void setHdmiStatus(bool status);

        bool isPresetSupported(unsigned int preset);
        int getConfig();
        int enable();
        void disable();
        void setPreset(int preset);
        int convert3DTo2D(int preset);
        void setHdcpStatus(int status);
        void setAudioChannel(uint32_t channels);
        uint32_t getAudioChannel();
        int getCecPaddr();

        virtual int openHdmi();
        virtual int blank();
        virtual int prepare(hwc_display_contents_1_t* contents);
        virtual int set(hwc_display_contents_1_t* contents);

        /* Fields */
        ExynosMPPModule         *mMPPs[1];

        bool                    mEnabled;
        bool                    mBlanked;

        const void              *mLastLayerHandles[NUM_VIRT_OVER_HDMI];
        int                     mVirtualOverlayFlag;

        exynos5_hwc_post_data_t  mPostData;
        bool                     mRetry;
        int                      mForceOverlayLayerIndex;
        int                      mYuvLayers;
        bool                     mFbNeeded;
        size_t                   mFirstFb;
        size_t                   mLastFb;
        bool                     mGscUsed;
        int                      mCurrentGscIndex;
        bool                     mBypassSkipStaticLayer;
        int                      mLastOverlayWindowIndex;
        int                      mLastOverlayLayerIndex;
        int                      mGscLayers;
        size_t                   mLastFbWindow;

        uint32_t                 mDmaChannelMaxBandwidth[MAX_NUM_HDMI_DMA_CH];
        uint32_t                 mDmaChannelMaxOverlapCount[MAX_NUM_HDMI_DMA_CH];
        struct s3c_fb_win_config mLastConfig[S3C_FB_MAX_WIN];
        exynos5_gsc_map_t        mLastGscMap[NUM_HW_WINDOWS];
        const void               *mLastHandles[NUM_HDMI_WINDOWS];
        bool                    mUseSubtitles;

    protected:
        void determineYuvOverlay(hwc_display_contents_1_t *contents);
        void determineSupportedOverlays(hwc_display_contents_1_t *contents);
        void determineBandwidthSupport(hwc_display_contents_1_t *contents);
        void assignWindows(hwc_display_contents_1_t *contents);
        bool assignGscLayer(hwc_layer_1_t &layer, int index, int nextWindow);
        int postGscM2M(hwc_layer_1_t &layer, struct s3c_fb_win_config *config, int win_map, int index);
        void configureHandle(private_handle_t *handle, hwc_frect_t &sourceCrop,
                hwc_rect_t &displayFrame, int32_t blending, int32_t planeAlpha, int fence_fd, s3c_fb_win_config &cfg);
        void skipStaticLayers(hwc_display_contents_1_t *contents);
        void handleStaticLayers(hwc_display_contents_1_t *contents, struct s3c_fb_win_config_data &win_data, int tot_ovly_wins);
        void skipUILayers(hwc_display_contents_1_t *contents);
        void cleanupGscs();
        int clearDisplay();
        int getDVTimingsIndex(int preset);

        //virtual void configureOverlay(hwc_layer_1_t *layer, s3c_fb_win_config &cfg);
        //virtual bool isOverlaySupported(hwc_layer_1_t &layer, size_t i);
        //virtual int postFrame(hwc_display_contents_1_t *contents);
        void configureOverlay(hwc_layer_1_t *layer, s3c_fb_win_config &cfg);
        bool isOverlaySupported(hwc_layer_1_t &layer, size_t i);
        int postFrame(hwc_display_contents_1_t *contents);
};

#endif
