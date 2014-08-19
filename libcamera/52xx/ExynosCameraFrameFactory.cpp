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
#define LOG_TAG "ExynosCameraFrameFactory"
#include <cutils/log.h>

#include "ExynosCameraFrameFactory.h"
#include "ExynosCameraFrameFactoryFront.h"

namespace android {

ExynosCameraFrameFactory::ExynosCameraFrameFactory() {
    m_cameraId = 0;
    m_parameters = NULL;
    m_frameCount = 0;

    /* Activity Control */
    m_activityControl = NULL;

    for (int i = 0; i < MAX_NUM_PIPES; i++)
        m_pipes[i] = NULL;

    m_request3AP = 0;
    m_request3AC = 0;
    m_requestISP = 1;
    m_requestSCC = 0;
    m_requestDIS = 0;
    m_requestSCP = 1;

    m_instanceOnMaxCnt = 0;
    m_instanceOnCurCnt = 0;

    m_bypassDRC = true;
    m_bypassDIS = true;
    m_bypassDNR = true;
    m_bypassFD = true;
}

ExynosCameraFrameFactory::ExynosCameraFrameFactory(int cameraId, ExynosCameraParameters *param)
{
    m_cameraId = cameraId;
    m_parameters = param;
    m_frameCount = 0;

    /* Activity Control */
    m_activityControl = m_parameters->getActivityControl();

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

ExynosCameraFrameFactory::~ExynosCameraFrameFactory()
{
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        ALOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);
}

ExynosCameraFrameFactory *ExynosCameraFrameFactory::createFrameFactory(int cameraId, ExynosCameraParameters *param)
{
    if (cameraId == CAMERA_ID_BACK) {
        return new ExynosCameraFrameFactory(cameraId, param);
    } else if (cameraId == CAMERA_ID_FRONT) {
        return (ExynosCameraFrameFactory*)new ExynosCameraFrameFactoryFront(cameraId, param);
    } else {
        ALOGE("ERR(%s[%d]):Unknown cameraId(%d)", __FUNCTION__, __LINE__, cameraId);
    }
    return NULL;
}

status_t ExynosCameraFrameFactory::create(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    int sensorId = getSensorId(m_cameraId);
    int32_t nodeNums[MAX_NODE] = {-1};
    int32_t sensorIds[MAX_NODE] = {-1};

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = MAIN_CAMERA_FLITE_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_FLITE)] = (ExynosCameraPipe*)new ExynosCameraPipeFlite(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_FLITE)]->setPipeId(PIPE_FLITE);
    m_pipes[INDEX(PIPE_FLITE)]->setPipeName("PIPE_FLITE");

    nodeNums[OUTPUT_NODE] = FIMC_IS_VIDEO_ISP_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_ISP)] = (ExynosCameraPipe*)new ExynosCameraPipeISP(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_ISP)]->setPipeId(PIPE_ISP);
    m_pipes[INDEX(PIPE_ISP)]->setPipeName("PIPE_ISP");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCC_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_SCC)] = (ExynosCameraPipe*)new ExynosCameraPipeSCC(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_SCC)]->setPipeId(PIPE_SCC);
    m_pipes[INDEX(PIPE_SCC)]->setPipeName("PIPE_SCC");

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCP_NUM;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_SCP)] = (ExynosCameraPipe*)new ExynosCameraPipeSCP(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_SCP)]->setPipeId(PIPE_SCP);
    m_pipes[INDEX(PIPE_SCP)]->setPipeName("PIPE_SCP");

    nodeNums[OUTPUT_NODE] = PREVIEW_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_GSC)]->setPipeId(PIPE_GSC);
    m_pipes[INDEX(PIPE_GSC)]->setPipeName("PIPE_GSC");

    nodeNums[OUTPUT_NODE] = VIDEO_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC_VIDEO)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, false, nodeNums);
    m_pipes[INDEX(PIPE_GSC_VIDEO)]->setPipeId(PIPE_GSC_VIDEO);
    m_pipes[INDEX(PIPE_GSC_VIDEO)]->setPipeName("PIPE_GSC_VIDEO");

    nodeNums[OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_GSC_PICTURE)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_GSC_PICTURE)]->setPipeId(PIPE_GSC_PICTURE);
    m_pipes[INDEX(PIPE_GSC_PICTURE)]->setPipeName("PIPE_GSC_PICTURE");  

    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;
    m_pipes[INDEX(PIPE_JPEG)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, true, nodeNums);
    m_pipes[INDEX(PIPE_JPEG)]->setPipeId(PIPE_JPEG);
    m_pipes[INDEX(PIPE_JPEG)]->setPipeName("PIPE_JPEG");

    /* flite pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SS0_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_FLITE)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_FLITE));

    /* ISP pipe initialize */
    sensorIds[OUTPUT_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((MAIN_CAMERA_FLITE_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT)
                   | ((MAIN_CAMERA_3AA_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[CAPTURE_NODE] = -1;
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_ISP)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_ISP));

    /* SCC pipe initialize
     * TODO: preview Frame dose not need SCC
     *       Will be removed after driver fix.
     */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SCC_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_SCC)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_SCC));

    /* SCP pipe initialize */
    sensorIds[OUTPUT_NODE] = -1;
    sensorIds[CAPTURE_NODE] = (0 << REPROCESSING_SHIFT)
                   | ((FIMC_IS_VIDEO_SCP_NUM - FIMC_IS_VIDEO_SS0_NUM) << TAX_VINDEX_SHIFT)
                   | (sensorId << 0);
    sensorIds[SUB_NODE] = -1;
    ret = m_pipes[INDEX(PIPE_SCP)]->create(sensorIds);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_SCP));

    /* GSC pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC));

    /* GSC pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_VIDEO)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_VIDEO));

    /* GSC pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_PICTURE)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_PICTURE));

    /* JPEG pipe initialize */
    ret = m_pipes[INDEX(PIPE_JPEG)]->create(NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    ALOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_JPEG));

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::destroy(void)
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

