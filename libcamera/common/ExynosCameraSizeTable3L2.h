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

#ifndef EXYNOS_CAMERA_LUT_3L2_H
#define EXYNOS_CAMERA_LUT_3L2_H

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

static int PREVIEW_SIZE_LUT_3L2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] *//* w/o margin */
      4128, 3096,       /* [bns    ] */
      4128, 2322,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] */
      1920, 1080        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 3096,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] */
      1440, 1080        /* [target ] */
    }
};

static int PREVIEW_SIZE_LUT_3L2_BNS[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] *//* w/o margin */
      2056, 1542,       /* [bns    ] */
      2048, 1152,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] */
      1920, 1080        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      4128, 3096,       /* [sensor ] */
      2056, 1542,       /* [bns    ] */
      2056, 1542,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] */
      1440, 1080        /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_3L2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 2322,       /* [bcrop  ] */
      4128, 2322,       /* [bds    ] */
      4128, 2322        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 3096,       /* [bcrop  ] */
      4128, 3096,       /* [bds    ] */
      4128, 3096        /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_3L2[][SIZE_OF_LUT] =
{
    /*  16:9 (Single) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 2322,       /* [bcrop  ] */
      3840, 2160,       /* [bds    ] *//* this value is not effective */
      3840, 2160        /* [target ] *//* bds size equals target(video) size from service */
    },
    /*  4:3 (Single) */
    { SIZE_RATIO_4_3,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 3096,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] *//* this value is not effective */
      1440, 1080        /* [target ] *//* bds size equals target(video) size from service */
    }
};

static int VIDEO_SIZE_LUT_3L2_BNS[][SIZE_OF_LUT] =
{
    /*  16:9 (Single) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      2056, 1542,       /* [bns    ] */
      2048, 1152,       /* [bcrop  ] */
      3840, 2160,       /* [bds    ] *//* this value is not effective */
      3840, 2160        /* [target ] *//* bds size equals target(video) size from service */
    },
    /*  4:3 (Single) */
    { SIZE_RATIO_4_3,
      4128, 3096,       /* [sensor ] */
      2056, 1542,       /* [bns    ] */
      2056, 1542,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] *//* this value is not effective */
      1440, 1080        /* [target ] *//* bds size equals target(video) size from service */
    }
};

static int VIDEO_SIZE_LUT_HIGH_SPEED_3L2[][SIZE_OF_LUT] =
{
    /*  FHD_60  16:9 (Single) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 2322,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] *//* this value is not effective */
      1920, 1080        /* [target ] *//* bds size equals target(video) size from service */
    },
    /*   HD_120  16:9 (Single) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      4128, 3096,       /* [bns    ] */
      4128, 2322,       /* [bcrop  ] */
      1280,  720,       /* [bds    ] *//* this value is not effective */
      1280,  720        /* [target ] *//* bds size equals target(video) size from service */
    }
};

static int VIDEO_SIZE_LUT_HIGH_SPEED_3L2_BNS[][SIZE_OF_LUT] =
{
    /*  FHD_60  16:9 (Single) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      2056, 1542,       /* [bns    ] */
      2048, 1152,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] *//* this value is not effective */
      1920, 1080        /* [target ] *//* bds size equals target(video) size from service */
    },
    /*   HD_120  16:9 (Single) */
    { SIZE_RATIO_16_9,
      4128, 3096,       /* [sensor ] */
      2056, 1542,       /* [bns    ] */
      2056, 1542,       /* [bcrop  ] */
      1280,  720,       /* [bds    ] *//* this value is not effective */
      1280,  720        /* [target ] *//* bds size equals target(video) size from service */
    }
};
#endif
