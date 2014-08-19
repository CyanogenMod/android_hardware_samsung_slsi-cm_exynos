/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraUtilsModule"
#include <cutils/log.h>

#include "ExynosCameraUtilsModule.h"

namespace android {

void updateNodeGroupInfoMainPreview(
        int cameraId,
        camera2_node_group *node_group_info_3aa,
        camera2_node_group *node_group_info_isp,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH)
{
    ALOGV("Leader before (%d, %d, %d, %d)(%d, %d, %d, %d)(%d %d)",
        node_group_info_3aa->leader.input.cropRegion[0],
        node_group_info_3aa->leader.input.cropRegion[1],
        node_group_info_3aa->leader.input.cropRegion[2],
        node_group_info_3aa->leader.input.cropRegion[3],
        node_group_info_3aa->leader.output.cropRegion[0],
        node_group_info_3aa->leader.output.cropRegion[1],
        node_group_info_3aa->leader.output.cropRegion[2],
        node_group_info_3aa->leader.output.cropRegion[3],
        node_group_info_3aa->leader.request,
        node_group_info_3aa->leader.vid);

    /* Leader : 3AA : BCrop */
    node_group_info_3aa->leader.input.cropRegion[0] = bayerCropSize.x;
    node_group_info_3aa->leader.input.cropRegion[1] = bayerCropSize.y;
    node_group_info_3aa->leader.input.cropRegion[2] = bayerCropSize.w;
    node_group_info_3aa->leader.input.cropRegion[3] = bayerCropSize.h;
    node_group_info_3aa->leader.output.cropRegion[0] = node_group_info_3aa->leader.input.cropRegion[0];
    node_group_info_3aa->leader.output.cropRegion[1] = node_group_info_3aa->leader.input.cropRegion[1];
    node_group_info_3aa->leader.output.cropRegion[2] = node_group_info_3aa->leader.input.cropRegion[2];
    node_group_info_3aa->leader.output.cropRegion[3] = node_group_info_3aa->leader.input.cropRegion[3];

    /* Capture 0 : 3AC -[X] - output cropX, cropY should be Zero */
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[0] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[1] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[2] = node_group_info_3aa->leader.input.cropRegion[2];
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[3] = node_group_info_3aa->leader.input.cropRegion[3];
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[0] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[1] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[2] = node_group_info_3aa->leader.input.cropRegion[2];
    node_group_info_3aa->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[3] = node_group_info_3aa->leader.input.cropRegion[3];

    /* Capture 1 : 3AP - [BDS] */
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[0] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[1] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[2] = bayerCropSize.w;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[3] = bayerCropSize.h;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[0] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[1] = 0;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[2] = (bayerCropSize.w < bdsSize.w) ? bayerCropSize.w : bdsSize.w;
    node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[3] = (bayerCropSize.h < bdsSize.h) ? bayerCropSize.h : bdsSize.h;

    /* Leader : ISP */
    node_group_info_isp->leader.input.cropRegion[0] = 0;
    node_group_info_isp->leader.input.cropRegion[1] = 0;
    node_group_info_isp->leader.input.cropRegion[2] = node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[2];
    node_group_info_isp->leader.input.cropRegion[3] = node_group_info_3aa->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[3];
    node_group_info_isp->leader.output.cropRegion[0] = 0;
    node_group_info_isp->leader.output.cropRegion[1] = 0;
    node_group_info_isp->leader.output.cropRegion[2] = node_group_info_isp->leader.input.cropRegion[2];
    node_group_info_isp->leader.output.cropRegion[3] = node_group_info_isp->leader.input.cropRegion[3];

    /* Capture 0 : SCP - [scaling] */
#if 0 /* HACK: Driver do not support SCP scaling */
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[0] = 0;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[1] = 0;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[2] = node_group_info_isp->leader.output.cropRegion[2];
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[3] = node_group_info_isp->leader.output.cropRegion[3];
#else
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[0] = 0;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[1] = 0;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[2] = previewW;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[3] = previewH;
#endif
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].output.cropRegion[0] = 0;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].output.cropRegion[1] = 0;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].output.cropRegion[2] = previewW;
    node_group_info_isp->capture[PERFRAME_BACK_SCP_POS].output.cropRegion[3] = previewH;

    ALOGV("Leader after (%d, %d, %d, %d)(%d, %d, %d, %d)(%d %d)",
        node_group_info_3aa->leader.input.cropRegion[0],
        node_group_info_3aa->leader.input.cropRegion[1],
        node_group_info_3aa->leader.input.cropRegion[2],
        node_group_info_3aa->leader.input.cropRegion[3],
        node_group_info_3aa->leader.output.cropRegion[0],
        node_group_info_3aa->leader.output.cropRegion[1],
        node_group_info_3aa->leader.output.cropRegion[2],
        node_group_info_3aa->leader.output.cropRegion[3],
        node_group_info_3aa->leader.request,
        node_group_info_3aa->leader.vid);
}