status_t ExynosCameraFrameFactory::m_initFrameMetadata(ExynosCameraFrame *frame)
{
    int ret = 0;

    struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]): new struct camera2_shot_ext fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    memset(shot_ext, 0x00, sizeof(struct camera2_shot_ext));

    shot_ext->shot.magicNumber = SHOT_MAGIC_NUMBER;

    /* TODO: These bypass values are enabled at per-frame control */
#if 0
    setMetaBypassDrc(&shot_ext, m_bypassDRC);
    setMetaBypassDnr(&shot_ext, m_bypassDNR);
    setMetaBypassDis(&shot_ext, m_bypassDIS);
    setMetaBypassFd(&shot_ext, m_bypassFD);
#endif

    ret = frame->initMetaData(shot_ext);
    if (ret < 0)
        ALOGE("ERR(%s[%d]): initMetaData fail", __FUNCTION__, __LINE__);

    frame->setRequest(m_request3AP,
                      m_request3AC,
                      m_requestISP,
                      m_requestSCC,
                      m_requestDIS,
                      m_requestSCP);

    delete shot_ext;
    shot_ext = NULL;

    return ret;
}

status_t ExynosCameraFrameFactory::fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers)
{
    ALOGI("INFO(%s[%d]): Start", __FUNCTION__, __LINE__);

    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *queueFrame = NULL;
    ExynosCameraFrameEntity *newEntity = NULL;
    ExynosCameraFrameEntity *prevEntity = NULL;
    ExynosCameraList<ExynosCameraFrame *> instantQ;
    frame_queue_t *instanceOnDoneQ = NULL;

    m_instanceOnMaxCnt = numFrames;
    m_instanceOnCurCnt = 0;
    instanceOnDoneQ = new frame_queue_t;

    /* TODO 1. setup pipes for 120FPS */
    camera_pipe_info_t pipeInfo[3];
    ExynosRect tempRect;
    int hwSensorW = 0, hwSensorH = 0;
    int hwPreviewW = 0, hwPreviewH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();
    ExynosRect bayerCropSize;
    uint32_t frameRate = 0;
    struct v4l2_streamparm streamParam;
    sp<Thread> instanceOnThread;
    int count = 0;

#if 0
    m_parameters->getMaxSensorSize(&bayerCropSize.w, &bayerCropSize.h);
    frameRate = 30;
    bayerCropSize.w = 3248;
    bayerCropSize.h = 2438;
    bayerCropSize.x = 0;
    bayerCropSize.y = 0;
    hwSensorW  = bayerCropSize.w;
    hwSensorH  = bayerCropSize.h;
    hwPreviewW = hwSensorW;
    hwPreviewH = hwSensorH;
/*
    frameRate = 30;
    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
*/
#else
    frameRate = FASTEN_AE_FPS;
    bayerCropSize.w = FASTEN_AE_WIDTH;
    bayerCropSize.h = FASTEN_AE_HEIGHT;
    bayerCropSize.x = 0;
    bayerCropSize.y = 0;
    hwSensorW  = bayerCropSize.w;
    hwSensorH  = bayerCropSize.h;
    hwPreviewW = hwSensorW;
    hwPreviewH = hwSensorH;
#endif

    ALOGI("INFO(%s[%d]): hwSensorSize (%dx%d)", __FUNCTION__, __LINE__, hwSensorW, hwSensorH);

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* FLITE pipe */
    tempRect.fullW = hwSensorW + 16;
    tempRect.fullH = hwSensorH + 10;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = NUM_FASTAESTABLE_BUFFER;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    /* packed bayer bytesPerPlane */
    pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
#endif

    ret = m_pipes[INDEX(PIPE_FLITE)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        goto cleanup;
    }

    /* setParam for Frame rate */
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = frameRate;
    ALOGI("INFO(%s[%d]:set framerate (denominator=%d)", __FUNCTION__, __LINE__, frameRate);
    ret = setParam(streamParam, PIPE_FLITE);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE setParam fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* ISP pipe */
    tempRect.fullW = hwSensorW + 16;
    tempRect.fullH = hwSensorH + 10;
#if 0
    tempRect.w = hwSensorW + 16;
    tempRect.h = hwSensorH + 10;
    tempRect.x = 0;
    tempRect.y = 0;
#endif
    tempRect.colorFormat = bayerFormat;

    /* ISP pipe */
    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = numFrames;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 3;
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

    ret = m_pipes[INDEX(PIPE_ISP)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        goto cleanup;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* SCC pipe */
    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = pictureFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = NUM_PREVIEW_BUFFERS;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_SCC)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        goto cleanup;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* SCP pipe */
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

    ret = m_pipes[INDEX(PIPE_SCP)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        goto cleanup;
    }

    for (int i = 0; i < numFrames; i++) {
        /* 2. generate instant frames */
        newFrame = new ExynosCameraFrame(m_parameters, i);

        ret = m_initFrameMetadata(newFrame);
        if (ret < 0)
            ALOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, i);

        prevEntity = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
        newFrame->addSiblingEntity(NULL, prevEntity);

        newEntity = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        newFrame->addChildEntity(prevEntity, newEntity);

        newFrame->setNumRequestPipe(1);
        ret = m_initPipelines(newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s):m_initPipelines fail, ret(%d)", __FUNCTION__, ret);
        }

        camera2_node_group node_group_info_isp;
        int zoom = m_parameters->getZoomLevel();
        memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
        node_group_info_isp.leader.request = 1;
        node_group_info_isp.leader.input.cropRegion[0] = 0;
        node_group_info_isp.leader.input.cropRegion[1] = 0;
        node_group_info_isp.leader.input.cropRegion[2] = bayerCropSize.w;
        node_group_info_isp.leader.input.cropRegion[3] = bayerCropSize.h;
        node_group_info_isp.leader.output.cropRegion[0] = node_group_info_isp.leader.input.cropRegion[0];
        node_group_info_isp.leader.output.cropRegion[1] = node_group_info_isp.leader.input.cropRegion[1];
        node_group_info_isp.leader.output.cropRegion[2] = node_group_info_isp.leader.input.cropRegion[2];
        node_group_info_isp.leader.output.cropRegion[3] = node_group_info_isp.leader.input.cropRegion[3];
        newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoom);

        setRequestSCC(false);
        setRequestSCP(false);

        prevEntity->setDstBuf(buffers[i]);
        newEntity->setSrcBuf(buffers[i]);

        /* set metadata for instant on */
        camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buffers[i].addr[1]);
        newFrame->getMetaData(shot_ext);
        if (shot_ext != NULL) {
            shot_ext->shot.ctl.scaler.cropRegion[0] = 0;
            shot_ext->shot.ctl.scaler.cropRegion[1] = 0;
            shot_ext->shot.ctl.scaler.cropRegion[2] = hwPreviewW;
            shot_ext->shot.ctl.scaler.cropRegion[3] = hwPreviewH;
        }

        /* 3. push instance frames to pipe */
        ret = pushFrameToPipe(&newFrame, PIPE_FLITE);
        if (ret < 0) {
            ALOGE("DEBUG(%s[%d]): pushFrameToPipeFail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }
        /* 3. push instance frames to doneQ*/
        ret = setOutputFrameQToPipe(instanceOnDoneQ, PIPE_ISP);
        if (ret < 0) {
            ALOGE("DEBUG(%s[%d]): pushFrameToPipeFail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }

        ALOGD("DEBUG(%s[%d]): Instant shot - FD(%d, %d) frameCnt(%d) frame(%p)", __FUNCTION__, __LINE__, buffers[i].fd[0], buffers[i].fd[1], newFrame->getFrameCount(), newFrame);

        instantQ.pushProcessQ(&newFrame);
    }

    for( int k = 0 ; k < numFrames ; k++ )
    {
        ret = m_pipes[INDEX(PIPE_FLITE)]->instantOnQbuf(&queueFrame, BUFFER_POS::DST);
        if (ret < 0) {
            ALOGE("DEBUG(%s[%d]): FLITE instantOnQbuf fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto cleanup;
        }
    }

    /* 4. pipe instant on */
    ret = m_pipes[INDEX(PIPE_FLITE)]->instantOn(numFrames);
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): PIPE_FLITE On fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->instantOn(0);
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): PIPE_ISP On fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

    instanceOnThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraFrameFactory::m_instanceOnThreadFunc, "InstanceOnThread", PRIORITY_URGENT_DISPLAY);
    instanceOnThread->run();

    /* 5. setControl to sensor instant on */
    ret = m_pipes[INDEX(PIPE_FLITE)]->setControl(V4L2_CID_IS_S_STREAM, (1 | 1 << SENSOR_NOBLOCK_SHIFT | numFrames << SENSOR_INSTANT_SHIFT));
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): instantOn fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto cleanup;
    }

    while( true ) {
        newFrame = NULL;
        ret = instanceOnDoneQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            /* TODO: We need to make timeout duration depends on FPS */
            if (ret == TIMED_OUT) {
                ALOGW("WARN(%s):wait timeout", __FUNCTION__);
            } else {
                ALOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
                /* TODO: doing exception handling */
            }
            continue;
        }
        count++;
        ALOGE("Instance On wait Frame count(%d) numFrames(%d)", count, numFrames);
        if( numFrames <= count )
            break;
    }

    instanceOnThread->requestExit();

    /* 6. pipe instant off */
    ret = m_pipes[INDEX(PIPE_FLITE)]->instantOff();
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): FLITE Off fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->instantOff();
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): ISP Off fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

