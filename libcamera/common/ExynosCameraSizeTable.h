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

#ifndef EXYNOS_CAMERA_SIZE_TABLE_H
#define EXYNOS_CAMERA_SIZE_TABLE_H
#include <cutils/log.h>
#include <utils/String8.h>

namespace android {

#define SIZE_OF_LUT         11
#define SIZE_OF_RESOLUTION  3

enum EXYNOS_CAMERA_SIZE_RATIO_ID {
    SIZE_RATIO_16_9 = 0,
    SIZE_RATIO_4_3,
    SIZE_RATIO_1_1,
    SIZE_RATIO_3_2,
    SIZE_RATIO_5_4,
    SIZE_RATIO_5_3,
    SIZE_RATIO_11_9,
    SIZE_RATIO_END
};

enum SIZE_LUT_INDEX {
    RATIO_ID,
    SENSOR_W   = 1,
    SENSOR_H,
    BNS_W,
    BNS_H,
    BCROP_W,
    BCROP_H,
    BDS_W,
    BDS_H,
    TARGET_W,
    TARGET_H,
    SIZE_LUT_INDEX_END
};

#if defined(CAMERA_DISPLAY_WQHD)
#include "ExynosCameraSizeTable2P2_WQHD.h"
#else
#include "ExynosCameraSizeTable2P2_FHD.h"
#endif
#include "ExynosCameraSizeTable3L2.h"
#include "ExynosCameraSizeTable3H7.h"
#include "ExynosCameraSizeTableIMX175.h"
#include "ExynosCameraSizeTable4H5.h"

}; /* namespace android */
#endif
