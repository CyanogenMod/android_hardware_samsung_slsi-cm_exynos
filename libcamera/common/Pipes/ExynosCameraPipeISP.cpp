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
#define LOG_TAG "ExynosCameraPipeISP"
#include <cutils/log.h>

#include "ExynosCameraPipeISP.h"

namespace android {

ExynosCameraPipeISP::ExynosCameraPipeISP(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_sensorId = 0;
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = isReprocessing ? 1 : 0;

    m_mainNodeNum = nodeNums[OUTPUT_NODE];

    m_activityControl = m_parameters->getActivityControl();
    m_exynosconfig = m_parameters->getConfig();
}

ExynosCameraPipeISP::~ExynosCameraPipeISP()
{
    this->destroy();
}

status_t ExynosCameraPipeISP::create(int32_t *sensorIds)
{
    ALOGI("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("ISP");
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    ret = m_mainNode->open(m_mainNodeNum);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    ALOGD("DEBUG(%s):Node(%d) opened", __FUNCTION__, FIMC_IS_VIDEO_ISP_NUM);

    /* HACK: ISP setInput have to be called after 3AA open
     * ret = m_mainNode->setInput(sensorIds[OUTPUT_NODE]);
     * if (ret < 0) {
     *     ALOGE("ERR(%s[%d]): mainNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[OUTPUT_NODE], ret);
     *     return ret;
     * }
     */
    m_sensorId = sensorIds[OUTPUT_NODE];

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeISP::m_mainThreadFunc, "ISPThread", PRIORITY_URGENT_DISPLAY);

    if (m_isReprocessing() == false)
        m_inputFrameQ = new frame_queue_t;
    else
        m_inputFrameQ = new frame_queue_t(m_mainThread);

    m_inputFrameQ->setWaitTime(500000000); /* .5 sec */

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    CLOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);
    return NO_ERROR;
}

status_t ExynosCameraPipeISP::destroy(void)
{
    int ret = 0;

    if (m_mainNode != NULL) {
        ret = m_mainNode->close();
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):main node close fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
        delete m_mainNode;
        m_mainNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) closed", __FUNCTION__, FIMC_IS_VIDEO_ISP_NUM);
    }

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeISP::setupPipe(camera_pipe_info_t *pipeInfos)
{
    ALOGI("DEBUG(%s[%d]): -IN-", __FUNCTION__, __LINE__);
#ifdef DEBUG_RAWDUMP
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
#endif
    /* TODO: check node state */
    /*       stream on? */

    int ret = 0;

    /* HACK: ISP setInput have to be called after 3AA open */
    ret = m_mainNode->setInput(m_sensorId);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, m_sensorId, ret);
        return ret;
    }

    /* initialize node */
    int maxW = pipeInfos[0].rectInfo.fullW;
    int maxH = pipeInfos[0].rectInfo.fullH;
    int colorFormat = pipeInfos[0].rectInfo.colorFormat;
    enum v4l2_buf_type bufType = (enum v4l2_buf_type)pipeInfos[0].bufInfo.type;
    enum v4l2_memory memType = (enum v4l2_memory)pipeInfos[0].bufInfo.memory;
    m_numBuffers = pipeInfos[0].bufInfo.count;

    m_perframeMainNodeGroupInfo = pipeInfos[0].perFrameNodeGroupInfo;

    m_mainNode->setSize(maxW, maxH);
    m_mainNode->setColorFormat(colorFormat, 2);
    m_mainNode->setBufferType(m_numBuffers, bufType, memType);

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = (maxW) * 2;
        ret = m_mainNode->setFormat();
    } else
#endif /* DEBUG_RAWDUMP */
    {
        ret = m_mainNode->setFormat(pipeInfos[0].bytesPerPlane);
    }
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): isp node set format fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    ret = m_mainNode->reqBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): isp node req buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    /* setfile setting */
#ifdef SET_SETFILE_BY_SHOT
    /* nop */
#else
#if SET_SETFILE_BY_SET_CTRL_ISP
    int setfile = 0;
    int yuvRange = 0;
    m_parameters->getSetfileYuvRange(m_reprocessing, &setfile, &yuvRange);

    ret = m_mainNode->setControl(V4L2_CID_IS_SET_SETFILE, setfile);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):setControl(%d) fail(ret = %d", __FUNCTION__, __LINE__, setfile, ret);
        return ret;
    }
