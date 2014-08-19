/*
**
** Copyright 2014, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraFrameFactoryVision"
#include <cutils/log.h>

#include "ExynosCameraFrameFactoryVision.h"

namespace android {

ExynosCameraFrameFactoryVision::ExynosCameraFrameFactoryVision(int cameraId, ExynosCameraParameters *param)
{
    m_cameraId = cameraId;
    m_parameters = param;
    m_frameCount = 0;

    for (int i = 0; i < MAX_NUM_PIPES; i++)
        m_pipes[i] = NULL;

    m_request3AP = 0;
    m_request3AC = 0;
    m_requestISP = 1;
    m_requestSCC = 0;
    m_requestDIS = 0;
    m_requestSCP = 1;

    m_bypassDRC = true;
    m_bypassDIS = true;
    m_bypassDNR = true;
    m_bypassFD = true;
}

ExynosCameraFrameFactoryVision::~ExynosCameraFrameFactoryVision()
{
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        ALOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);
}

status_t ExynosCameraFrameFactoryVision::create(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    int sensorId = getSensorId(m_cameraId);
    int32_t nodeNums[MAX_NODE] = {-1};
    int32_t sensorIds[MAX_NODE] = {-1};

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FRONT_CAMERA_FLITE_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_FLITE_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeFlite(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_FLITE_FRONT)]->setPipeId(PIPE_FLITE_FRONT);
    m_pipes[INDEX(PIPE_FLITE_FRONT)]->setPipeName("PIPE_FLITE_FRONT");

    /* flite pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SS0_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | (sensorId << 0) | (0x1 << 28);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_FLITE_FRONT));

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::destroy(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            m_pipes[i]->destroy();
            delete m_pipes[i];
            m_pipes[i] = NULL;

            ALOGD("DEBUG(%s):Pipe(%d) destroyed", __FUNCTION__, INDEX(i));
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    /* Do nothing */
    return NO_ERROR;
}

ExynosCameraFrame *ExynosCameraFrameFactoryVision::createNewFrame(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES];

    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);
    int requestEntityCount = 0;

    ALOGV("INFO(%s[%d])", __FUNCTION__, __LINE__);

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        ALOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

    /* set flite pipe to linkageList */
    newEntity[INDEX(PIPE_FLITE_FRONT)] = new ExynosCameraFrameEntity(PIPE_FLITE_FRONT, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE_FRONT)]);
    requestEntityCount++;

    ret = m_initPipelines(frame);
    if (ret < 0) {
        ALOGE("ERR(%s):m_initPipelines fail, ret(%d)", __FUNCTION__, ret);
    }

    m_fillNodeGroupInfo(frame);

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameFactoryVision::initPipes(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    camera_pipe_info_t pipeInfo[3];
    ExynosRect tempRect;
    int maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int bayerFormat = V4L2_PIX_FMT_SBGGR12;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);

    ALOGI("INFO(%s[%d]): MaxSensorSize(%dx%d), HwSensorSize(%dx%d)", __FUNCTION__, __LINE__, maxSensorW, maxSensorH, hwSensorW, hwSensorH);
    ALOGI("INFO(%s[%d]): MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", __FUNCTION__, __LINE__, maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    ALOGI("INFO(%s[%d]): MaxPixtureSize(%dx%d), HwPixtureSize(%dx%d)", __FUNCTION__, __LINE__, maxPictureW, maxPictureH, hwPictureW, hwPictureH);

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* FLITE pipe */
#if 0
    tempRect.fullW = maxSensorW + 16;
    tempRect.fullH = maxSensorH + 10;
    tempRect.colorFormat = bayerFormat;
#else
    tempRect.fullW = VISION_WIDTH;
    tempRect.fullH = VISION_HEIGHT;
    tempRect.colorFormat = V4L2_PIX_FMT_SGRBG8;
#endif

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = NUM_BAYER_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    m_frameCount = 0;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::preparePipes(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::startPipes(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->sensorStream(true);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE sensorStream on fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Starting Front [FLITE] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::startInitialThreads(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    ret = startThread(PIPE_FLITE_FRONT);
    if (ret < 0)
        return ret;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::stopPipes(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->sensorStream(false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE sensorStream fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Stopping Front [FLITE] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCameraFrameFactoryVision::setRequest3AC(bool enable)
{
    /* Do nothing */

    return;
}

}; /* namespace android */
