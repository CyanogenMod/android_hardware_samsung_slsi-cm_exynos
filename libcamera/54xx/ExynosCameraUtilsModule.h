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

#ifndef EXYNOS_CAMERA_UTILS_MODULE_H
#define EXYNOS_CAMERA_UTILS_MODULE_H

#include <cutils/properties.h>
#include <utils/threads.h>
#include <utils/String8.h>

#include "ExynosRect.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraConfig.h"

namespace android {

void updateNodeGroupInfoMainPreview(
        int cameraId,
        camera2_node_group *node_group_info_3aa,
        camera2_node_group *node_group_info_isp,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH);

void updateNodeGroupInfoReprocessing(
        int cameraId,
        camera2_node_group *node_group_info_3aa,
        camera2_node_group *node_group_info_isp,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        bool pureBayerReprocessing);

void updateNodeGroupInfoFront(
        int cameraId,
        camera2_node_group *node_group_info_3aa,
        camera2_node_group *node_group_info_isp,
        ExynosRect bayerCropSize,
        int previewW, int previewH);

}; /* namespace android */

#endif