cleanup:
    newFrame = NULL;

    /* clean up all frames */
    for (int i = 0; i < numFrames; i++) {
        if (instantQ.getSizeOfProcessQ() == 0)
            break;

        ret = instantQ.popProcessQ(&newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): pop instantQ fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            continue;
        }
        if (newFrame == NULL) {
            ALOGE("ERR(%s[%d]): newFrame is NULL,", __FUNCTION__, __LINE__);
            continue;
        }

        delete newFrame;
        newFrame = NULL;
    }

    if (instanceOnDoneQ != NULL) {
        delete instanceOnDoneQ;
        instanceOnDoneQ = NULL;
    }

    ALOGI("INFO(%s[%d]): Done", __FUNCTION__, __LINE__);
    return ret;
}

bool ExynosCameraFrameFactory::m_instanceOnThreadFunc(void)
{
    int ret = 0;
    ExynosCameraFrame *queueFrame = NULL;


    ret = m_pipes[INDEX(PIPE_FLITE)]->instantOnDQbuf(&queueFrame, BUFFER_POS::DST);
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): FLITE instantOnDQbuf fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->instantOnPushFrameQ(BUFFERQ_TYPE::INPUT, &queueFrame);
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): FLITE instantOnPushFrameQ fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->instantOnQbuf(&queueFrame, BUFFER_POS::SRC);
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): PIPE_ISP instantOnQbuf fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->instantOnPushFrameQ(BUFFERQ_TYPE::OUTPUT, &queueFrame);
    if (ret < 0) {
        ALOGE("DEBUG(%s[%d]): PIPE_ISP instantOnPushFrameQ fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    m_instanceOnCurCnt++;

    if( m_instanceOnCurCnt >= m_instanceOnMaxCnt )
    {
        return false;
    }

    return true;

}


status_t ExynosCameraFrameFactory::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_isp;
    int zoom = m_parameters->getZoomLevel();
    int previewW = 0, previewH = 0;
    ExynosRect sensorSize;       /* == bayerCropInputSize */
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;

    m_parameters->getPreviewSize(&previewW, &previewH);

#ifdef SUPPORT_SW_VDIS
	m_parameters->getHwPreviewSize(&previewW, &previewH);
#endif /*SUPPORT_SW_VDIS*/

    m_parameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);
    m_parameters->getPreviewBdsSize(&bdsSize);
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));

    node_group_info_isp.leader.request = 1;
    node_group_info_isp.capture[PERFRAME_BACK_SCC_POS].request = m_requestSCC;
    node_group_info_isp.capture[PERFRAME_BACK_SCP_POS].request = m_requestSCP;

    updateNodeGroupInfoMainPreview(
            m_cameraId,
            &node_group_info_isp,
            bayerCropSize,
            bdsSize,
            previewW, previewH);

    frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoom);

    return NO_ERROR;
}

