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
#define LOG_TAG "ExynosCameraUtils"
#include <cutils/log.h>

#include "ExynosCameraUtils.h"

namespace android {

status_t getCropRectAlign(
        int  src_w,  int   src_h,
        int  dst_w,  int   dst_h,
        int *crop_x, int *crop_y,
        int *crop_w, int *crop_h,
        int align_w, int align_h,
        int zoom)
{
    *crop_w = src_w;
    *crop_h = src_h;

    if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        ALOGE("ERR(%s):width or height values is 0, src(%dx%d), dst(%dx%d)",
                 __func__, src_w, src_h, dst_w, dst_h);
        return BAD_VALUE;
    } else if (src_w < dst_w || src_h < dst_h) {
        ALOGE("ERR(%s):destination width or height values is bigger then src(%dx%d): dst(%dx%d)",
                 __func__, src_w, src_h, dst_w, dst_h);
        return BAD_VALUE;
    }

    /* Calculation aspect ratio */
    if (   src_w != dst_w
        || src_h != dst_h) {
        float src_ratio = 1.0f;
        float dst_ratio = 1.0f;

        /* ex : 1024 / 768 */
        src_ratio = ROUND_OFF_HALF(((float)src_w / (float)src_h), 2);

        /* ex : 352  / 288 */
        dst_ratio = ROUND_OFF_HALF(((float)dst_w / (float)dst_h), 2);

        if (src_w == dst_w || src_h == dst_h) {
            *crop_w = dst_w;
            *crop_h = dst_h;
        } else if (dst_ratio <= src_ratio) {
            /* shrink w */
            *crop_w = src_h * ((float)dst_w / (float)dst_h);
            *crop_h = src_h;
        } else {
            /* shrink h */
            *crop_w = src_w;
            *crop_h = src_w / ((float)dst_w / (float)dst_h);
        }
    }

    /* Calculation zoom */
    if (zoom != 0) {
        float zoomLevel = ((float)zoom + 10.00) / 10.00;
        *crop_w = (int)((float)*crop_w / zoomLevel);
        *crop_h = (int)((float)*crop_h / zoomLevel);
    }

    *crop_w = ALIGN_UP(*crop_w, align_w);
    *crop_h = ALIGN_UP(*crop_h, align_h);

    if (*crop_w > src_w)
        *crop_w = ALIGN_DOWN(src_w, align_w);
    if (*crop_h > src_h)
        *crop_h = ALIGN_DOWN(src_h, align_h);

    *crop_x = (src_w - *crop_w) >> 1;
    *crop_y = (src_h - *crop_h) >> 1;

