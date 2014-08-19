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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPipe3AA"
#include <cutils/log.h>

#include "ExynosCameraPipe3AA.h"

namespace android {

ExynosCameraPipe3AA::ExynosCameraPipe3AA(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = isReprocessing ? 1 : 0;

    m_mainNodeNum = nodeNums[OUTPUT_NODE];

    m_subNode = NULL;
    m_numCaptureBuf = 0;
    m_setfile = 0x0;

    memset(&m_perframeSubNodeGroupInfo, 0x00, sizeof(camera_pipe_perframe_node_group_info_t));

    m_activityControl = m_parameters->getActivityControl();
    m_exynosconfig = m_parameters->getConfig();
}

ExynosCameraPipe3AA::~ExynosCameraPipe3AA()
{
    this->destroy();
}

status_t ExynosCameraPipe3AA::create(int32_t *sensorIds)
{
    ALOGI("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    int fd = -1;
    int sensorId = 1;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    /* 3AA output */
    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("3AA_OUTPUT");
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    ret = m_mainNode->open(m_mainNodeNum);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    ALOGD("DEBUG(%s):Node(%d) opened", __FUNCTION__, m_mainNodeNum);

    ret = m_mainNode->setInput(sensorIds[OUTPUT_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[OUTPUT_NODE], ret);
        return ret;
    }

    ret = m_mainNode->getFd(&fd);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s):mainNode->getFd failed", __FUNCTION__);
        return ret;
    }

    /* 3AA capture */
    m_subNode = new ExynosCameraNode();
    m_subNode->create("3AA_CAPTURE", fd);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): subNode create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    m_subNode->setInput(sensorIds[CAPTURE_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): subNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[CAPTURE_NODE], ret);
        return ret;
    }

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe3AA::m_mainThreadFunc, "3AAThread");

    if (m_reprocessing == true)
        m_inputFrameQ = new frame_queue_t(m_mainThread);
    else
        m_inputFrameQ = new frame_queue_t;
    m_inputFrameQ->setWaitTime(500000000); /* .5 sec */

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::destroy(void)
{
    int ret = 0;
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    if (m_mainNode != NULL) {
        ret = m_mainNode->close();
        if (ret < 0) {
            ALOGE("ERR(%s):3AA output node close fail(ret = %d)", __FUNCTION__, ret);
            return INVALID_OPERATION;
        }
        delete m_mainNode;
        m_mainNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) closed", __FUNCTION__, m_mainNodeNum);
    }

    if (m_subNode != NULL) {
        delete m_subNode;
        m_subNode = NULL;
    }

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::setupPipe(camera_pipe_info_t *pipeInfos)
{
    ALOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
#ifdef DEBUG_RAWDUMP
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
#endif

    /* TODO: check node state */
    /*       stream on? */

    /* initialize node */
    int maxW = pipeInfos[0].rectInfo.fullW;
    int maxH = pipeInfos[0].rectInfo.fullH;
    int colorFormat = pipeInfos[0].rectInfo.colorFormat;
    enum v4l2_buf_type bufType = (enum v4l2_buf_type)pipeInfos[0].bufInfo.type;
    enum v4l2_memory memType = (enum v4l2_memory)pipeInfos[0].bufInfo.memory;
    m_numBuffers = pipeInfos[0].bufInfo.count;

    m_perframeMainNodeGroupInfo = pipeInfos[0].perFrameNodeGroupInfo;

    /* 3AA output */
    m_mainNode->setSize(maxW, maxH);
    m_mainNode->setColorFormat(colorFormat, 2);
    m_mainNode->setBufferType(m_numBuffers, bufType, memType);

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = (maxW + 16) * 2;
        m_mainNode->setFormat();
    } else
#endif
    {
        m_mainNode->setFormat(pipeInfos[0].bytesPerPlane);
    }
    m_mainNode->reqBuffers();

    /* 3AA capture */
    maxW = pipeInfos[1].rectInfo.fullW;
    maxH = pipeInfos[1].rectInfo.fullH;
    colorFormat = pipeInfos[1].rectInfo.colorFormat;
    bufType = (enum v4l2_buf_type)pipeInfos[1].bufInfo.type;
    memType = (enum v4l2_memory)pipeInfos[1].bufInfo.memory;
    m_numCaptureBuf = pipeInfos[1].bufInfo.count;

    m_perframeSubNodeGroupInfo = pipeInfos[1].perFrameNodeGroupInfo;

    m_subNode->setSize(maxW, maxH);
    m_subNode->setColorFormat(colorFormat, 2);
    m_subNode->setBufferType(m_numCaptureBuf, bufType, memType);

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = (maxW + 16) * 2;
        m_subNode->setFormat();
    } else
