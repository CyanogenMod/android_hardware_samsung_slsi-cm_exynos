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

#ifndef EXYNOS_CAMERA_UTILS_H
#define EXYNOS_CAMERA_UTILS_H

#include <cutils/properties.h>
#include <utils/threads.h>
#include <utils/String8.h>

#include "exynos_format.h"
#include "ExynosRect.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraSensorInfo.h"
#include "videodev2_exynos_media.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraConfig.h"

#define ROUND_OFF(x, dig)           (floor((x) * pow(10.0f, dig)) / pow(10.0f, dig))

namespace android {

bool            getCropRect(
                    int  src_w,  int   src_h,
                    int  dst_w,  int   dst_h,
                    int *crop_x, int *crop_y,
                    int *crop_w, int *crop_h,
                    int  zoom);

bool            getCropRect2(
                    int  src_w,     int   src_h,
                    int  dst_w,     int   dst_h,
                    int *new_src_x, int *new_src_y,
                    int *new_src_w, int *new_src_h,
                    int  zoom);

status_t        getCropRectAlign(
                    int  src_w,  int   src_h,
                    int  dst_w,  int   dst_h,
                    int *crop_x, int *crop_y,
                    int *crop_w, int *crop_h,
                    int align_w, int align_h,
                    int  zoom);

uint32_t        bracketsStr2Ints(
                    char *str,
                    uint32_t num,
                    ExynosRect2 *rect2s,
                    int *weights,
                    int mode);
bool            subBracketsStr2Ints(int num, char *str, int *arr);

void            convertingRectToRect2(ExynosRect *rect, ExynosRect2 *rect2);
void            convertingRect2ToRect(ExynosRect2 *rect2, ExynosRect *rect);

bool            isRectNull(ExynosRect *rect);
bool            isRectNull(ExynosRect2 *rect2);
bool            isRectEqual(ExynosRect *rect1, ExynosRect *rect2);
bool            isRectEqual(ExynosRect2 *rect1, ExynosRect2 *rect2);

ExynosRect2     convertingAndroidArea2HWArea(ExynosRect2 *srcRect, const ExynosRect *regionRect);

status_t        getResolutionList(String8 &string8Buf, int *w, int *h, int mode);
status_t        getZoomRatioList(String8 & string8Buf, int maxZoom, int start, int end);
status_t        getSupportedFpsList(String8 & string8Buf, int min, int max);


/*
 * Control struct camera2_shot_ext
 */
int32_t getMetaDmRequestFrameCount(struct camera2_shot_ext *shot_ext);

void setMetaCtlAeTargetFpsRange(struct camera2_shot_ext *shot_ext, uint32_t min, uint32_t max);
void getMetaCtlAeTargetFpsRange(struct camera2_shot_ext *shot_ext, uint32_t *min, uint32_t *max);

void setMetaCtlSensorFrameDuration(struct camera2_shot_ext *shot_ext, uint64_t duration);
void getMetaCtlSensorFrameDuration(struct camera2_shot_ext *shot_ext, uint64_t *duration);

void setMetaCtlAeMode(struct camera2_shot_ext *shot_ext, enum aa_aemode aeMode);
void getMetaCtlAeMode(struct camera2_shot_ext *shot_ext, enum aa_aemode *aeMode);

void setMetaCtlExposureCompensation(struct camera2_shot_ext *shot_ext, int32_t expCompensation);
void getMetaCtlExposureCompensation(struct camera2_shot_ext *shot_ext, int32_t *expCompensatione);

status_t setMetaCtlCropRegion(
            struct camera2_shot_ext *shot_ext,
            int zoom,
            int srcW, int srcH,
            int dstW, int dstH);
void getMetaCtlCropRegion(
            struct camera2_shot_ext *shot_ext,
            int *x, int *y,
            int *w, int *h);

void setMetaCtlAeRegion(
            struct camera2_shot_ext *shot_ext,
            int x, int y,
            int w, int h,
            int weight);
void getMetaCtlAeRegion(
            struct camera2_shot_ext *shot_ext,
            int *x, int *y,
            int *w, int *h,
            int *weight);

void setMetaCtlAntibandingMode(struct camera2_shot_ext *shot_ext, enum aa_ae_antibanding_mode antibandingMode);
void getMetaCtlAntibandingMode(struct camera2_shot_ext *shot_ext, enum aa_ae_antibanding_mode *antibandingMode);

void setMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_mode mode, enum aa_scene_mode sceneMode);
void getMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_mode *mode, enum aa_scene_mode *sceneMode);