    if (*crop_x < 0 || *crop_y < 0) {
        ALOGE("ERR(%s):crop size too big (%d, %d, %d, %d)",
                 __func__, *crop_x, *crop_y, *crop_w, *crop_h);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

uint32_t bracketsStr2Ints(
        char *str,
        uint32_t num,
        ExynosRect2 *rect2s,
        int *weights,
        int mode)
{
    char *curStr = str;
    char buf[128];
    char *bracketsOpen;
    char *bracketsClose;

    int tempArray[5];
    uint32_t validFocusedAreas = 0;
    bool isValid;
    bool nullArea = false;
    isValid = true;

    for (uint32_t i = 0; i < num + 1; i++) {
        if (curStr == NULL) {
            if (i != num) {
                nullArea = false;
            }
            break;
        }

        bracketsOpen = strchr(curStr, '(');
        if (bracketsOpen == NULL) {
            if (i != num) {
                nullArea = false;
            }
            break;
        }

        bracketsClose = strchr(bracketsOpen, ')');
        if (bracketsClose == NULL) {
            ALOGE("ERR(%s):subBracketsStr2Ints(%s) fail", __func__, buf);
            if (i != num) {
                nullArea = false;
            }
            break;
        } else if (i == num) {
            return 0;
        }

        strncpy(buf, bracketsOpen, bracketsClose - bracketsOpen + 1);
        buf[bracketsClose - bracketsOpen + 1] = 0;

        if (subBracketsStr2Ints(5, buf, tempArray) == false) {
            nullArea = false;
            break;
        }

        rect2s[i].x1 = tempArray[0];
        rect2s[i].y1 = tempArray[1];
        rect2s[i].x2 = tempArray[2];
        rect2s[i].y2 = tempArray[3];
        weights[i] = tempArray[4];

        if (mode) {
            isValid = true;

            for (int j = 0; j < 4; j++) {
                if (tempArray[j] < -1000 || tempArray[j] > 1000)
                    isValid = false;
            }

            if (tempArray[4] < 0 || tempArray[4] > 1000)
                isValid = false;

            if (!rect2s[i].x1 && !rect2s[i].y1 && !rect2s[i].x2 && !rect2s[i].y2 && !weights[i])
                nullArea = true;
            else if (weights[i] == 0)
                isValid = false;
            else if (!(tempArray[0] == 0 && tempArray[2] == 0) && tempArray[0] >= tempArray[2])
                isValid = false;
            else if (!(tempArray[1] == 0 && tempArray[3] == 0) && tempArray[1] >= tempArray[3])
                isValid = false;
            else if (!(tempArray[0] == 0 && tempArray[2] == 0) && (tempArray[1] == 0 && tempArray[3] == 0))
                isValid = false;
            else if ((tempArray[0] == 0 && tempArray[2] == 0) && !(tempArray[1] == 0 && tempArray[3] == 0))
                isValid = false;

            if (isValid)
                validFocusedAreas++;
            else
                return 0;
        } else {
            if (rect2s[i].x1 || rect2s[i].y1 || rect2s[i].x2 || rect2s[i].y2 || weights[i])
                validFocusedAreas++;
        }

        curStr = bracketsClose;
    }
    if (nullArea && mode)
        validFocusedAreas = num;

    if (validFocusedAreas == 0)
        validFocusedAreas = 1;

    return validFocusedAreas;
}

bool subBracketsStr2Ints(int num, char *str, int *arr)
{
    if (str == NULL || arr == NULL) {
        ALOGE("ERR(%s):str or arr is NULL", __func__);
        return false;
    }

    /* ex : (-10,-10,0,0,300) */
    char buf[128];
    char *bracketsOpen;
    char *bracketsClose;
    char *tok;

    bracketsOpen = strchr(str, '(');
    if (bracketsOpen == NULL) {
        ALOGE("ERR(%s):no '('", __func__);
        return false;
    }

    bracketsClose = strchr(bracketsOpen, ')');
    if (bracketsClose == NULL) {
        ALOGE("ERR(%s):no ')'", __func__);
        return false;
    }

    strncpy(buf, bracketsOpen + 1, bracketsClose - bracketsOpen + 1);
    buf[bracketsClose - bracketsOpen + 1] = 0;

    tok = strtok(buf, ",");
    if (tok == NULL) {
        ALOGE("ERR(%s):strtok(%s) fail", __func__, buf);
        return false;
    }

    arr[0] = atoi(tok);

    for (int i = 1; i < num; i++) {
        tok = strtok(NULL, ",");
        if (tok == NULL) {
            if (i < num - 1) {
                ALOGE("ERR(%s):strtok() (index : %d, num : %d) fail", __func__, i, num);
                return false;
            }
            break;
        }

        arr[i] = atoi(tok);
    }

    return true;
}

void convertingRectToRect2(ExynosRect *rect, ExynosRect2 *rect2)
{
    rect2->x1 = rect->x;
    rect2->y1 = rect->y;
    rect2->x2 = rect->x + rect->w;
    rect2->y2 = rect->y + rect->h;
}

void convertingRect2ToRect(ExynosRect2 *rect2, ExynosRect *rect)
{
    rect->x = rect2->x1;
    rect->y = rect2->y1;
    rect->w = rect2->x2 - rect2->x1;
    rect->h = rect2->y2 - rect2->y1;
}

bool isRectNull(ExynosRect *rect)
{
    if (   rect->x == 0
        && rect->y == 0
        && rect-> w == 0
        && rect->h == 0
        && rect->fullW == 0
        && rect->fullH == 0
        && rect->colorFormat == 0)
        return true;
    
    return false;
}

bool isRectNull(ExynosRect2 *rect2)
{
    if (   rect2->x1 == 0
        && rect2->y1 == 0
        && rect2->x2 == 0
        && rect2->y2 == 0)
        return true;

    return false;
}

bool isRectEqual(ExynosRect *rect1, ExynosRect *rect2)
{
    if (   rect1->x == rect2->x
        && rect1->y == rect2->y
        && rect1->w == rect2->w
        && rect1->h == rect2->h
        && rect1->fullW == rect2->fullW
        && rect1->fullH == rect2->fullH
        && rect1->colorFormat == rect2->colorFormat)
        return true;

    return false;
}

bool isRectEqual(ExynosRect2 *rect1, ExynosRect2 *rect2)
{
    if (   rect1->x1 == rect2->x1
        && rect1->y1 == rect2->y1
        && rect1->x2 == rect2->x2
        && rect1->y2 == rect2->y2)
        return true;

    return false;
}

ExynosRect2 convertingAndroidArea2HWArea(ExynosRect2 *srcRect, const ExynosRect *regionRect)
{
    int x = regionRect->x;
    int y = regionRect->y;
    int w = regionRect->w;
    int h = regionRect->h;

    ExynosRect2 newRect2;

    newRect2.x1 = (srcRect->x1 + 1000) * w / 2000;
    newRect2.y1 = (srcRect->y1 + 1000) * h / 2000;
    newRect2.x2 = (srcRect->x2 + 1000) * w / 2000;
    newRect2.y2 = (srcRect->y2 + 1000) * h / 2000;

    if (newRect2.x1 < 0)
        newRect2.x1 = 0;
    else if (w <= newRect2.x1)
        newRect2.x1 = w - 1;

    if (newRect2.y1 < 0)
        newRect2.y1 = 0;
    else if (h <= newRect2.y1)
        newRect2.y1 = h - 1;

    if (newRect2.x2 < 0)
        newRect2.x2 = 0;
    else if (w <= newRect2.x2)
        newRect2.x2 = w - 1;

    if (newRect2.y2 < 0)
        newRect2.y2 = 0;
    else if (h <= newRect2.y2)
        newRect2.y2 = h - 1;

    if (newRect2.x2 < newRect2.x1)
        newRect2.x2 = newRect2.x1;

    if (newRect2.y2 < newRect2.y1)
        newRect2.y2 = newRect2.y1;

    newRect2.x1 += x;
    newRect2.y1 += y;
    newRect2.x2 += x;
    newRect2.y2 += y;

    return newRect2;
}

status_t getResolutionList(String8 &string8Buf, int *w, int *h, int mode)
{
    bool found = false;
    bool flagFirst = true;
    char strBuf[32];
    int sizeOfResSize = 0;
    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
    int max_w = 0, max_h = 0;

    /* this is up to /packages/apps/Camera/res/values/arrays.xml */
    int (*RESOLUTION_LIST)[SIZE_OF_RESOLUTION] = {NULL,};

    switch (mode) {
    case MODE_PREVIEW:
        RESOLUTION_LIST = PREVIEW_LIST;
        sizeOfResSize = sizeof(PREVIEW_LIST) / (sizeof(int) * SIZE_OF_RESOLUTION);
        break;
    case MODE_PICTURE:
        RESOLUTION_LIST = PICTURE_LIST;
        sizeOfResSize = sizeof(PICTURE_LIST) / (sizeof(int) * SIZE_OF_RESOLUTION);
        break;
    case MODE_VIDEO:
        RESOLUTION_LIST = VIDEO_LIST;
        sizeOfResSize = sizeof(VIDEO_LIST) / (sizeof(int) * SIZE_OF_RESOLUTION);
        break;
    case MODE_THUMBNAIL:
        RESOLUTION_LIST = THUMBNAIL_LIST;
        sizeOfResSize = sizeof(THUMBNAIL_LIST) / (sizeof(int) * SIZE_OF_RESOLUTION);
        break;
    default:
        ALOGE("ERR(%s):invalid mode(%d)", __func__, mode);
        return BAD_VALUE;
    }

    for (int i = 0; i < sizeOfResSize; i++) {
        if (   RESOLUTION_LIST[i][0] <= *w
            && RESOLUTION_LIST[i][1] <= *h) {
            if (flagFirst == true) {
                snprintf(strBuf, sizeof(strBuf), "%dx%d", RESOLUTION_LIST[i][0], RESOLUTION_LIST[i][1]);
                string8Buf.append(strBuf);
                max_w = RESOLUTION_LIST[i][0];
                max_h = RESOLUTION_LIST[i][1];

                flagFirst = false;
            } else {
                if ((mode == MODE_PICTURE || mode == MODE_THUMBNAIL) &&
                    ((max_w) / 16 > RESOLUTION_LIST[i][0] ||
                     (max_h / 16) > RESOLUTION_LIST[i][1]))
                    continue;

                snprintf(strBuf, sizeof(strBuf), ",%dx%d", RESOLUTION_LIST[i][0], RESOLUTION_LIST[i][1]);
                string8Buf.append(strBuf);
            }

            found = true;
        }
    }

    if (found == false) {
        ALOGE("ERR(%s):cannot find resolutions", __func__);
    } else {
        *w = max_w;
        *h = max_h;
    }

    return NO_ERROR;
}

status_t getZoomRatioList(String8 & string8Buf, int maxZoom, int start, int end)
{
    bool flagFirst = true;
    char strBuf[32];

    int cur = start;
    int step = (end - start) / (maxZoom - 1);

    for (int i = 0; i < (maxZoom - 1); i++) {
        snprintf(strBuf, sizeof(strBuf), "%d", cur);
        string8Buf.append(strBuf);
        string8Buf.append(",");
        cur += step;
    }

    snprintf(strBuf, sizeof(strBuf), "%d", end);
    string8Buf.append(strBuf);

    /* ex : "100,130,160,190,220,250,280,310,340,360,400" */

    return NO_ERROR;
}

status_t getSupportedFpsList(String8 & string8Buf, int min, int max)
{
    bool found = false;
    bool flagFirst = true;
    char strBuf[32];
    int numOfList = 0;

    numOfList = sizeof(FPS_RANGE_LIST) / (sizeof(int) * 2);

    for (int i = 0; i < numOfList; i++) {
        if (min <= FPS_RANGE_LIST[i][0] &&
            FPS_RANGE_LIST[i][1] <= max) {
            if (flagFirst == true) {
                flagFirst = false;
                snprintf(strBuf, sizeof(strBuf), "(%d,%d)", FPS_RANGE_LIST[i][0], FPS_RANGE_LIST[i][1]);
            } else {
                snprintf(strBuf, sizeof(strBuf), ",(%d,%d)", FPS_RANGE_LIST[i][0], FPS_RANGE_LIST[i][1]);
            }
            string8Buf.append(strBuf);

            found = true;
        }
    }

    if (found == false)
        ALOGE("ERR(%s):cannot find fps list", __func__);

    return NO_ERROR;
}


int32_t getMetaDmRequestFrameCount(struct camera2_shot_ext *shot_ext)
{
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return -1;
    }
    return shot_ext->shot.dm.request.frameCount;
}

void setMetaCtlAeTargetFpsRange(struct camera2_shot_ext *shot_ext, uint32_t min, uint32_t max)
{
    ALOGI("INFO(%s):aeTargetFpsRange(min=%d, min=%d)", __FUNCTION__, min, max);
    shot_ext->shot.ctl.aa.aeTargetFpsRange[0] = min;
    shot_ext->shot.ctl.aa.aeTargetFpsRange[1] = max;
}

void getMetaCtlAeTargetFpsRange(struct camera2_shot_ext *shot_ext, uint32_t *min, uint32_t *max)
{
    *min = shot_ext->shot.ctl.aa.aeTargetFpsRange[0];
    *max = shot_ext->shot.ctl.aa.aeTargetFpsRange[1];
}

void setMetaCtlSensorFrameDuration(struct camera2_shot_ext *shot_ext, uint64_t duration)
{
    shot_ext->shot.ctl.sensor.frameDuration = duration;
}

void getMetaCtlSensorFrameDuration(struct camera2_shot_ext *shot_ext, uint64_t *duration)
{
    *duration = shot_ext->shot.ctl.sensor.frameDuration;
}

void setMetaCtlAeMode(struct camera2_shot_ext *shot_ext, enum aa_aemode aeMode)
{
    shot_ext->shot.ctl.aa.aeMode = aeMode;
}

void getMetaCtlAeMode(struct camera2_shot_ext *shot_ext, enum aa_aemode *aeMode)
{
    *aeMode = shot_ext->shot.ctl.aa.aeMode;
}

void setMetaCtlExposureCompensation(struct camera2_shot_ext *shot_ext, int32_t expCompensation)
{
    shot_ext->shot.ctl.aa.aeExpCompensation = expCompensation;
}

void getMetaCtlExposureCompensation(struct camera2_shot_ext *shot_ext, int32_t *expCompensation)
{
    *expCompensation = shot_ext->shot.ctl.aa.aeExpCompensation;
}

status_t setMetaCtlCropRegion(
        struct camera2_shot_ext *shot_ext,
        int zoom,
        int srcW, int srcH,
        int dstW, int dstH)
{
    int newX = 0;
    int newY = 0;
    int newW = 0;
    int newH = 0;

    if (getCropRectAlign(srcW,  srcH,
                         dstW,  dstH,
                         &newX, &newY,
                         &newW, &newH,
                         CAMERA_MAGIC_ALIGN, 2,
                         zoom) != NO_ERROR) {
        ALOGE("ERR(%s):getCropRectAlign(%d, %d, %d, %d) fail",
            __func__, srcW,  srcH, dstW,  dstH);
        return BAD_VALUE;
    }

    newX = ALIGN(newX, 2);
    newY = ALIGN(newY, 2);

    ALOGI("DEBUG(%s):size(%d, %d, %d, %d), level(%d)",
        __FUNCTION__, newX, newY, newW, newH, zoom);

    shot_ext->shot.ctl.scaler.cropRegion[0] = newX;
    shot_ext->shot.ctl.scaler.cropRegion[1] = newY;
    shot_ext->shot.ctl.scaler.cropRegion[2] = newW;
    shot_ext->shot.ctl.scaler.cropRegion[3] = newH;

    return NO_ERROR;
}

void getMetaCtlCropRegion(
        struct camera2_shot_ext *shot_ext,
        int *x, int *y,
        int *w, int *h)
{
    *x = shot_ext->shot.ctl.scaler.cropRegion[0];
    *y = shot_ext->shot.ctl.scaler.cropRegion[1];
    *w = shot_ext->shot.ctl.scaler.cropRegion[2];
    *h = shot_ext->shot.ctl.scaler.cropRegion[3];
}

void setMetaCtlAeRegion(
        struct camera2_shot_ext *shot_ext,
        int x, int y,
        int w, int h,
        int weight)
{
    shot_ext->shot.ctl.aa.aeRegions[0] = x;
    shot_ext->shot.ctl.aa.aeRegions[1] = y;
    shot_ext->shot.ctl.aa.aeRegions[2] = w;
    shot_ext->shot.ctl.aa.aeRegions[3] = h;
    shot_ext->shot.ctl.aa.aeRegions[4] = weight;
}

void getMetaCtlAeRegion(
        struct camera2_shot_ext *shot_ext,
        int *x, int *y,
        int *w, int *h,
        int *weight)
{
    *x = shot_ext->shot.ctl.aa.aeRegions[0];
    *y = shot_ext->shot.ctl.aa.aeRegions[1];
    *w = shot_ext->shot.ctl.aa.aeRegions[2];
    *h = shot_ext->shot.ctl.aa.aeRegions[3];
    *weight = shot_ext->shot.ctl.aa.aeRegions[4];
}


void setMetaCtlAntibandingMode(struct camera2_shot_ext *shot_ext, enum aa_ae_antibanding_mode antibandingMode)
{
    shot_ext->shot.ctl.aa.aeAntibandingMode = antibandingMode;
}

void getMetaCtlAntibandingMode(struct camera2_shot_ext *shot_ext, enum aa_ae_antibanding_mode *antibandingMode)
{
    *antibandingMode = shot_ext->shot.ctl.aa.aeAntibandingMode;
}

void setMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_mode mode, enum aa_scene_mode sceneMode)
{
    shot_ext->shot.ctl.aa.mode = mode;
    shot_ext->shot.ctl.aa.sceneMode = sceneMode;

    switch (sceneMode) {
    case AA_SCENE_MODE_FACE_PRIORITY:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF) 
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.sceneMode = AA_SCENE_MODE_UNSUPPORTED;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_ACTION:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_PORTRAIT:
    case AA_SCENE_MODE_LANDSCAPE:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_NIGHT:
        /* AE_LOCK is prohibited */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF ||
            shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_LOCKED)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_NIGHT_PORTRAIT:
    case AA_SCENE_MODE_THEATRE:
    case AA_SCENE_MODE_BEACH:
    case AA_SCENE_MODE_SNOW:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_SUNSET:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_STEADYPHOTO:
    case AA_SCENE_MODE_FIREWORKS:
    case AA_SCENE_MODE_SPORTS:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_PARTY:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_MANUAL;
        shot_ext->shot.ctl.aa.isoValue = 200;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 4; /* "4" is default + 1. */
        break;
    case AA_SCENE_MODE_CANDLELIGHT:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.isoValue = 0;
        shot_ext->shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.noise.strength = 0;
        shot_ext->shot.ctl.edge.mode = PROCESSING_MODE_OFF;
        shot_ext->shot.ctl.edge.strength = 0;
        shot_ext->shot.ctl.color.saturation = 3; /* "3" is default. */
        break;
    default:
        break;
    }
}

void getMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_mode *mode, enum aa_scene_mode *sceneMode)
{
    *mode = shot_ext->shot.ctl.aa.mode;
    *sceneMode = shot_ext->shot.ctl.aa.sceneMode;
}

void setMetaCtlAwbMode(struct camera2_shot_ext *shot_ext, enum aa_awbmode awbMode)
{
    shot_ext->shot.ctl.aa.awbMode = awbMode;
}

void getMetaCtlAwbMode(struct camera2_shot_ext *shot_ext, enum aa_awbmode *awbMode)
{
    *awbMode = shot_ext->shot.ctl.aa.awbMode;
}

void setMetaCtlAfRegion(struct camera2_shot_ext *shot_ext,
                        int x, int y, int w, int h, int weight)
{
    shot_ext->shot.ctl.aa.afRegions[0] = x;
    shot_ext->shot.ctl.aa.afRegions[1] = y;
    shot_ext->shot.ctl.aa.afRegions[2] = w;
    shot_ext->shot.ctl.aa.afRegions[3] = h;
    shot_ext->shot.ctl.aa.afRegions[4] = weight;
}

void getMetaCtlAfRegion(struct camera2_shot_ext *shot_ext,
                        int *x, int *y, int *w, int *h, int *weight)
{
    *x = shot_ext->shot.ctl.aa.afRegions[0];
    *y = shot_ext->shot.ctl.aa.afRegions[1];
    *w = shot_ext->shot.ctl.aa.afRegions[2];
    *h = shot_ext->shot.ctl.aa.afRegions[3];
    *weight = shot_ext->shot.ctl.aa.afRegions[4];
}

