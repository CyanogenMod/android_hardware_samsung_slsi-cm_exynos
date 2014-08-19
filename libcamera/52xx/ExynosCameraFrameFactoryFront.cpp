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
#define LOG_TAG "ExynosCameraFrameFactoryFront"
#include <cutils/log.h>

#include "ExynosCameraFrameFactoryFront.h"

namespace android {

ExynosCameraFrameFactoryFront::ExynosCameraFrameFactoryFront(int cameraId, ExynosCameraParameters *param)
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

ExynosCameraFrameFactoryFront::~ExynosCameraFrameFactoryFront()
{
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        ALOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);
}

status_t ExynosCameraFrameFactoryFront::create(void)
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

    nodeNums[OUTPUT_NODE] = FIMC_IS_VIDEO_ISP_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_ISP_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeISP(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_ISP_FRONT)]->setPipeId(PIPE_ISP_FRONT);
    m_pipes[INDEX(PIPE_ISP_FRONT)]->setPipeName("PIPE_ISP_FRONT");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCC_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_SCC_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeSCC(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_SCC_FRONT)]->setPipeId(PIPE_SCC_FRONT);
    m_pipes[INDEX(PIPE_SCC_FRONT)]->setPipeName("PIPE_SCC_FRONT");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCP_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_SCP_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeSCP(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_SCP_FRONT)]->setPipeId(PIPE_SCP_FRONT);
    m_pipes[INDEX(PIPE_SCP_FRONT)]->setPipeName("PIPE_SCP_FRONT");

    nodeNums[OUTPUT_NODE] = PREVIEW_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_GSC_FRONT)]->setPipeId(PIPE_GSC_FRONT);
    m_pipes[INDEX(PIPE_GSC_FRONT)]->setPipeName("PIPE_GSC_FRONT");

    nodeNums[OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC_PICTURE_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_GSC_PICTURE_FRONT)]->setPipeId(PIPE_GSC_PICTURE_FRONT);
    m_pipes[INDEX(PIPE_GSC_PICTURE_FRONT)]->setPipeName("PIPE_GSC_PICTURE_FRONT");

    nodeNums[OUTPUT_NODE] = VIDEO_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC_VIDEO_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_GSC_VIDEO_FRONT)]->setPipeId(PIPE_GSC_VIDEO_FRONT);
    m_pipes[INDEX(PIPE_GSC_VIDEO_FRONT)]->setPipeName("PIPE_GSC_VIDEO_FRONT");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_JPEG_FRONT)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_JPEG_FRONT)]->setPipeId(PIPE_JPEG_FRONT);
    m_pipes[INDEX(PIPE_JPEG_FRONT)]->setPipeName("PIPE_JPEG_FRONT");

    /* flite pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SS0_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_FLITE_FRONT));

    /* ISP pipe initialize */
    sensorIds[OUTPUT_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FRONT_CAMERA_FLITE_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | ((FRONT_CAMERA_3AA_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[CAPTURE_NODE] = -1;
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_ISP_FRONT));

    /* SCC pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SCC_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_SCC_FRONT));

    /* SCP pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SCP_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_SCP_FRONT));

    /* GSC_PICTURE pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_PICTURE_FRONT)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC_PICTURE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_PICTURE_FRONT));

    /* GSC_PREVIEW pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_FRONT)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_FRONT));

    ret = m_pipes[INDEX(PIPE_GSC_VIDEO_FRONT)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC_RECORDING create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_VIDEO_FRONT));

    /* JPEG pipe initialize */
    ret = m_pipes[INDEX(PIPE_JPEG_FRONT)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_JPEG_FRONT));

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryFront::destroy(void)
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

status_t ExynosCameraFrameFactoryFront::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_isp;
    int zoom = m_parameters->getZoomLevel();
    int previewW = 0, previewH = 0;
    ExynosRect sensorSize;
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;

    m_parameters->getPreviewSize(&previewW, &previewH);
    m_parameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);
    m_parameters->getPreviewBdsSize(&bdsSize);
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));

    /* should add this request value in FrameFactory */
    node_group_info_isp.leader.request = 1;
    node_group_info_isp.capture[PERFRAME_FRONT_SCC_POS].request = m_requestSCC;
    node_group_info_isp.capture[PERFRAME_FRONT_SCP_POS].request = m_requestSCP;

    updateNodeGroupInfoFront(
            m_cameraId,
            &node_group_info_isp,
            bayerCropSize,
            previewW, previewH);

    frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoom);

    return NO_ERROR;
}