void updateNodeGroupInfoReprocessing(
        int cameraId,
        camera2_node_group *node_group_info_3aa,
        camera2_node_group *node_group_info_isp,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        bool pureBayerReprocessing)
{
    ALOGV("Leader before (%d, %d, %d, %d)(%d, %d, %d, %d)(%d %d)",
        node_group_info_3aa->leader.input.cropRegion[0],
        node_group_info_3aa->leader.input.cropRegion[1],
        node_group_info_3aa->leader.input.cropRegion[2],
        node_group_info_3aa->leader.input.cropRegion[3],
        node_group_info_3aa->leader.output.cropRegion[0],
        node_group_info_3aa->leader.output.cropRegion[1],
        node_group_info_3aa->leader.output.cropRegion[2],
        node_group_info_3aa->leader.output.cropRegion[3],
        node_group_info_3aa->leader.request,
        node_group_info_3aa->leader.vid);

    if (pureBayerReprocessing == true) {
        /* Leader : 3AA */
        node_group_info_3aa->leader.input.cropRegion[0] = bayerCropSizePicture.x;
        node_group_info_3aa->leader.input.cropRegion[1] = bayerCropSizePicture.y;
        node_group_info_3aa->leader.input.cropRegion[2] = bayerCropSizePicture.w;
        node_group_info_3aa->leader.input.cropRegion[3] = bayerCropSizePicture.h;
        node_group_info_3aa->leader.output.cropRegion[0] = node_group_info_3aa->leader.input.cropRegion[0];
        node_group_info_3aa->leader.output.cropRegion[1] = node_group_info_3aa->leader.input.cropRegion[1];
        node_group_info_3aa->leader.output.cropRegion[2] = node_group_info_3aa->leader.input.cropRegion[2];
        node_group_info_3aa->leader.output.cropRegion[3] = node_group_info_3aa->leader.input.cropRegion[3];

        /* Capture 1 : 3AP - [BDS] */
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[0] = 0;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[1] = 0;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[2] = bayerCropSizePicture.w;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[3] = bayerCropSizePicture.h;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[0] = 0;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[1] = 0;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[2] = (bayerCropSizePicture.w < bdsSize.w) ? bayerCropSizePicture.w : bdsSize.w;
        node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[3] = (bayerCropSizePicture.h < bdsSize.h) ? bayerCropSizePicture.h : bdsSize.h;

        /* Leader : ISP */
        node_group_info_isp->leader.input.cropRegion[0] = 0;
        node_group_info_isp->leader.input.cropRegion[1] = 0;
        node_group_info_isp->leader.input.cropRegion[2] = node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[2];
        node_group_info_isp->leader.input.cropRegion[3] = node_group_info_3aa->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[3];
        node_group_info_isp->leader.output.cropRegion[0] = 0;
        node_group_info_isp->leader.output.cropRegion[1] = 0;
        node_group_info_isp->leader.output.cropRegion[2] = node_group_info_isp->leader.input.cropRegion[2];
        node_group_info_isp->leader.output.cropRegion[3] = node_group_info_isp->leader.input.cropRegion[3];
    } else {
        /* Leader : ISP */
        node_group_info_isp->leader.input.cropRegion[0] = bayerCropSizePicture.x;
        node_group_info_isp->leader.input.cropRegion[1] = bayerCropSizePicture.y;
        node_group_info_isp->leader.input.cropRegion[2] = bayerCropSizePicture.w;
        node_group_info_isp->leader.input.cropRegion[3] = bayerCropSizePicture.h;
        node_group_info_isp->leader.output.cropRegion[0] = 0;
        node_group_info_isp->leader.output.cropRegion[1] = 0;
        node_group_info_isp->leader.output.cropRegion[2] = node_group_info_isp->leader.input.cropRegion[2];
        node_group_info_isp->leader.output.cropRegion[3] = node_group_info_isp->leader.input.cropRegion[3];
    }

    /* Capture 1 : SCC */
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0] = node_group_info_isp->leader.output.cropRegion[0];
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1] = node_group_info_isp->leader.output.cropRegion[1];
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info_isp->leader.output.cropRegion[2];
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info_isp->leader.output.cropRegion[3];
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0] = 0;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1] = 0;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2] = pictureW;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3] = pictureH;

    ALOGV("Leader after (%d, %d, %d, %d)(%d, %d, %d, %d)(%d %d)",
        node_group_info_3aa->leader.input.cropRegion[0],
        node_group_info_3aa->leader.input.cropRegion[1],
        node_group_info_3aa->leader.input.cropRegion[2],
        node_group_info_3aa->leader.input.cropRegion[3],
        node_group_info_3aa->leader.output.cropRegion[0],
        node_group_info_3aa->leader.output.cropRegion[1],
        node_group_info_3aa->leader.output.cropRegion[2],
        node_group_info_3aa->leader.output.cropRegion[3],
        node_group_info_3aa->leader.request,
        node_group_info_3aa->leader.vid);
}