void setMetaCtlColorCorrectionMode(struct camera2_shot_ext *shot_ext, enum colorcorrection_mode mode)
{
    shot_ext->shot.ctl.color.mode = mode;
}

void getMetaCtlColorCorrectionMode(struct camera2_shot_ext *shot_ext, enum colorcorrection_mode *mode)
{
    *mode = shot_ext->shot.ctl.color.mode;
}

void setMetaCtlBrightness(struct camera2_shot_ext *shot_ext, int32_t brightness)
{
    shot_ext->shot.ctl.color.brightness = brightness;
}

void getMetaCtlBrightness(struct camera2_shot_ext *shot_ext, int32_t *brightness)
{
    *brightness = shot_ext->shot.ctl.color.brightness;
}

void setMetaCtlSaturation(struct camera2_shot_ext *shot_ext, int32_t saturation)
{
    shot_ext->shot.ctl.color.saturation = saturation;
}

void getMetaCtlSaturation(struct camera2_shot_ext *shot_ext, int32_t *saturation)
{
    *saturation = shot_ext->shot.ctl.color.saturation;
}

void setMetaCtlHue(struct camera2_shot_ext *shot_ext, int32_t hue)
{
    shot_ext->shot.ctl.color.hue = hue;
}

void getMetaCtlHue(struct camera2_shot_ext *shot_ext, int32_t *hue)
{
    *hue = shot_ext->shot.ctl.color.hue;
}

