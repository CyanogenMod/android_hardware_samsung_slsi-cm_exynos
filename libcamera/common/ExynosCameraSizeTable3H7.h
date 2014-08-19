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

#ifndef EXYNOS_CAMERA_LUT_3H7_H
#define EXYNOS_CAMERA_LUT_3H7_H

/* -------------------------
    SIZE_RATIO_16_9 = 0,
    SIZE_RATIO_4_3,
    SIZE_RATIO_1_1,
    SIZE_RATIO_3_2,
    SIZE_RATIO_5_4,
    SIZE_RATIO_5_3,
    SIZE_RATIO_11_9,
    SIZE_RATIO_END
----------------------------
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
-----------------------------*/

static int PREVIEW_SIZE_LUT_3H7[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      3248, 2438,       /* [sensor ] *//* w/o margin */
      3248, 2438,       /* [bns    ] */
      3248, 1826,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] */
      1920, 1080        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3248, 2438,       /* [sensor ] */
      3248, 2438,       /* [bns    ] */
      3248, 2438,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] */
      1440, 1080        /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_3H7[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      3248, 2438,       /* [sensor ] */
      3248, 2438,       /* [bns    ] */
      3248, 1826,       /* [bcrop  ] */
      3248, 1826,       /* [bds    ] */
      3248, 1826        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3248, 2438,       /* [sensor ] */
      3248, 2438,       /* [bns    ] */
      3248, 2438,       /* [bcrop  ] */
      3248, 2438,       /* [bds    ] */
      3248, 2438        /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_3H7[][SIZE_OF_LUT] =
{
    /*  16:9 (Single) */
    { SIZE_RATIO_16_9,
      3248, 2438,       /* [sensor ] */
      3248, 2438,       /* [bns    ] */
      3248, 1826,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] *//* this value is not effective */
      1920, 1080        /* [target ] *//* bds size equals target(video) size from service */
    },
    /*  4:3 (Single) */
    { SIZE_RATIO_4_3,
      3248, 2438,       /* [sensor ] */
      3248, 2438,       /* [bns    ] */
      3248, 2438,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] *//* this value is not effective */
      1440, 1080        /* [target ] *//* bds size equals target(video) size from service */
    }
};

static int VIDEO_SIZE_LUT_HIGH_SPEED_3H7[][SIZE_OF_LUT] =
{
    /*   HD_60  16:9 (Single) */
    { SIZE_RATIO_16_9,
      3248, 2438,       /* [sensor ] */
      3248, 2438,       /* [bns    ] */
      3248, 1826,       /* [bcrop  ] */
      1280,  720,       /* [bds    ] *//* this value is not effective */
      1280,  720        /* [target ] *//* bds size equals target(video) size from service */
    }
};
#endif