void updateNodeGroupInfoFront(
        int cameraId,
        camera2_node_group *node_group_info_3aa,
        camera2_node_group *node_group_info_isp,
        ExynosRect bayerCropSize,
        int previewW, int previewH)
{

    /* Leader : 3AA : BCrop */
    node_group_info_3aa->leader.input.cropRegion[0] = bayerCropSize.x;
    node_group_info_3aa->leader.input.cropRegion[1] = bayerCropSize.y;
    node_group_info_3aa->leader.input.cropRegion[2] = bayerCropSize.w;
    node_group_info_3aa->leader.input.cropRegion[3] = bayerCropSize.h;
    node_group_info_3aa->leader.output.cropRegion[0] = node_group_info_3aa->leader.input.cropRegion[0];
    node_group_info_3aa->leader.output.cropRegion[1] = node_group_info_3aa->leader.input.cropRegion[1];
    node_group_info_3aa->leader.output.cropRegion[2] = node_group_info_3aa->leader.input.cropRegion[2];
    node_group_info_3aa->leader.output.cropRegion[3] = node_group_info_3aa->leader.input.cropRegion[3];

    /* Capture 0 :3AP -- Like Back 3AC IN = ISP IN = SCC OUT : NO BDS */
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[0] = 0;
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[1] = 0;
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[2] = node_group_info_3aa->leader.output.cropRegion[2];
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[3] = node_group_info_3aa->leader.output.cropRegion[3];
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[0] = 0;
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[1] = 0;
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[2] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[2];
    node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[3] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[3];

    /* Leader : ISP */
    node_group_info_isp->leader.input.cropRegion[0] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[0];
    node_group_info_isp->leader.input.cropRegion[1] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[1];
    node_group_info_isp->leader.input.cropRegion[2] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[2];
    node_group_info_isp->leader.input.cropRegion[3] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].input.cropRegion[3];
    node_group_info_isp->leader.output.cropRegion[0] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[0];
    node_group_info_isp->leader.output.cropRegion[1] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[1];
    node_group_info_isp->leader.output.cropRegion[2] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[2];
    node_group_info_isp->leader.output.cropRegion[3] = node_group_info_3aa->capture[PERFRAME_FRONT_3AP_POS].output.cropRegion[3];

    /* Capture 0 : SCC */
    /* Capture 1 : SCP */
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].input.cropRegion[0] = node_group_info_isp->leader.output.cropRegion[0];
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].input.cropRegion[1] = node_group_info_isp->leader.output.cropRegion[1];
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].input.cropRegion[2] = node_group_info_isp->leader.output.cropRegion[2];
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].input.cropRegion[3] = node_group_info_isp->leader.output.cropRegion[3];
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[0] = 0;
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[1] = 0;
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[2] = previewW;
    node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[3] = previewH;

    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[0] = node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[0];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[1] = node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[1];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[2] = node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[2];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[3] = node_group_info_isp->capture[PERFRAME_FRONT_SCC_POS].output.cropRegion[3];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].output.cropRegion[0] = node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[0];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].output.cropRegion[1] = node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[1];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].output.cropRegion[2] = node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[2];
    node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].output.cropRegion[3] = node_group_info_isp->capture[PERFRAME_FRONT_SCP_POS].input.cropRegion[3];
}

}; /* namespace android */