void setMetaCtlContrast(struct camera2_shot_ext *shot_ext, uint32_t contrast)
{
    shot_ext->shot.ctl.color.contrast = contrast;
}

void getMetaCtlContrast(struct camera2_shot_ext *shot_ext, uint32_t *contrast)
{
    *contrast = shot_ext->shot.ctl.color.contrast;
}

void setMetaCtlSharpness(struct camera2_shot_ext *shot_ext, enum processing_mode mode, int32_t sharpness)
{
    shot_ext->shot.ctl.edge.mode = mode;
    shot_ext->shot.ctl.edge.strength = sharpness;
}

void getMetaCtlSharpness(struct camera2_shot_ext *shot_ext, enum processing_mode *mode, int32_t *sharpness)
{
    *mode = shot_ext->shot.ctl.edge.mode;
    *sharpness = shot_ext->shot.ctl.edge.strength;
}

void setMetaCtlIso(struct camera2_shot_ext *shot_ext, enum aa_isomode mode, uint32_t iso)
{
    shot_ext->shot.ctl.aa.isoMode = mode;
    shot_ext->shot.ctl.aa.isoValue = iso;
}

void getMetaCtlIso(struct camera2_shot_ext *shot_ext, enum aa_isomode *mode, uint32_t *iso)
{
    *mode = shot_ext->shot.ctl.aa.isoMode;
    *iso = shot_ext->shot.ctl.aa.isoValue;
}