#endif /* DEBUG_RAWDUMP */
    {
        m_subNode->setFormat(pipeInfos[1].bytesPerPlane);
    }
    m_subNode->reqBuffers();

    /* setfile setting */
#ifdef SET_SETFILE_BY_SHOT
    int setfile = 0;
    int yuvRange = 0;
    m_parameters->getSetfileYuvRange(m_reprocessing, &setfile, &yuvRange);

    m_setfile = mergeSetfileYuvRange(setfile, yuvRange);
#endif

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];

    ALOGI("INFO(%s[%d]):setupPipe() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    ALOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::prepare(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::start(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    /* TODO: stream on */
    ret = m_subNode->start();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]): Starting subNode Error!", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = m_mainNode->start();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]): Starting mainNode Error!", __FUNCTION__, __LINE__);
        return ret;
    }

    m_flagStartPipe = true;
    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::stop(void)
{
    ALOGD("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_flagStartPipe = false;
    m_flagTryStop = false;

    /* 3AA  output stop */
    ret = m_mainNode->stop();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA  output node stop fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA  output node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    /* 3A1 capture stop */
    ret = m_subNode->stop();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA  capture node stop fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_subNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA  capture node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    m_mainThread->requestExitAndWait();

    CLOGD("DEBUG(%s[%d]): thead exited", __FUNCTION__, __LINE__);

    m_inputFrameQ->release();

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::getPipeInfo(int *fullW, int *fullH, int *colorFormat, int pipePosition)
{
    int planeCount = 0;
    int ret = NO_ERROR;

    if (pipePosition == SRC_PIPE) {
        ret = m_subNode->getSize(fullW, fullH);
        if (ret < 0) {
            CLOGE("ERR(%s):getSize fail", __FUNCTION__);
            return ret;
        }

        ret = m_subNode->getColorFormat(colorFormat, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s):getColorFormat fail", __FUNCTION__);
            return ret;
        }
    } else {
        ret = ExynosCameraPipe::getPipeInfo(fullW, fullH, colorFormat, pipePosition);
    }

    return ret;
}

status_t ExynosCameraPipe3AA::m_putBuffer(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer fliteBuffer;
    ExynosCameraBuffer ispBuffer;

    int ret = 0;

    ALOGV("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
            m_mainNode->dumpState();
            m_subNode->dumpState();
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        ALOGE("ERR(%s):new frame is NULL", __FUNCTION__);
        return INVALID_OPERATION;
        /* return NO_ERROR; */
    }

    ret = newFrame->getSrcBuffer(getPipeId(), &fliteBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get src buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &ispBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get dst buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_runningFrameList[fliteBuffer.index] != NULL) {
        ALOGE("ERR(%s):new buffer is invalid, we already get buffer index(%d)",
            __FUNCTION__, fliteBuffer.index);
        dump();
        return BAD_VALUE;
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(fliteBuffer.addr[1]);

    if (shot_ext != NULL) {
        int previewW = 0, previewH = 0;
        int pictureW = 0, pictureH = 0;
        int cropW = 0, cropH = 0, cropX = 0, cropY = 0;

        m_parameters->getPreviewSize(&previewW, &previewH);
        m_parameters->getPictureSize(&pictureW, &pictureH);
        m_parameters->getHwBayerCropRegion(&cropW, &cropH, &cropX, &cropY);

        newFrame->getMetaData(shot_ext);
        ret = m_parameters->duplicateCtrlMetadata((void *)shot_ext);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):duplicate Ctrl metadata fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        setMetaSetfile(shot_ext, m_setfile);

        ALOGV("DEBUG(%s[%d]):frameCount(%d), rCount(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount(shot_ext));

        if (m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
            int zoomParamInfo = m_parameters->getZoomLevel();
            int zoomFrameInfo = 0;
            ExynosRect bnsSize;
            ExynosRect bayerCropSize;
            ExynosRect bdsSize;
            camera2_node_group node_group_info;
            camera2_node_group node_group_info_isp;

            newFrame->getNodeGroupInfo(&node_group_info, m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex, &zoomFrameInfo);

#ifdef PERFRAME_CONTROL_NODE_3AA
            /* HACK: To speed up DZOOM */
            if (zoomFrameInfo != zoomParamInfo) {
                ALOGI("INFO(%s[%d]):zoomFrameInfo(%d), zoomParamInfo(%d)",
                    __FUNCTION__, __LINE__, zoomFrameInfo, zoomParamInfo);

                newFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, &zoomFrameInfo);
                if (m_cameraId == CAMERA_ID_BACK) {
                    int pictureW = 0, pictureH = 0;
                    ExynosRect bayerCropSizePicture;
                    m_parameters->getPictureSize(&pictureW, &pictureH);
                    m_parameters->getPictureBayerCropSize(&bnsSize, &bayerCropSizePicture);
                    m_parameters->getPictureBdsSize(&bdsSize);
                    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
                    updateNodeGroupInfoReprocessing(
                        m_cameraId,
                        &node_group_info,
                        &node_group_info_isp,
                        bayerCropSize,
                        bayerCropSizePicture,
                        bdsSize,
                        pictureW, pictureH,
                        m_parameters->getUsePureBayerReprocessing());
                    newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_PURE_REPROCESSING_3AA, zoomParamInfo);
                    newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP, zoomParamInfo);
                } else {
                    int previewW = 0, previewH = 0;
                    m_parameters->getPreviewSize(&previewW, &previewH);
                    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
                    m_parameters->getPreviewBdsSize(&bdsSize);
                    updateNodeGroupInfoFront(
                        m_cameraId,
                        &node_group_info,
                        &node_group_info_isp,
                        bayerCropSize,
                        previewW, previewH);
                    newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_3AA, zoomParamInfo);
                    newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoomParamInfo);
                }
            }