void setMetaCtlAwbMode(struct camera2_shot_ext *shot_ext, enum aa_awbmode awbMode);
void getMetaCtlAwbMode(struct camera2_shot_ext *shot_ext, enum aa_awbmode *awbMode);

void setMetaCtlAfRegion(struct camera2_shot_ext *shot_ext,
                            int x, int y, int w, int h, int weight);
void getMetaCtlAfRegion(struct camera2_shot_ext *shot_ext,
                        int *x, int *y, int *w, int *h, int *weight);

void setMetaCtlColorCorrectionMode(struct camera2_shot_ext *shot_ext, enum colorcorrection_mode mode);
void getMetaCtlColorCorrectionMode(struct camera2_shot_ext *shot_ext, enum colorcorrection_mode *mode);

void setMetaCtlBrightness(struct camera2_shot_ext *shot_ext, int32_t brightness);
void getMetaCtlBrightness(struct camera2_shot_ext *shot_ext, int32_t *brightness);

void setMetaCtlSaturation(struct camera2_shot_ext *shot_ext, int32_t saturation);
void getMetaCtlSaturation(struct camera2_shot_ext *shot_ext, int32_t *saturation);

void setMetaCtlHue(struct camera2_shot_ext *shot_ext, int32_t hue);
void getMetaCtlHue(struct camera2_shot_ext *shot_ext, int32_t *hue);

void setMetaCtlContrast(struct camera2_shot_ext *shot_ext, uint32_t contrast);
void getMetaCtlContrast(struct camera2_shot_ext *shot_ext, uint32_t *contrast);

void setMetaCtlSharpness(struct camera2_shot_ext *shot_ext, enum processing_mode mode, int32_t sharpness);
void getMetaCtlSharpness(struct camera2_shot_ext *shot_ext, enum processing_mode *mode, int32_t *sharpness);

void setMetaCtlIso(struct camera2_shot_ext *shot_ext, enum aa_isomode mode, uint32_t iso);
void getMetaCtlIso(struct camera2_shot_ext *shot_ext, enum aa_isomode *mode, uint32_t *iso);
void setMetaCtlFdMode(struct camera2_shot_ext *shot_ext, enum facedetect_mode mode);

void getStreamFrameValid(struct camera2_stream *shot_stream, uint32_t *fvalid);
void getStreamFrameCount(struct camera2_stream *shot_stream, uint32_t *fcount);

nsecs_t getMetaDmSensorTimeStamp(struct camera2_shot_ext *shot_ext);

void setMetaNodeLeaderRequest(struct camera2_shot_ext* shot_ext, int value);
void setMetaNodeLeaderVideoID(struct camera2_shot_ext* shot_ext, int value);
void setMetaNodeLeaderInputSize(struct camera2_shot_ext * shot_ext, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
void setMetaNodeLeaderOutputSize(struct camera2_shot_ext * shot_ext, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
void setMetaNodeCaptureRequest(struct camera2_shot_ext* shot_ext, int index, int value);
void setMetaNodeCaptureVideoID(struct camera2_shot_ext* shot_ext, int index, int value);
void setMetaNodeCaptureInputSize(struct camera2_shot_ext * shot_ext, int index, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
void setMetaNodeCaptureOutputSize(struct camera2_shot_ext * shot_ext, int index, unsigned int x, unsigned int y, unsigned int w, unsigned int h);

void setMetaBypassDrc(struct camera2_shot_ext *shot_ext, int value);
void setMetaBypassDis(struct camera2_shot_ext *shot_ext, int value);
void setMetaBypassDnr(struct camera2_shot_ext *shot_ext, int value);
void setMetaBypassFd(struct camera2_shot_ext *shot_ext, int value);

void setMetaSetfile(struct camera2_shot_ext *shot_ext, int value);

int mergeSetfileYuvRange(int setfile, int yuvRange);

int getPlaneSizeFlite(int width, int height);

bool dumpToFile(char *filename, char *srcBuf, unsigned int size);

/* TODO: This functions need to be commonized */
status_t getYuvPlaneSize(int format, unsigned int *size,
                         unsigned int width, unsigned int height);
status_t getYuvFormatInfo(unsigned int v4l2_pixel_format,
                         unsigned int *bpp, unsigned int *planes);
int getYuvPlaneCount(unsigned int v4l2_pixel_format);
int displayExynosBuffer( ExynosCameraBuffer *buffer);

void clearBit(unsigned int *target, int index, bool isStatePrint = false);
void setBit(unsigned int *target, int index, bool isStatePrint = false);
void resetBit(unsigned int *target, int value, bool isStatePrint = false);

void checkAndroidVersion(void);

bool isReprocessing(int cameraId);
bool isSccCapture(int cameraId);

}; /* namespace android */

#endif

