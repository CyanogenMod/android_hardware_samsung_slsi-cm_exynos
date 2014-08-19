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
#define LOG_TAG "ExynosCameraFrameReprocessingFactory"
#include <cutils/log.h>

#include "ExynosCameraFrameReprocessingFactory.h"

namespace android {

ExynosCameraFrameReprocessingFactory::ExynosCameraFrameReprocessingFactory(int cameraId, ExynosCameraParameters *param)
{
    m_cameraId = cameraId;
    m_parameters = param;
    m_frameCount = 0;

    m_activityControl = m_parameters->getActivityControl();

    for (int i = 0; i < MAX_NUM_PIPES; i++)
        m_pipes[i] = NULL;

    m_request3AP = 0;
    m_request3AC = 0;
    m_requestISP = 1;
    m_requestSCC = 1;
    m_requestDIS = 0;
    m_requestSCP = 0;

    m_bypassDRC = true;
    m_bypassDIS = true;
    m_bypassDNR = true;
    m_bypassFD = true;
}

ExynosCameraFrameReprocessingFactory::~ExynosCameraFrameReprocessingFactory()
{
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            delete m_pipes[i];
            m_pipes[i] = NULL;
        }
    }
}

status_t ExynosCameraFrameReprocessingFactory::create(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    int sensorId = getSensorId(m_cameraId);
    int32_t nodeNums[MAX_NODE] = {-1};
    int32_t sensorIds[MAX_NODE] = {-1};

#if 0
    nodeNums[OUTPUT_NODE] = REPROCESSING_3AA_NUM;
    nodeNums[CAPTURE_NODE] = REPROCESSING_3AA_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_3AA_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipe3AA(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setPipeId(PIPE_3AA_REPROCESSING);
    m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setPipeName("PIPE_3AA_REPROCESSING");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = REPROCESSING_3AA_NUM + 1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_3AC_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipe3AC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->setPipeId(PIPE_3AC_REPROCESSING);
    m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->setPipeName("PIPE_3AC_REPROCESSING");
#endif

    nodeNums[OUTPUT_NODE] = FIMC_IS_VIDEO_ISP_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_ISP_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeISP(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setPipeId(PIPE_ISP_REPROCESSING);
    m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setPipeName("PIPE_ISP_REPROCESSING");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCC_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_SCC_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeSCC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->setPipeId(PIPE_SCC_REPROCESSING);
    m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->setPipeName("PIPE_SCC_REPROCESSING");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCP_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_SCP_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeSCP(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_SCP_REPROCESSING)]->setPipeId(PIPE_SCP_REPROCESSING);
    m_pipes[INDEX(PIPE_SCP_REPROCESSING)]->setPipeName("PIPE_SCP_REPROCESSING");

    nodeNums[OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->setPipeId(PIPE_GSC_REPROCESSING);
    m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->setPipeName("PIPE_GSC_REPROCESSING");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_JPEG_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->setPipeId(PIPE_JPEG_REPROCESSING);
    m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->setPipeName("PIPE_JPEG_REPROCESSING");

    ALOGD("DEBUG(%s[%d]):pipe ids %d %d %d %d %d",
            __FUNCTION__, __LINE__
            ,m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->getPipeId()
            ,m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->getPipeId()
            ,m_pipes[INDEX(PIPE_SCP_REPROCESSING)]->getPipeId()
            ,m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->getPipeId()
            ,m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->getPipeId()
            );

    /* PIPE_ISP_REPROCESSING pipe initialize */
    sensorIds[OUTPUT_NODE] = (1 << REPROCESSING_SHIFT)
                   | ((MAIN_CAMERA_FLITE_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | (((REPROCESSING_3AA_NUM + 2) - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[CAPTURE_NODE] = -1;
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_ISP_REPROCESSING));

#if 0
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* PIPE_3AA_REPROCESSING pipe initialize */
        sensorIds[OUTPUT_NODE] = (1 << REPROCESSING_SHIFT) | sensorId;
        sensorIds[CAPTURE_NODE] = (1 << REPROCESSING_SHIFT) | sensorId;
        sensorIds[SUB_NODE] = -1;
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->create(sensorIds);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):3AA create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_3AA_REPROCESSING));
    }

    /* PIPE_3AC_REPROCESSING pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (1 << REPROCESSING_SHIFT)
                   | ((REPROCESSING_3AA_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):3AC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_3AC_REPROCESSING));
#endif

    /* PIPE_SCC_REPROCESSING pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (1 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SCC_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_SCC_REPROCESSING));

    /* PIPE_SCP_REPROCESSING pipe initialize
     * TODO: preview Frame dose not need SCC
     *       Will be removed after driver fix.
     */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (1 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SCP_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_SCP_REPROCESSING)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_SCP_REPROCESSING));

    /* PIPE_GSC_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_REPROCESSING));

    /* PIPE_JPEG_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_JPEG_REPROCESSING));

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::destroy(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            delete m_pipes[i];
            m_pipes[i] = NULL;

            ALOGD("DEBUG(%s):Pipe(%d) destroyed", __FUNCTION__, INDEX(i));
        }
    }

    return NO_ERROR;
}

ExynosCameraFrame * ExynosCameraFrameReprocessingFactory::createNewFrame(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_PIPE_NUM_REPROCESSING];
    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);
    int requestEntityCount = 0;

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        ALOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* set 3AA pipe to linkageList */
        newEntity[PIPE_3AA_REPROCESSING] = new ExynosCameraFrameEntity(PIPE_3AA_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[PIPE_3AA_REPROCESSING]);
    } else {
        newEntity[PIPE_3AA_REPROCESSING] = NULL;
    }

    /* set ISP pipe to linkageList */
    newEntity[PIPE_ISP_REPROCESSING] = new ExynosCameraFrameEntity(PIPE_ISP_REPROCESSING, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
    if (newEntity[PIPE_3AA_REPROCESSING] == NULL)
        frame->addSiblingEntity(NULL, newEntity[PIPE_ISP_REPROCESSING]);
    else
        frame->addChildEntity(newEntity[PIPE_3AA_REPROCESSING], newEntity[PIPE_ISP_REPROCESSING]);
    requestEntityCount++;

    /* set SCC pipe to linkageList */
    newEntity[PIPE_SCC_REPROCESSING] = new ExynosCameraFrameEntity(PIPE_SCC_REPROCESSING, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[PIPE_SCC_REPROCESSING]);
    requestEntityCount++;

    /* set GSC pipe to linkageList */
    newEntity[PIPE_GSC_REPROCESSING] = new ExynosCameraFrameEntity(PIPE_GSC_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[PIPE_GSC_REPROCESSING]);
    requestEntityCount++;

    /* set JPEG pipe to linkageList */
    newEntity[PIPE_JPEG_REPROCESSING] = new ExynosCameraFrameEntity(PIPE_JPEG_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[PIPE_JPEG_REPROCESSING]);
    requestEntityCount++;

    ret = m_initPipelines(frame);
    if (ret < 0) {
        ALOGE("ERR(%s):m_initPipelines fail, ret(%d)", __FUNCTION__, ret);
    }

    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameReprocessingFactory::startInitialThreads(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    ret = startThread(PIPE_SCC_REPROCESSING);
    if (ret < 0)
        return ret;

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_3aa, node_group_info_isp;
    int zoom = m_parameters->getZoomLevel();
    int pictureW = 0, pictureH = 0;
    ExynosRect bnsSize;
    ExynosRect bayerCropSizePicture;
    ExynosRect bayerCropSizePreview;
    ExynosRect bdsSize;

    m_parameters->getPictureSize(&pictureW, &pictureH);
    m_parameters->getPictureBayerCropSize(&bnsSize, &bayerCropSizePicture);
    m_parameters->getPictureBdsSize(&bdsSize);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSizePreview);

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* should add this request value in FrameFactory */
        node_group_info_3aa.leader.request = 1;
        /* should add this request value in FrameFactory */
        node_group_info_3aa.capture[PERFRAME_REPROCESSING_3AP_POS].request = 1;
    }

    /* should add this request value in FrameFactory */
    node_group_info_isp.leader.request = 1;
    node_group_info_isp.capture[PERFRAME_REPROCESSING_SCC_POS].request = 1;

    updateNodeGroupInfoReprocessing(
            m_cameraId,
            &node_group_info_3aa,
            &node_group_info_isp,
            bayerCropSizePreview,
            bayerCropSizePicture,
            bdsSize,
            pictureW, pictureH,
            m_parameters->getUsePureBayerReprocessing());

    frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_PURE_REPROCESSING_3AA, zoom);
    frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP, zoom);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::initPipes(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    camera_pipe_info_t pipeInfo[3];
    ExynosRect tempRect;
    int maxSensorW = 0, maxSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);

    ALOGI("INFO(%s[%d]): MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", __FUNCTION__, __LINE__, maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    ALOGI("INFO(%s[%d]): MaxPixtureSize(%dx%d), HwPixtureSize(%dx%d)", __FUNCTION__, __LINE__, maxPictureW, maxPictureH, hwPictureW, hwPictureH);

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

        /* 3AA pipe */
        tempRect.fullW = maxSensorW + 16;
        tempRect.fullH = maxSensorH + 10;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[0].rectInfo = tempRect;
        pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[0].bufInfo.count = NUM_BAYER_BUFFERS;
        /* per frame info */
#ifdef USE_PURE_BAYER_REPROCESSING
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 2; /* 3AA, 3AP */
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_PURE_REPROCESSING_3AA;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (MAIN_CAMERA_3AA_NUM - FIMC_IS_VIDEO_SS0_NUM);
        pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameVideoID = (MAIN_CAMERA_3AP_NUM - FIMC_IS_VIDEO_SS0_NUM);
#else
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
#endif

#ifdef CAMERA_PACKED_BAYER_ENABLE
        /* packed bayer bytesPerPlane */
        pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
#endif

        tempRect.fullW = maxPictureW;
        tempRect.fullH = maxPictureH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[1].rectInfo = tempRect;
        pipeInfo[1].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[1].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[1].bufInfo.count = NUM_BAYER_BUFFERS;
        /* per frame info */
        pipeInfo[1].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[1].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
        /* packed bayer bytesPerPlane */
        pipeInfo[1].bytesPerPlane[0] = ROUND_UP(pipeInfo[1].rectInfo.fullW * 3 / 2, 16);
#endif

        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setupPipe(pipeInfo);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* ISP pipe */
    tempRect.fullW = maxSensorW + 16;
    tempRect.fullH = maxSensorH + 10;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = NUM_BAYER_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 2; /* ISP, SCC*/
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
#ifdef USE_PURE_BAYER_REPROCESSING
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_PURE_REPROCESSING_ISP;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (FIMC_IS_VIDEO_ISP_NUM - FIMC_IS_VIDEO_SS0_NUM);
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameVideoID = (FIMC_IS_VIDEO_SCC_NUM - FIMC_IS_VIDEO_SS0_NUM);
#else
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIRTY_REPROCESSING_ISP;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (FIMC_IS_VIDEO_ISP_NUM - FIMC_IS_VIDEO_SS0_NUM);
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameCaptureInfo[0].perFrameVideoID = (FIMC_IS_VIDEO_SCC_NUM - FIMC_IS_VIDEO_SS0_NUM);
#endif

#if 0
    /* packed bayer bytesPerPlane */
    pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW * 3 / 2, 16);
#endif
#ifdef CAMERA_PACKED_BAYER_ENABLE
            /* packed bayer bytesPerPlane */
            pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
#endif

    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setupPipe(pipeInfo);
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
    pipeInfo[0].bufInfo.count = NUM_PICTURE_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* TODO: Why we need setup pipe for reprocessing SCP pipe
     *       Need to remove after driver fix
     */
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

    ret = m_pipes[INDEX(PIPE_SCP_REPROCESSING)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::preparePipes(void)
{
    int ret = 0;

    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::startPipes(void)
{
    int ret = 0;
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    /* Do not start SCC for preview instance */
    ret = m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* 3AA Reprocessing */
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->start();
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* ISP Reprocessing */
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Starting Reprocessing [SCC>ISP] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::stopPipes(void)
{
    int ret = 0;
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stopThread();
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stop();
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC_REPROCESSING)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Stopping Reprocessing [ISP>SCC] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

}; /* namespace android */