#endif
#endif

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    CLOGI("INFO(%s[%d]):setupPipe() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    ALOGI("DEBUG(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipeISP::prepare(void)
{
    ALOGI("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipeISP::m_putBuffer(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    camera2_node_group node_group_info;
    int ret = 0;

    ALOGV("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
            m_mainNode->dumpState();
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        ALOGE("ERR(%s[%d]):new frame is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    ret = newFrame->getSrcBuffer(getPipeId(), &newBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get src buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_runningFrameList[newBuffer.index] != NULL) {
        ALOGE("ERR(%s[%d]):new buffer is invalid, we already get buffer index(%d)",
            __FUNCTION__, __LINE__, newBuffer.index);
        return BAD_VALUE;
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(newBuffer.addr[1]);

    if (shot_ext != NULL) {
        newFrame->getMetaData(shot_ext);
        ret = m_parameters->duplicateCtrlMetadata((void *)shot_ext);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):duplicate Ctrl metadata fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
        m_parameters->getFdMeta(m_isReprocessing(), (void *)shot_ext);
        m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&newBuffer);

        if (m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
            int zoomParamInfo = m_parameters->getZoomLevel();
            int zoomFrameInfo = 0;
            int previewW = 0, previewH = 0;
            ExynosRect sensorSize;
            ExynosRect bayerCropSize;
            ExynosRect bdsSize;

            newFrame->getNodeGroupInfo(&node_group_info, m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex, &zoomFrameInfo);
#ifdef PERFRAME_CONTROL_NODE_ISP
            if (zoomFrameInfo != zoomParamInfo) {
                ALOGI("INFO(%s[%d]):zoomFrameInfo(%d), zoomParamInfo(%d)",
                    __FUNCTION__, __LINE__, zoomFrameInfo, zoomParamInfo);

                m_parameters->getPreviewSize(&previewW, &previewH);
                m_parameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);
                m_parameters->getPreviewBdsSize(&bdsSize);
                if (m_cameraId == CAMERA_ID_BACK) {
                    updateNodeGroupInfoMainPreview(
                        m_cameraId,
                        &node_group_info,
                        bayerCropSize,
                        bdsSize,
                        previewW, previewH);
                } else {
                    updateNodeGroupInfoFront(
                        m_cameraId,
                        &node_group_info,
                        bayerCropSize,
                        previewW, previewH);
                }

                newFrame->storeNodeGroupInfo(&node_group_info, m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex, zoomParamInfo);
            }
#endif

            memset(&shot_ext->node_group, 0x0, sizeof(camera2_node_group));

            /* Per - ISP */
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

            /* Per - SCP, SCC */
            for (int i = 0; i < m_perframeMainNodeGroupInfo.perframeSupportNodeNum - 1; i ++) {
                /* HACK: 5260 driver has bug. Will be fixed */
                /*if (node_group_info.capture[i].request == 1) {*/
                    /* W */
                    if (node_group_info.capture[i].output.cropRegion[2] > node_group_info.capture[i].input.cropRegion[2] * 4) {
                        node_group_info.capture[i].output.cropRegion[2] = node_group_info.capture[i].input.cropRegion[2] * 4;
                    }
                    /* H */
                    if (node_group_info.capture[i].output.cropRegion[3] > node_group_info.capture[i].input.cropRegion[3] * 4) {
                        node_group_info.capture[i].output.cropRegion[3] = node_group_info.capture[i].input.cropRegion[3] * 4;
                    }

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
                    setMetaNodeCaptureRequest(shot_ext,  i, node_group_info.capture[i].request);
                    setMetaNodeCaptureVideoID(shot_ext, i, m_perframeMainNodeGroupInfo.perFrameCaptureInfo[i].perFrameVideoID);
                /*}*/
            }
            /* ALOGI("INFO(%s[%d]):fcount(%d)", __FUNCTION__, __LINE__, shot_ext_dst->shot.dm.request.frameCount); */
            /* newFrame->dumpNodeGroupInfo("ISP"); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeIspNodeGroupInfo", m_perframeIspNodeGroupInfo); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeMainNodeGroupInfo", m_perframeMainNodeGroupInfo); */
        }
    }

    ret = m_mainNode->putBuffer(&newBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):putBuffer fail ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
    }

    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
        return ret;
    }

    m_runningFrameList[newBuffer.index] = newFrame;
    m_numOfRunningFrame++;

    return NO_ERROR;
}

status_t ExynosCameraPipeISP::m_getBuffer(void)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = 0;
    int ret = 0;

    ALOGV("DEBUG(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        ALOGD("DEBUG(%s[%d]): skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d", __FUNCTION__, __LINE__, m_flagStartPipe, m_numOfRunningFrame);
        return NO_ERROR;
    }

    ret = m_mainNode->getBuffer(&curBuffer, &index);
    if (ret != NO_ERROR || index < 0) {
        ALOGE("ERR(%s[%d]):getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        camera2_shot_ext *shot_ext;
        shot_ext = (struct camera2_shot_ext *)(curBuffer.addr[1]);
        curBuffer.index = index;
        ALOGE("ERR(%s[%d]):Shot done invalid, frame(cnt:%d, index(%d)) skip", __FUNCTION__, __LINE__, getMetaDmRequestFrameCount(shot_ext), index);

        /* complete frame */
        ret = m_completeFrame(&curFrame, curBuffer, false);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):complete frame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            return ret;
        }

        if (curFrame == NULL) {
            ALOGE("ERR(%s[%d]):curFrame is fail", __FUNCTION__, __LINE__);
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

    ret = m_updateMetadataToFrame(curBuffer.addr[1], curBuffer.index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): updateMetadataToFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    m_activityControl->activityAfterExecFunc(getPipeId(), (void *)&curBuffer);

    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    /* complete frame */
    ret = m_completeFrame(&curFrame, curBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):complete frame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return ret;
    }

    if (curFrame == NULL) {
        ALOGE("ERR(%s[%d]):curFrame is fail", __FUNCTION__, __LINE__);
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(curBuffer.addr[1]);
    if (curFrame->getRequest(PIPE_SCC) == true && shot_ext->node_group.capture[0].request == 0) {
        ALOGE("ERR(%s[%d]: @@@@@@@@@@@@@@@@@@@@@@@ SCC Frame drop, halFrameCount(%d) metaFrameCount(%d)",
                __FUNCTION__, __LINE__, curFrame->getFrameCount(), getMetaDmRequestFrameCount(shot_ext));
        curFrame->setRequest(PIPE_SCC, false);
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

bool ExynosCameraPipeISP::m_mainThreadFunc(void)
{
    int ret = 0;

    m_configDvfs();

    ret = m_putBuffer();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]:m_putBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return m_checkThreadLoop();
    }

    ret = m_getBuffer();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]:m_getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return m_checkThreadLoop();
    }

    return m_checkThreadLoop();
}

}; /* namespace android */
