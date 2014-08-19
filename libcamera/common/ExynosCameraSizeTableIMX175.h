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

#ifndef EXYNOS_CAMERA_LUT_IMX175_H
#define EXYNOS_CAMERA_LUT_IMX175_H

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

static int PREVIEW_SIZE_LUT_IMX175[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      3264, 1836,       /* [sensor ] *//* w/o margin */
      3264, 1836,       /* [bns    ] *//* if sensor do not support bns, set this value equals sensor */
      3264, 1836,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] */
      1920, 1080        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 2448,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] */
      1440, 1080        /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      2448, 2448,       /* [bcrop  ] */
      1080, 1080,       /* [bds    ] */
      1080, 1080        /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 2176,       /* [bcrop  ] */
      1616, 1080,       /* [bds    ] *//* w=1620, Reduced for 8 pixel align */
      1616, 1080        /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3060, 2448,       /* [bcrop  ] */
      1352, 1080,       /* [bds    ] *//* w=1350, Increased for 8 pixel align */
      1352, 1080        /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 1958,       /* [bcrop  ] */
      1800, 1080,       /* [bds    ] */
      1800, 1080        /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      2992, 2448,       /* [bcrop  ] */
      1320, 1080,       /* [bds    ] */
      1320, 1080        /* [target ] */
    }
};

static int PICTURE_SIZE_LUT_IMX175[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      3264, 1836,       /* [sensor ] */
      3264, 1836,       /* [bns    ] */
      3264, 1836,       /* [bcrop  ] */
      3264, 1836,       /* [bds    ] */
      3264, 1836        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 2448,       /* [bcrop  ] */
      3264, 2448,       /* [bds    ] */
      3264, 2448        /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      2448, 2448,       /* [bcrop  ] */
      2448, 2448,       /* [bds    ] */
      2448, 2448        /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_IMX175[][SIZE_OF_LUT] =
{
    /*  16:9 (Single) */
    { SIZE_RATIO_16_9,
      3264, 1836,       /* [sensor ] */
      3264, 1836,       /* [bns    ] */
      3264, 1836,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] */
      1920, 1080        /* [target ] */
    },
    /*  4:3 (Single) */
    { SIZE_RATIO_4_3,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 2448,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] */
      1440, 1080        /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      2448, 2448,       /* [bcrop  ] */
      1080, 1080,       /* [bds    ] */
      1080, 1080        /* [target ] */
    },
    /*  3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 2176,       /* [bcrop  ] */
      1616, 1080,       /* [bds    ] *//* w=1620, Reduced for 8 pixel align */
      1616, 1080        /* [target ] */
    },
    /*  5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3060, 2448,       /* [bcrop  ] */
      1352, 1080,       /* [bds    ] *//* w=1350, Increased for 8 pixel align */
      1352, 1080        /* [target ] */
    },
    /*  5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      3264, 1958,       /* [bcrop  ] */
      1800, 1080,       /* [bds    ] */
      1800, 1080        /* [target ] */
    },
    /*  11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
      3264, 2448,       /* [sensor ] */
      3264, 2448,       /* [bns    ] */
      2992, 2448,       /* [bcrop  ] */
      1320, 1080,       /* [bds    ] */
      1320, 1080        /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_HIGH_SPEED_IMX175[][SIZE_OF_LUT] =
{
    /*   HD_60  16:9 (Single) */
    { SIZE_RATIO_16_9,
      1624,  914,       /* [sensor ] */
      1624,  914,       /* [bns    ] */
      1624,  914,       /* [bcrop  ] */
      1280,  720,       /* [bds    ] */
      1280,  720        /* [target ] */
    },
    /*  HD_120  4:3 (Single) */
    { SIZE_RATIO_4_3,
       800,  450,       /* [sensor ] */
       800,  450,       /* [bns    ] */
       800,  450,       /* [bcrop  ] */
       800,  450,       /* [bds    ] */
       800,  450        /* [target ] */
    }
};
#endif