ExynosCameraFrame *ExynosCameraFrameFactory::createNewFrameOnlyOnePipe(int pipeId)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {};
    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);

    /* set pipe to linkageList */
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);

    return frame;
}

ExynosCameraFrame *ExynosCameraFrameFactory::createNewFrameVideoOnly(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {};
    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);

    /* set GSC-Video pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_VIDEO)] = new ExynosCameraFrameEntity(PIPE_GSC_VIDEO, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VIDEO)]);

    return frame;
}

ExynosCameraFrame *ExynosCameraFrameFactory::createNewFrame(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {};
    ExynosCameraFrame *frame = new ExynosCameraFrame(m_parameters, m_frameCount);
    int requestEntityCount = 0;

    setRequestSCP(true);

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        ALOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

    /* set FLITE pipe to linkageList */
    newEntity[INDEX(PIPE_FLITE)] = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE)]);
    requestEntityCount++;

    /* set ISP pipe to linkageList */
    newEntity[INDEX(PIPE_ISP)] = new ExynosCameraFrameEntity(PIPE_ISP, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addChildEntity(newEntity[INDEX(PIPE_FLITE)], newEntity[INDEX(PIPE_ISP)]);

    /* set SCC pipe to linkageList */
    newEntity[INDEX(PIPE_SCC)] = new ExynosCameraFrameEntity(PIPE_SCC, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_DELIVERY);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SCC)]);
    requestEntityCount++;

    /* set SCP pipe to linkageList */
    newEntity[INDEX(PIPE_SCP)] = new ExynosCameraFrameEntity(PIPE_SCP, ENTITY_TYPE_OUTPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SCP)]);
    requestEntityCount++;

    /* set GSC pipe to linkageList */
    newEntity[INDEX(PIPE_GSC)] = new ExynosCameraFrameEntity(PIPE_GSC, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC)]);

    /* set GSC-Picture pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_PICTURE)] = new ExynosCameraFrameEntity(PIPE_GSC_PICTURE, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_PICTURE)]);

    /* set GSC-Video pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_VIDEO)] = new ExynosCameraFrameEntity(PIPE_GSC_VIDEO, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_VIDEO)]);

    /* set JPEG pipe to linkageList */
    newEntity[INDEX(PIPE_JPEG)] = new ExynosCameraFrameEntity(PIPE_JPEG, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_JPEG)]);

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