#endif

            memset(&shot_ext->node_group, 0x0, sizeof(camera2_node_group));

            /* Per - 3AA */
            if (node_group_info.leader.request == 1) {
                setMetaNodeLeaderInputSize(shot_ext,
                    node_group_info.leader.input.cropRegion[0],
                    node_group_info.leader.input.cropRegion[1],
                    node_group_info.leader.input.cropRegion[2],
                    node_group_info.leader.input.cropRegion[3]);
                setMetaNodeLeaderOutputSize(shot_ext,
                    node_group_info.leader.output.cropRegion[0],
                    node_group_info.leader.output.cropRegion[1],
                    node_group_info.leader.output.cropRegion[2],
                    node_group_info.leader.output.cropRegion[3]);
                setMetaNodeLeaderRequest(shot_ext,
                    node_group_info.leader.request);
                setMetaNodeLeaderVideoID(shot_ext,
                    m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID);
            }

            /* Per - 0:3AC 1:3AP */
            for (int i = 0; i < m_perframeMainNodeGroupInfo.perframeSupportNodeNum - 1; i ++) {
                if (node_group_info.capture[i].request == 1) {
                    setMetaNodeCaptureInputSize(shot_ext, i,
                        node_group_info.capture[i].input.cropRegion[0],
                        node_group_info.capture[i].input.cropRegion[1],
                        node_group_info.capture[i].input.cropRegion[2],
                        node_group_info.capture[i].input.cropRegion[3]);
                    setMetaNodeCaptureOutputSize(shot_ext, i,
                        node_group_info.capture[i].output.cropRegion[0],
                        node_group_info.capture[i].output.cropRegion[1],
                        node_group_info.capture[i].output.cropRegion[2],
                        node_group_info.capture[i].output.cropRegion[3]);
                    setMetaNodeCaptureRequest(shot_ext, i, node_group_info.capture[i].request);
                    setMetaNodeCaptureVideoID(shot_ext, i, m_perframeMainNodeGroupInfo.perFrameCaptureInfo[i].perFrameVideoID);
                }
            }
            /* ALOGI("INFO(%s[%d]):fcount(%d)", __FUNCTION__, __LINE__, shot_ext_dst->shot.dm.request.frameCount); */
            /* newFrame->dumpNodeGroupInfo("3AA"); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeIspNodeGroupInfo", m_perframeIspNodeGroupInfo); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeMainNodeGroupInfo", m_perframeMainNodeGroupInfo); */
        }
    }

    if (m_subNode->putBuffer(&ispBuffer) != NO_ERROR) {
        ALOGE("ERR(%s):capture putBuffer fail ret(%d)", __FUNCTION__, ret);
        /* TODO: doing exception handling */
        return ret;
    }

    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
        return ret;
    }

    if (m_mainNode->putBuffer(&fliteBuffer) != NO_ERROR) {
        ALOGE("ERR(%s):output putBuffer fail ret(%d)", __FUNCTION__, ret);
        /* TODO: doing exception handling */
        return ret;
    }

    ret = newFrame->setSrcBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s): setSrcBuffer state fail", __FUNCTION__);
        return ret;
    }

    m_runningFrameList[fliteBuffer.index] = newFrame;
    m_numOfRunningFrame++;

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA::m_getBuffer(void)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraBuffer fliteBuffer;
    ExynosCameraBuffer ispBuffer;
    int index = 0;
    status_t ret = 0;
    int error = 0;

    ALOGV("DEBUG(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        ALOGD("DEBUG(%s[%d]): skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d", __FUNCTION__, __LINE__, m_flagStartPipe, m_numOfRunningFrame);
        return NO_ERROR;
    }

    ret = m_subNode->getBuffer(&ispBuffer, &index);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_subNode->getBuffer fail ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        error = ret;
    }

    ALOGV("DEBUG(%s[%d]):index : %d", __FUNCTION__, __LINE__, index);

    ret = m_mainNode->getBuffer(&fliteBuffer, &index);
    if (ret != NO_ERROR || error != NO_ERROR) {
        ALOGE("ERR(%s[%d]):m_mainNode->getBuffer fail ret(%d)", __FUNCTION__, __LINE__, ret);
        camera2_shot_ext *shot_ext;
        shot_ext = (struct camera2_shot_ext *)(fliteBuffer.addr[1]);
        fliteBuffer.index = index;
        CLOGE("ERR(%s[%d]):Shot done invalid, frame(cnt:%d, index(%d)) skip", __FUNCTION__, __LINE__, getMetaDmRequestFrameCount(shot_ext), index);

        /* complete frame */
        ret = m_completeFrame(&curFrame, fliteBuffer, false);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):complete frame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            return ret;
        }

        if (curFrame == NULL) {
            ALOGE("ERR(%s[%d]):curFrame is fail", __FUNCTION__, __LINE__);
            return ret;
        }

        /* Push to outputQ */
        if (m_outputFrameQ != NULL) {
            m_outputFrameQ->pushProcessQ(&curFrame);
        } else {
            ALOGE("ERR(%s[%d]):m_outputFrameQ is NULL", __FUNCTION__, __LINE__);
        }

        ALOGV("DEBUG(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

        return NO_ERROR;
    }

    ret = m_updateMetadataToFrame(fliteBuffer.addr[1], fliteBuffer.index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): updateMetadataToFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ALOGV("DEBUG(%s[%d]):index : %d", __FUNCTION__, __LINE__, index);

    nsecs_t timeStamp = (nsecs_t)getMetaDmSensorTimeStamp((struct camera2_shot_ext *)fliteBuffer.addr[1]);
    if (timeStamp < 0) {
        ALOGW("WRN(%s[%d]): frameCount(%d), Invalid timeStamp(%lld)",
           __FUNCTION__, __LINE__,
           getMetaDmRequestFrameCount((struct camera2_shot_ext *)fliteBuffer.addr[1]),
           timeStamp);
    }

    /* complete frame */
    ret = m_completeFrame(&curFrame, fliteBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):complete frame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return ret;
    }

    if (curFrame == NULL) {
        ALOGE("ERR(%s[%d]):curFrame is fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = curFrame->getSrcBuffer(getPipeId(), &fliteBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get src buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    ret = curFrame->getDstBuffer(getPipeId(), &ispBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get dst buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    memcpy(ispBuffer.addr[1], fliteBuffer.addr[1], sizeof(struct camera2_shot_ext));

    ALOGV("DEBUG(%s[%d]):isp frameCount %d", __FUNCTION__, __LINE__,
           getMetaDmRequestFrameCount((struct camera2_shot_ext *)ispBuffer.addr[1]));

    if (m_outputFrameQ != NULL)
        m_outputFrameQ->pushProcessQ(&curFrame);
    else
        ALOGE("ERR(%s[%d]):m_outputFrameQ is NULL", __FUNCTION__, __LINE__);

    ALOGV("DEBUG(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

bool ExynosCameraPipe3AA::m_mainThreadFunc(void)
{
    int ret = 0;

    ret = m_putBuffer();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;
        ALOGE("ERR(%s[%d]):m_putBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return m_checkThreadLoop();
    }

    ret = m_getBuffer();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return m_checkThreadLoop();
    }

    m_timer.stop();
    m_timeInterval = m_timer.durationMsecs();
    m_timer.start();

    /* update renew count */
    if (ret >= 0)
        m_threadRenew = 0;

    return m_checkThreadLoop();
}

}; /* namespace android */
