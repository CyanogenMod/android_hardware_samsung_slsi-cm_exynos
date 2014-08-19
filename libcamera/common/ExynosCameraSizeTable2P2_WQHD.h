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

#ifndef EXYNOS_CAMERA_LUT_2P2_H
#define EXYNOS_CAMERA_LUT_2P2_H

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

static int PREVIEW_SIZE_LUT_2P2_BNS[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      5312, 2990,       /* [sensor ] *//* w/o margin */
      2648, 1490,       /* [bns    ] */
      2648, 1490,       /* [bcrop  ] */
      2560, 1440,       /* [bds    ] *//* WQHD display */
      2560, 1440        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3984, 2990,       /* [sensor ] */
      1984, 1490,       /* [bns    ] */
      1986, 1490,       /* [bcrop  ] */
      1920, 1440,       /* [bds    ] */
      1920, 1440        /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2992, 2990,       /* [sensor ] */
      1488, 1490,       /* [bns    ] */
      1488, 1488,       /* [bcrop  ] */
      1440, 1440,       /* [bds    ] */
      1440, 1440        /* [target ] */
    },
    /*	3:2 (Single, Dual) */
    { SIZE_RATIO_3_2,
      5312, 2990,		/* [sensor ] */
      2648, 1490,		/* [bns    ] */
      2232, 1488,		/* [bcrop  ] */
      2160, 1440,		/* [bds    ] */
      2160, 1440		/* [target ] */
    },
    /*	5:4 (Single, Dual) */
    { SIZE_RATIO_5_4,
      3984, 2990,		/* [sensor ] */
      1984, 1490,		/* [bns    ] */
      1860, 1490,		/* [bcrop  ] */
      1800, 1440,		/* [bds    ] */
      1800, 1440		/* [target ] */
    },
    /*	5:3 (Single, Dual) */
    { SIZE_RATIO_5_3,
      5312, 2990,		/* [sensor ] */
      2648, 1490,		/* [bns    ] */
      2484, 1490,		/* [bcrop  ] */
      2400, 1440,		/* [bds    ] */
      2400, 1440		/* [target ] */
    },
    /*	11:9 (Single, Dual) */
    { SIZE_RATIO_11_9,
      3984, 2990,		/* [sensor ] */
      1984, 1490,		/* [bns    ] */
      1820, 1490,		/* [bcrop  ] */
      1760, 1440,		/* [bds    ] */
      1760, 1440		/* [target ] */
    }
};

static int PICTURE_SIZE_LUT_2P2[][SIZE_OF_LUT] =
{
    /* 16:9 (Single, Dual) */
    { SIZE_RATIO_16_9,
      5312, 2990,       /* [sensor ] */
      5312, 2990,       /* [bns    ] */
      5312, 2988,       /* [bcrop  ] */
      5312, 2988,       /* [bds    ] */
      5312, 2988        /* [target ] */
    },
    /*  4:3 (Single, Dual) */
    { SIZE_RATIO_4_3,
      3984, 2990,       /* [sensor ] */
      3984, 2990,       /* [bns    ] */
      3984, 2988,       /* [bcrop  ] */
      3984, 2988,       /* [bds    ] */
      3984, 2988        /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2992, 2990,       /* [sensor ] */
      2992, 2990,       /* [bns    ] */
      2988, 2988,       /* [bcrop  ] */
      2988, 2988,       /* [bds    ] */
      2988, 2988        /* [target ] */
    }
};

static int VIDEO_SIZE_LUT_2P2_BNS[][SIZE_OF_LUT] =
{
    /*  16:9 (Single) */
    { SIZE_RATIO_16_9,
      5312, 2990,       /* [sensor ] */
      5312, 2990,       /* [bns    ] */
      5312, 2988,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] *//* UHD (3840x2160) special handling in ExynosCameraParameters class */
      1920, 1080        /* [target ] */
    },
    /*  4:3 (Single) */
    { SIZE_RATIO_4_3,
      3984, 2990,       /* [sensor ] */
      3984, 2990,       /* [bns    ] */
      3984, 2988,       /* [bcrop  ] */
      1440, 1080,       /* [bds    ] */
      1440, 1080        /* [target ] */
    },
    /*  1:1 (Single, Dual) */
    { SIZE_RATIO_1_1,
      2992, 2990,       /* [sensor ] */
      2992, 2990,       /* [bns    ] */
      1490, 1490,       /* [bcrop  ] */
      1440, 1440,       /* [bds    ] */
      1440, 1440        /* [target ] */
    },
    /*	3:2 (Single) */
    { SIZE_RATIO_3_2,
      3984, 2990,		/* [sensor ] */
      3984, 2990,		/* [bns    ] */
      3984, 2656,		/* [bcrop  ] */
      1616, 1080,		/* [bds    ] *//* w=1620, Reduced for 8 pixel align */
      1616, 1080		/* [target ] */
    },
    /*	5:4 (Single) */
    { SIZE_RATIO_5_4,
      3984, 2990,		/* [sensor ] */
      3984, 2990,		/* [bns    ] */
      3732, 2988,		/* [bcrop  ] */
      1344, 1080,		/* [bds    ] *//* w=1350, Reduced for 8 pixel align */
      1344, 1080		/* [target ] */
    },
    /*	5:3 (Single) */
    { SIZE_RATIO_5_3,
      5312, 2990,		/* [sensor ] */
      5312, 2990,		/* [bns    ] */
      4980, 2988,		/* [bcrop  ] */
      1800, 1080,		/* [bds    ] */
      1800, 1080		/* [target ] */
    },
    /*	11:9 (Single) */
    { SIZE_RATIO_11_9,
      3984, 2990,		/* [sensor ] */
      3984, 2990,		/* [bns    ] */
      3652, 2988,		/* [bcrop  ] */
      1320, 1080,		/* [bds    ] */
      1320, 1080		/* [target ] */
    }
};

static int VIDEO_SIZE_LUT_HIGH_SPEED_2P2_BNS[][SIZE_OF_LUT] =
{
    /*	FHD_60	16:9 (Single) */
    { SIZE_RATIO_16_9,
      2648, 1490,       /* [sensor ] */
      2648, 1490,       /* [bns    ] */
      2648, 1490,       /* [bcrop  ] */
      1920, 1080,       /* [bds    ] */
      1920, 1080        /* [target ] */
    },
    /*	 HD_120  16:9 (Single) */
    { SIZE_RATIO_16_9,
      1312,  738,       /* [sensor ] */
      1312,  738,       /* [bns    ] */
      1312,  738,       /* [bcrop  ] */
      1280,  720,       /* [bds    ] */
      1280,  720        /* [target ] */
    },
    /* WVGA_300  5:3 (Single) */
    { SIZE_RATIO_16_9,
       808,  486,       /* [sensor ] */
       808,  486,       /* [bns    ] */
       808,  486,       /* [bcrop  ] */
       800,  480,       /* [bds    ] */
       800,  480        /* [target ] */
    }
};
#endif