status_t ExynosCameraFrameFactory::initPipes(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    camera_pipe_info_t pipeInfo[3];
    ExynosRect tempRect;
    int maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int hwPictureW = 0, hwPictureH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);

    ALOGI("INFO(%s[%d]): MaxSensorSize(%dx%d), HWSensorSize(%dx%d)", __FUNCTION__, __LINE__, maxSensorW, maxSensorH, hwSensorW, hwSensorH);
    ALOGI("INFO(%s[%d]): MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", __FUNCTION__, __LINE__, maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    ALOGI("INFO(%s[%d]): HWPictureSize(%dx%d)", __FUNCTION__, __LINE__, hwPictureW, hwPictureH);

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* FLITE pipe */
#ifdef FIXED_SENSOR_SIZE
    tempRect.fullW = maxSensorW + 16;
    tempRect.fullH = maxSensorH + 10;
#else
    tempRect.fullW = hwSensorW + 16;
    tempRect.fullH = hwSensorH + 10;
#endif
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
        /* packed bayer bytesPerPlane */
    pipeInfo[0].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
#endif

    ret = m_pipes[INDEX(PIPE_FLITE)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* setParam for Frame rate */
    uint32_t min, max, frameRate;
    struct v4l2_streamparm streamParam;

    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));
    m_parameters->getPreviewFpsRange(&min, &max);

    if (m_parameters->getScalableSensorMode() == true)
        frameRate = 24;
    else
        frameRate = max;

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = frameRate;
    ALOGI("INFO(%s[%d]:set framerate (denominator=%d)", __FUNCTION__, __LINE__, frameRate);
    ret = setParam(streamParam, PIPE_FLITE);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE setParam fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return INVALID_OPERATION;
    }