void setMetaCtlFdMode(struct camera2_shot_ext *shot_ext, enum facedetect_mode mode)
{
    shot_ext->shot.ctl.stats.faceDetectMode = mode;
}

void getStreamFrameValid(struct camera2_stream *shot_stream, uint32_t *fvalid)
{
    *fvalid = shot_stream->fvalid;
}

void getStreamFrameCount(struct camera2_stream *shot_stream, uint32_t *fcount)
{
    *fcount = shot_stream->fcount;
}

nsecs_t getMetaDmSensorTimeStamp(struct camera2_shot_ext *shot_ext)
{
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):buffer is NULL", __FUNCTION__, __LINE__);
        return 0;
    }
    return shot_ext->shot.dm.sensor.timeStamp;
}

void setMetaNodeLeaderRequest(struct camera2_shot_ext* shot_ext, int value)
{
    shot_ext->node_group.leader.request = value;

    ALOGV("INFO(%s[%d]):(%d)", __FUNCTION__, __LINE__,
        shot_ext->node_group.leader.request);
}

void setMetaNodeLeaderVideoID(struct camera2_shot_ext* shot_ext, int value)
{
    shot_ext->node_group.leader.vid = value;

    ALOGV("INFO(%s[%d]):(%d)", __FUNCTION__, __LINE__,
    shot_ext->node_group.leader.vid);
}

void setMetaNodeLeaderInputSize(struct camera2_shot_ext* shot_ext, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.leader.input.cropRegion[0] = x;
    shot_ext->node_group.leader.input.cropRegion[1] = y;
    shot_ext->node_group.leader.input.cropRegion[2] = w;
    shot_ext->node_group.leader.input.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        shot_ext->node_group.leader.input.cropRegion[0],
        shot_ext->node_group.leader.input.cropRegion[1],
        shot_ext->node_group.leader.input.cropRegion[2],
        shot_ext->node_group.leader.input.cropRegion[3]);
}

void setMetaNodeLeaderOutputSize(struct camera2_shot_ext * shot_ext, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.leader.output.cropRegion[0] = x;
    shot_ext->node_group.leader.output.cropRegion[1] = y;
    shot_ext->node_group.leader.output.cropRegion[2] = w;
    shot_ext->node_group.leader.output.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        shot_ext->node_group.leader.output.cropRegion[0],
        shot_ext->node_group.leader.output.cropRegion[1],
        shot_ext->node_group.leader.output.cropRegion[2],
        shot_ext->node_group.leader.output.cropRegion[3]);
}