ExynosCameraFrame *ExynosCameraFrameFactoryFront::createNewFrameVideoOnly(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {};
    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);

    /* set GSC-Video pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_VIDEO_FRONT)] = new ExynosCameraFrameEntity(PIPE_GSC_VIDEO_FRONT, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VIDEO_FRONT)]);

    return frame;
}

ExynosCameraFrame *ExynosCameraFrameFactoryFront::createNewFrame(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES];
    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);
    int requestEntityCount = 0;

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        ALOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

    /* set flite pipe to linkageList */
    newEntity[INDEX(PIPE_FLITE_FRONT)] = new ExynosCameraFrameEntity(PIPE_FLITE_FRONT, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE_FRONT)]);

    /* set ISP pipe to linkageList */
    newEntity[INDEX(PIPE_ISP_FRONT)] = new ExynosCameraFrameEntity(PIPE_ISP_FRONT, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addChildEntity(newEntity[INDEX(PIPE_FLITE_FRONT)], newEntity[INDEX(PIPE_ISP_FRONT)]);
    requestEntityCount++;
    
    /* set SCC pipe to linkageList */
    newEntity[INDEX(PIPE_SCC_FRONT)] = new ExynosCameraFrameEntity(PIPE_SCC_FRONT, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SCC_FRONT)]);
    requestEntityCount++;

    /* set SCP pipe to linkageList */
    newEntity[INDEX(PIPE_SCP_FRONT)] = new ExynosCameraFrameEntity(PIPE_SCP_FRONT, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SCP_FRONT)]);
    requestEntityCount++;

    /* set GSC-Picture pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_PICTURE_FRONT)] = new ExynosCameraFrameEntity(PIPE_GSC_PICTURE_FRONT, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_PICTURE_FRONT)]);

    /* set GSC pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_FRONT)] = new ExynosCameraFrameEntity(PIPE_GSC_FRONT, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_FRONT)]);

    /* set GSC-Video pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_VIDEO_FRONT)] = new ExynosCameraFrameEntity(PIPE_GSC_VIDEO_FRONT, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VIDEO_FRONT)]);

    /* set JPEG pipe to linkageList */
    newEntity[INDEX(PIPE_JPEG_FRONT)] = new ExynosCameraFrameEntity(PIPE_JPEG_FRONT, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_JPEG_FRONT)]);

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

status_t ExynosCameraFrameFactoryFront::initPipes(void)
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
    tempRect.fullW = maxSensorW + 16;
    tempRect.fullH = maxSensorH + 10;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = FRONT_NUM_BAYER_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    /* packed bayer bytesPerPlane */
    pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
#endif

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* ISP pipe */
    tempRect.fullW = maxSensorW + 16;
    tempRect.fullH = maxSensorH + 10;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = FRONT_NUM_BAYER_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 3; /* ISP, SCP */
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_ISP;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (FIMC_IS_VIDEO_ISP_NUM - FIMC_IS_VIDEO_SS0_NUM);
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameVideoID = (FIMC_IS_VIDEO_SCC_NUM - FIMC_IS_VIDEO_SS0_NUM);
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[1].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[1].perFrameVideoID = (FIMC_IS_VIDEO_SCP_NUM - FIMC_IS_VIDEO_SS0_NUM);

#ifdef CAMERA_PACKED_BAYER_ENABLE
    /* packed bayer bytesPerPlane */
    pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
#endif

    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* SCC output pipe */
    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = pictureFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = FRONT_NUM_PICTURE_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* SCP pipe */
    hwPreviewW = m_parameters->getHwPreviewStride();
    ALOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, hwPreviewW);
    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = previewFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = NUM_PREVIEW_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    m_frameCount = 0;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryFront::preparePipes(void)
{
    int ret = 0;

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryFront::startPipes(void)
{
    int ret = 0;

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }


    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* stream on for ISP */
    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->sensorStream(true);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE sensorStream on fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Starting Front [FLITE>3AC>SCC>SCP>ISP] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryFront::startInitialThreads(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    ret = startThread(PIPE_FLITE_FRONT);
    if (ret < 0)
        return ret;

    ret = startThread(PIPE_ISP_FRONT);
    if (ret < 0)
        return ret;

    if (m_parameters->getUseDynamicScc() == false) {
        ret = startThread(PIPE_SCC_FRONT);
        if (ret < 0)
            return ret;
    }

    ret = startThread(PIPE_SCP_FRONT);
    if (ret < 0)
        return ret;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryFront::setStopFlag(void)
{
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->setStopFlag();

    return NO_ERROR;
}


status_t ExynosCameraFrameFactoryFront::stopPipes(void)
{
    int ret = 0;

    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }


    /* stream off for ISP */
    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->sensorStream(false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE sensorStream fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->setControl(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }


    ret = m_pipes[INDEX(PIPE_FLITE_FRONT)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* stream off for ISP */
    ret = m_pipes[INDEX(PIPE_ISP_FRONT)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC_FRONT)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCP_FRONT)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Stopping Front [FLITE>3AA>ISP>SCC>SCP] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCameraFrameFactoryFront::setRequest3AC(bool enable)
{
    /* Front dosen't use 3AC */
    m_request3AC = 0;
}

}; /* namespace android */