#if 0
    /* setParam for Frame rate */
    int bnsScaleRatio = m_parameters->getBnsScaleRatio();
    int bnsSize = 0;
    ret = m_pipes[PIPE_FLITE]->setControl(V4L2_CID_IS_S_BNS, bnsScaleRatio);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): set BNS(%d) fail, ret(%d)", __FUNCTION__, __LINE__, bnsScaleRatio, ret);
    } else {
        ret = m_pipes[PIPE_FLITE]->getControl(V4L2_CID_IS_G_BNS_SIZE, &bnsSize);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): get BNS size fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            bnsSize = -1;
        }
    }

    int bnsWidth = 0;
    int bnsHeight = 0;
    if (bnsSize > 0) {
        bnsHeight = bnsSize & 0xffff;
        bnsWidth = bnsSize >> 16;

        ALOGI("INFO(%s[%d]): BNS scale down ratio(%.1f), size (%dx%d)", __FUNCTION__, __LINE__, (float)(bnsScaleRatio / 1000), bnsWidth, bnsHeight);

        m_parameters->setBnsSize(bnsWidth - 16, bnsHeight - 10);
    }
#endif
    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

 
    /* ISP pipe */
#ifdef FIXED_SENSOR_SIZE
    tempRect.fullW = maxSensorW + 16;
    tempRect.fullH = maxSensorH + 10;
#else
    tempRect.fullW = hwSensorW + 16;
    tempRect.fullH = hwSensorH + 10;