void setMetaNodeCaptureRequest(struct camera2_shot_ext* shot_ext, int index, int value)
{
    shot_ext->node_group.capture[index].request = value;

    ALOGV("INFO(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].request);
}

void setMetaNodeCaptureVideoID(struct camera2_shot_ext* shot_ext, int index, int value)
{
    shot_ext->node_group.capture[index].vid = value;

    ALOGV("INFO(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].vid);
}

void setMetaNodeCaptureInputSize(struct camera2_shot_ext* shot_ext, int index, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.capture[index].input.cropRegion[0] = x;
    shot_ext->node_group.capture[index].input.cropRegion[1] = y;
    shot_ext->node_group.capture[index].input.cropRegion[2] = w;
    shot_ext->node_group.capture[index].input.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d)(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].input.cropRegion[0],
        shot_ext->node_group.capture[index].input.cropRegion[1],
        shot_ext->node_group.capture[index].input.cropRegion[2],
        shot_ext->node_group.capture[index].input.cropRegion[3]);
}

void setMetaNodeCaptureOutputSize(struct camera2_shot_ext * shot_ext, int index, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.capture[index].output.cropRegion[0] = x;
    shot_ext->node_group.capture[index].output.cropRegion[1] = y;
    shot_ext->node_group.capture[index].output.cropRegion[2] = w;
    shot_ext->node_group.capture[index].output.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d)(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].output.cropRegion[0],
        shot_ext->node_group.capture[index].output.cropRegion[1],
        shot_ext->node_group.capture[index].output.cropRegion[2],
        shot_ext->node_group.capture[index].output.cropRegion[3]);
}

void setMetaBypassDrc(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->drc_bypass = value;
}

void setMetaBypassDis(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->dis_bypass = value;
}

void setMetaBypassDnr(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->dnr_bypass = value;
}

void setMetaBypassFd(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->fd_bypass = value;
}

void setMetaSetfile(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->setfile = value;
}

int mergeSetfileYuvRange(int setfile, int yuvRange)
{
    int ret = setfile;

    ret &= (0x0000ffff);
    ret |= (yuvRange << 16);

    return ret;
}

int getPlaneSizeFlite(int width, int height)
{
    int PlaneSize;
    int Alligned_Width;
    int Bytes;

    Alligned_Width = (width + 9) / 10 * 10;
    Bytes = Alligned_Width * 8 / 5 ;

    PlaneSize = Bytes * height;

    return PlaneSize;
}

bool dumpToFile(char *filename, char *srcBuf, unsigned int size)
{
    FILE *yuvFd = NULL;
    char *buffer = NULL;

    yuvFd = fopen(filename, "w+");

    if (yuvFd == NULL) {
        ALOGE("ERR(%s):open(%s) fail",
            __func__, filename);
        return false;
    }

    buffer = (char *)malloc(size);

    if (buffer == NULL) {
        ALOGE("ERR(%s):malloc file", __func__);
        fclose(yuvFd);
        return false;
    }

    memcpy(buffer, srcBuf, size);

    fflush(stdout);

    fwrite(buffer, 1, size, yuvFd);

    fflush(yuvFd);

    if (yuvFd)
        fclose(yuvFd);
    if (buffer)
        free(buffer);

    ALOGD("DEBUG(%s):filedump(%s, size(%d) is successed!!",
            __func__, filename, size);

    return true;
}

status_t getYuvPlaneSize(int format, unsigned int *size,
                      unsigned int width, unsigned int height)
{
    unsigned int frame_ratio = 1;
    unsigned int frame_size = width * height;
    unsigned int src_bpp = 0;
    unsigned int src_planes = 0;

    if (getYuvFormatInfo(format, &src_bpp, &src_planes) < 0){
        ALOGE("ERR(%s[%d]): invalid format(%x)", __FUNCTION__, __LINE__, format);
        return BAD_VALUE;
    }

    src_planes = (src_planes == 0) ? 1 : src_planes;
    frame_ratio = 8 * (src_planes -1) / (src_bpp - 8);

    switch (src_planes) {
    case 1:
        switch (format) {
        case V4L2_PIX_FMT_BGR32:
        case V4L2_PIX_FMT_RGB32:
            size[0] = frame_size << 2;
            break;
        case V4L2_PIX_FMT_RGB565X:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_VYUY:
        case V4L2_PIX_FMT_YVYU:
            size[0] = frame_size << 1;
            break;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV21M:
            size[0] = (frame_size * 3) >> 1;
            break;
        case V4L2_PIX_FMT_YVU420:
            size[0] = frame_size + (ALIGN((width >> 1), 16) * ((height >> 1) * 2));
            break;
        default:
            ALOGE("%s::invalid color type", __func__);
            return false;
            break;
        }
        size[1] = 0;
        size[2] = 0;
        break;
    case 2:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = 0;
        break;
    case 3:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = frame_size / frame_ratio;
        break;
    default:
        ALOGE("%s::invalid color foarmt", __func__);
        return false;
        break;
    }

    return NO_ERROR;
}

