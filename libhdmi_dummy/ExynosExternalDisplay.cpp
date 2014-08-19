#include "ExynosMPPModule.h"
#include "ExynosExternalDisplay.h"

ExynosExternalDisplay::ExynosExternalDisplay(struct exynos5_hwc_composer_device_1_t *pdev) :
    ExynosDisplay(1)
{
    this->mHwc = pdev;
    mMPPs[0] = new ExynosMPPModule(this, HDMI_GSC_IDX);
    mEnabled = false;
    mBlanked = false;
    mUseSubtitles = false;

}

ExynosExternalDisplay::~ExynosExternalDisplay()
{
    delete mMPPs[0];
}

int ExynosExternalDisplay::prepare(hwc_display_contents_1_t *contents)
{
    return 0;
}

int ExynosExternalDisplay::set(hwc_display_contents_1_t *contents)
{
    return 0;
}

int ExynosExternalDisplay::openHdmi()
{
    return 0;
}

void ExynosExternalDisplay::setHdmiStatus(bool status)
{
}

bool ExynosExternalDisplay::isPresetSupported(unsigned int preset)
{
    return false;
}

int ExynosExternalDisplay::getConfig()
{
    return 0;
}

int ExynosExternalDisplay::enableLayer(hdmi_layer_t &hl)
{
    return 0;
}

void ExynosExternalDisplay::disableLayer(hdmi_layer_t &hl)
{
}

int ExynosExternalDisplay::enable()
{
    return 0;
}

void ExynosExternalDisplay::disable()
{
}

int ExynosExternalDisplay::output(hdmi_layer_t &hl, hwc_layer_1_t &layer, private_handle_t *h, int acquireFenceFd, int *releaseFenceFd)
{
    return 0;
}

void ExynosExternalDisplay::skipStaticLayers(hwc_display_contents_1_t *contents, int ovly_idx)
{
}

void ExynosExternalDisplay::setPreset(int preset)
{
}

int ExynosExternalDisplay::convert3DTo2D(int preset)
{
    return 0;
}

void ExynosExternalDisplay::calculateDstRect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect)
{
}

void ExynosExternalDisplay::setHdcpStatus(int status)
{
}

void ExynosExternalDisplay::setAudioChannel(uint32_t channels)
{
}

uint32_t ExynosExternalDisplay::getAudioChannel()
{
    return 0;
}

int ExynosExternalDisplay::blank()
{
    return 0;
}