#endif
    tempRect.colorFormat = bayerFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
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

    ret = m_pipes[INDEX(PIPE_ISP)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* SCC output pipe */
#ifdef FIXED_SENSOR_SIZE
    tempRect.fullW = maxSensorW;
    tempRect.fullH = maxSensorH;
#else
    tempRect.fullW = hwSensorW;
    tempRect.fullH = hwSensorH;
#endif
    tempRect.colorFormat = pictureFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = config->current->bufInfo.num_picture_buffers;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
     
    ret = m_pipes[INDEX(PIPE_SCC)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    memset(pipeInfo, 0, (sizeof(camera_pipe_info_t) * 3));

    /* SCP pipe */
    int stride = m_parameters->getHwPreviewStride();
    ALOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, stride);
    tempRect.fullW = stride;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = previewFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = config->current->bufInfo.num_preview_buffers;
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[0].bytesPerPlane[0] = hwPreviewW;
#endif

    ret = m_pipes[INDEX(PIPE_SCP)]->setupPipe(pipeInfo);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    m_frameCount = 0;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::m_initPipelines(ExynosCameraFrame *frame)
{
    ExynosCameraFrameEntity *curEntity = NULL;
    ExynosCameraFrameEntity *childEntity = NULL;
    frame_queue_t *frameQ = NULL;
    int ret = 0;

    curEntity = frame->getFirstEntity();

    while (curEntity != NULL) {
        childEntity = curEntity->getNextEntity();
        if (childEntity != NULL) {
            ret = getInputFrameQToPipe(&frameQ, childEntity->getPipeId());
            if (ret < 0 || frameQ == NULL) {
                ALOGE("ERR(%s):getInputFrameQToPipe fail, ret(%d), frameQ(%p)", __FUNCTION__, ret, frameQ);
                return ret;
            }

            ret = setOutputFrameQToPipe(frameQ, curEntity->getPipeId());
            if (ret < 0) {
                ALOGE("ERR(%s):setOutputFrameQToPipe fail, ret(%d)", __FUNCTION__, ret);
                return ret;
            }

            /* check Image Configuration Equality */
            ret = m_checkPipeInfo(curEntity->getPipeId(), childEntity->getPipeId());
            if (ret < 0) {
                ALOGE("ERR(%s):checkPipeInfo fail, Pipe[%d], Pipe[%d]", __FUNCTION__, curEntity->getPipeId(), childEntity->getPipeId());
                return ret;
            }

            curEntity = childEntity;
        } else {
            curEntity = frame->getNextEntity();
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::preparePipes(void)
{
    int ret = 0;

    /* NOTE: Prepare for 3AA is moved after ISP stream on */

    ret = m_pipes[INDEX(PIPE_FLITE)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCP)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC)]->prepare();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::pushFrameToPipe(ExynosCameraFrame **newFrame, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->pushFrame(newFrame);
    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::setOutputFrameQToPipe(frame_queue_t *outputQ, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->setOutputFrameQ(outputQ);
    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::getOutputFrameQToPipe(frame_queue_t **outputQ, uint32_t pipeId)
{
    ALOGV("DEBUG(%s[%d]):pipeId=%d", __FUNCTION__, __LINE__, pipeId);
    m_pipes[INDEX(pipeId)]->getOutputFrameQ(outputQ);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::getInputFrameQToPipe(frame_queue_t **inputFrameQ, uint32_t pipeId)
{
    ALOGV("DEBUG(%s[%d]):pipeId=%d", __FUNCTION__, __LINE__, pipeId);

    m_pipes[INDEX(pipeId)]->getInputFrameQ(inputFrameQ);

    if (inputFrameQ == NULL)
        ALOGE("ERR(%s[%d])inputFrameQ is NULL", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::getThreadState(int **threadState, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->getThreadState(threadState);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::getThreadInterval(uint64_t **threadInterval, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->getThreadInterval(threadInterval);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::getThreadRenew(int **threadRenew, uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->getThreadRenew(threadRenew);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::incThreadRenew(uint32_t pipeId)
{
    m_pipes[INDEX(pipeId)]->incThreadRenew();

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::startPipes(void)
{
    int ret = 0;

    ret = m_pipes[INDEX(PIPE_FLITE)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }


    ret = m_pipes[INDEX(PIPE_SCP)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* stream on for 3AA */
    ret = m_pipes[INDEX(PIPE_ISP)]->start();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->sensorStream(true);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE sensorStream on fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ALOGI("INFO(%s[%d]):Starting [FLITE>3AC>SCP>3AA_ISP] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::startInitialThreads(void)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    ret = startThread(PIPE_FLITE);
    if (ret < 0)
        return ret;

    ret = startThread(PIPE_SCP);
    if (ret < 0)
        return ret;

    if (m_parameters->getUseDynamicScc() == false) {
        ret = startThread(PIPE_SCC);
        if (ret < 0)
            return ret;
    }

    ret = startThread(PIPE_ISP);
    if (ret < 0)
        return ret;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::startThread(uint32_t pipeId)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):pipeId=%d", __FUNCTION__, __LINE__, pipeId);

    ret = m_pipes[INDEX(pipeId)]->startThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):start thread fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        /* TODO: exception handling */
    }
    return ret;
}

status_t ExynosCameraFrameFactory::stopThread(uint32_t pipeId)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):pipeId=%d", __FUNCTION__, __LINE__, pipeId);

    ret = m_pipes[INDEX(pipeId)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):stop thread fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        /* TODO: exception handling */
    }
    return ret;
}

status_t ExynosCameraFrameFactory::setStopFlag(void)
{
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;

    ret = m_pipes[INDEX(PIPE_FLITE)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_ISP)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_SCP)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_SCC)]->setStopFlag();

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::stopPipes(void)
{
    int ret = 0;

    ret = m_pipes[INDEX(PIPE_SCP)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_SCC)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->stopThread();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->sensorStream(false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE sensorStream off fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* 3AA force done */
    ret = m_pipes[INDEX(PIPE_ISP)]->setControl(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):3AA_ISP force done fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    /* stream off for FLITE */
    ret = m_pipes[INDEX(PIPE_FLITE)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):FLITE stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    /* stream off for 3AA, ISP */
    ret = m_pipes[INDEX(PIPE_ISP)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):3AA_ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    ret = m_pipes[INDEX(PIPE_SCP)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    ret = m_pipes[INDEX(PIPE_SCC)]->stop();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):SCP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        /* return INVALID_OPERATION; */
    }

    ALOGI("INFO(%s[%d]):Stopping [FLITE>3AA_ISP>3AC>SCP] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCameraFrameFactory::dump()
{
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            m_pipes[i]->dump();
        }
    }

    return;
}

void ExynosCameraFrameFactory::setRequest3AC(bool enable)
{
    m_request3AC = enable ? 1 : 0;
}

void ExynosCameraFrameFactory::setRequestSCC(bool enable)
{
    m_requestSCC = enable ? 1 : 0;
}

void ExynosCameraFrameFactory::setRequestSCP(bool enable)
{
    m_requestSCP = enable ? 1 : 0;
}

status_t ExynosCameraFrameFactory::setParam(struct v4l2_streamparm streamParam, uint32_t pipeId)
{
    int ret = 0;

    ret = m_pipes[INDEX(pipeId)]->setParam(streamParam);

    return ret;
}

status_t ExynosCameraFrameFactory::m_checkPipeInfo(uint32_t srcPipeId, uint32_t dstPipeId)
{
    int srcFullW, srcFullH, srcColorFormat;
    int dstFullW, dstFullH, dstColorFormat;
    int isDifferent = 0;
    int ret = 0;

    ret = m_pipes[INDEX(srcPipeId)]->getPipeInfo(&srcFullW, &srcFullH, &srcColorFormat, SRC_PIPE);
    if (ret < 0) {
        ALOGE("ERR(%s):Source getPipeInfo fail", __FUNCTION__);
        return ret;
    }
    ret = m_pipes[INDEX(dstPipeId)]->getPipeInfo(&dstFullW, &dstFullH, &dstColorFormat, DST_PIPE);
    if (ret < 0) {
        ALOGE("ERR(%s):Destination getPipeInfo fail", __FUNCTION__);
        return ret;
    }

    if (srcFullW != dstFullW || srcFullH != dstFullH || srcColorFormat != dstColorFormat) {
        ALOGE("ERR(%s[%d]):Video Node Image Configuration is NOT matching", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory::setControl(int cid, int value, uint32_t pipeId)
{
    int ret = 0;

    ret = m_pipes[INDEX(pipeId)]->setControl(cid, value);

    return ret;
}


}; /* namespace android */