status_t getYuvFormatInfo(unsigned int v4l2_pixel_format,
                          unsigned int *bpp, unsigned int *planes)
{
    switch (v4l2_pixel_format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
        *bpp    = 12;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV21M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV12MT_16X16:
        *bpp    = 12;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
        *bpp    = 12;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420M:
        *bpp    = 12;
        *planes = 3;
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_VYUY:
        *bpp    = 16;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
        *bpp    = 16;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUV422P:
        *bpp    = 16;
        *planes = 3;
        break;
    default:
        return BAD_VALUE;
        break;
    }

    return NO_ERROR;
}

int getYuvPlaneCount(unsigned int v4l2_pixel_format)
{
    int ret = 0;
    unsigned int bpp = 0;
    unsigned int planeCnt = 0;

    ret = getYuvFormatInfo(v4l2_pixel_format, &bpp, &planeCnt);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): BAD_VALUE", __FUNCTION__, __LINE__);
        return -1;
    }

    return planeCnt;
}

int displayExynosBuffer( ExynosCameraBuffer *buffer) {
        ALOGE("-----------------------------------------------");
        ALOGE(" buffer.index = %d ", buffer->index);
        ALOGE(" buffer.planeCount = %d ", buffer->planeCount);
        for(int i = 0 ; i < buffer->planeCount ; i++ ) {
            ALOGE(" buffer.fd[%d] = %d ", i, buffer->fd[i]);
            ALOGE(" buffer.size[%d] = %d ", i, buffer->size[i]);
            ALOGE(" buffer.addr[%d] = %p ", i, buffer->addr[i]);
        }
        ALOGE("-----------------------------------------------");
        return 0;
}

#ifdef SENSOR_NAME_GET_FROM_FILE
int getSensorIdFromFile(int camId)
{
    FILE *fp = NULL;
    int numread = -1;
    char sensor_name[50];
    int sensorName = -1;
    bool ret = true;

    if (camId == CAMERA_ID_BACK) {
        fp = fopen(SENSOR_NAME_PATH_BACK, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
    } else {
        fp = fopen(SENSOR_NAME_PATH_FRONT, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
    }

    if (fgets(sensor_name, sizeof(sensor_name), fp) == NULL) {
        ALOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
	    goto err;
    }

    numread = strlen(sensor_name);
    ALOGD("DEBUG(%s[%d]):Sensor name is %s(%d)", __FUNCTION__, __LINE__, sensor_name, numread);

    /* TODO: strncmp for check sensor name, str is vendor specific sensor name
     * ex)
     *    if (strncmp((const char*)sensor_name, "str", numread - 1) == 0) {
     *        sensorName = SENSOR_NAME_IMX135;
     *    }
     */
    sensorName = atoi(sensor_name);

err:
    if (fp != NULL)
        fclose(fp);

    return sensorName;
}
#endif

void clearBit(unsigned int *target, int index, bool isStatePrint)
{
    *target = *target &~ (1 << index);

    if (isStatePrint)
        ALOGD("INFO(%s[%d]):(0x%x)", __FUNCTION__, __LINE__, *target);
}

void setBit(unsigned int *target, int index, bool isStatePrint)
{
    *target = *target | (1 << index);

    if (isStatePrint)
        ALOGD("INFO(%s[%d]):(0x%x)", __FUNCTION__, __LINE__, *target);
}

void resetBit(unsigned int *target, int value, bool isStatePrint)
{
    *target = value;

    if (isStatePrint)
        ALOGD("INFO(%s[%d]):(0x%x)", __FUNCTION__, __LINE__, *target);
}

void checkAndroidVersion(void) {
    char value[PROPERTY_VALUE_MAX] = {0};
    char targetAndroidVersion[PROPERTY_VALUE_MAX] = {0};

    snprintf(targetAndroidVersion, PROPERTY_VALUE_MAX, "%d.%d", TARGET_ANDROID_VER_MAJ, TARGET_ANDROID_VER_MIN);

    property_get("ro.build.version.release", value, "0");

    if (strncmp(targetAndroidVersion, value, PROPERTY_VALUE_MAX))
        ALOGW("WRN(%s[%d]): Tartget Android version (%s), build version (%s)",
                __FUNCTION__, __LINE__, targetAndroidVersion, value);
    else
        ALOGI("Andorid build version release %s", value);
}

bool isReprocessing(int cameraId)
{
    bool reprocessing = false;

    if (cameraId == CAMERA_ID_BACK)
        reprocessing = MAIN_CAMERA_REPROCESSING;
    else
        reprocessing = FRONT_CAMERA_REPROCESSING;

    return reprocessing;
}

bool isSccCapture(int cameraId)
{
    bool sccCapture = false;

    if (cameraId == CAMERA_ID_BACK)
        sccCapture = MAIN_CAMERA_SCC_CAPTURE;
    else
        sccCapture = FRONT_CAMERA_SCC_CAPTURE;

    return sccCapture;
}

}; /* namespace android */
