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
#define LOG_TAG "ExynosCameraPipeSCP"
#include <cutils/log.h>

#include "ExynosCameraPipeSCP.h"

namespace android {

#ifdef TEST_WATCHDOG_THREAD
int testErrorDetect = 0;
#endif

ExynosCameraPipeSCP::ExynosCameraPipeSCP(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = isReprocessing ? 1 : 0;
    m_mainNodeNum = nodeNums[CAPTURE_NODE];

    m_metadataTypeShot = false;

    m_activityControl = m_parameters->getActivityControl();
    m_exynosconfig = m_parameters->getConfig();
}

ExynosCameraPipeSCP::~ExynosCameraPipeSCP()
{
    this->destroy();
#ifdef TEST_WATCHDOG_THREAD
    testErrorDetect = 0;
#endif
}

status_t ExynosCameraPipeSCP::create(int32_t *sensorIds)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("SCP");
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

    ret = m_mainNode->setInput(sensorIds[CAPTURE_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[CAPTURE_NODE], ret);
        return ret;
    }

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeSCP::m_mainThreadFunc, "SCPThread", PRIORITY_URGENT_DISPLAY);

    m_inputFrameQ = new frame_queue_t;


    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):create() is succeed (%d) setupPipe (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeSCP::destroy(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    if (m_mainNode != NULL) {
        if (m_mainNode->close() != NO_ERROR) {
            CLOGE("ERR(%s):close fail", __FUNCTION__);
            return INVALID_OPERATION;
        }
        delete m_mainNode;
        m_mainNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) closed", __FUNCTION__, FIMC_IS_VIDEO_SCP_NUM);
    }

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeSCP::setupPipe(camera_pipe_info_t *pipeInfos)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    status_t ret = NO_ERROR;

    /* TODO: check node state stream on? */

    /* initialize node */
    int maxW = pipeInfos[0].rectInfo.fullW;
    int maxH = pipeInfos[0].rectInfo.fullH;
    int colorFormat = pipeInfos[0].rectInfo.colorFormat;
    enum v4l2_buf_type bufType = (enum v4l2_buf_type)pipeInfos[0].bufInfo.type;
    enum v4l2_memory memType = (enum v4l2_memory)pipeInfos[0].bufInfo.memory;
    m_numBuffers = pipeInfos[0].bufInfo.count;

    m_mainNode->setSize(maxW, maxH);
    m_mainNode->setColorFormat(colorFormat, 4);
    m_mainNode->setBufferType(m_numBuffers, bufType, memType);

    m_mainNode->setFormat();
    m_mainNode->reqBuffers();

    /* setfile setting */
#ifdef SET_SETFILE_BY_SHOT
    /* nop */
#else
#if SET_SETFILE_BY_SET_CTRL_SCP
    int setfile = 0;
    int yuvRange = 0;
    m_parameters->getSetfileYuvRange(m_reprocessing, &setfile, &yuvRange);
    ret = m_mainNode->setControl(V4L2_CID_IS_COLOR_RANGE, yuvRange);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setControl(%d) fail(ret = %d)", __FUNCTION__, __LINE__, setfile, ret);
        return ret;
    }
#endif
#endif

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):setupPipe() is succeed (%d) setupPipe (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeSCP::m_checkPolling(void)
{
    int ret = 0;

    ret = m_mainNode->polling();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):polling fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */

        m_threadState = ERROR_POLLING_DETECTED;
        return ERROR_POLLING_DETECTED;
    }

    return NO_ERROR;
}

bool ExynosCameraPipeSCP::m_mainThreadFunc(void)
{
    int ret = 0;

#ifdef TEST_WATCHDOG_THREAD
    testErrorDetect++;
    if (testErrorDetect == 100)
        m_threadState = ERROR_POLLING_DETECTED;
#endif

    if  (m_flagTryStop == true)
        return true;

    if (m_numOfRunningFrame > 0) {
#ifndef SKIP_SCHECK_POLLING
        ret = m_checkPolling();
#endif

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_checkPolling fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            // HACK: for panorama shot
            //return false;
        }

        ret = m_getBuffer();
        if (ret < 0) {
            CLOGE("ERR(%s):m_getBuffer fail", __FUNCTION__);
            /* TODO: doing exception handling */
            return true;
        }
    }

    m_timer.stop();
    m_timeInterval = m_timer.durationMsecs();
    m_timer.start();

    ret = m_putBuffer();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;
        CLOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return true;
    }

    /* update renew count */
    if (ret >= 0)
        m_threadRenew = 0;

    return true;
}

status_t ExynosCameraPipeSCP::m_putBuffer(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    int ret = 0;

retry:
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
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return INVALID_OPERATION;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &newBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    /* check buffer index */
    if (newBuffer.index < 0) {
        CLOGD("DEBUG(%s[%d]): no buffer to QBUF (%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

        ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED);
        if (ret < 0) {
            CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
            return ret;
        }

        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):set entity state fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            return OK;
        }

        CLOGV("DEBUG(%s):entity pipeId(%d), frameCount(%d), numOfRunningFrame(%d), requestCount(%d)",
                __FUNCTION__, getPipeId(), newFrame->getFrameCount(), m_numOfRunningFrame, m_requestCount);

        usleep(33000);
        m_outputFrameQ->pushProcessQ(&newFrame);

        goto retry;
    } else {
        if (m_runningFrameList[newBuffer.index] != NULL) {
            CLOGE("ERR(%s):new buffer is invalid, we already get buffer index(%d), newFrame->frameCount(%d)",
                __FUNCTION__, newBuffer.index, newFrame->getFrameCount());
            m_dumpRunningFrameList();

            return BAD_VALUE;
        }

        camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(newBuffer.addr[1]);

        if (shot_ext != NULL) {
            newFrame->getMetaData(shot_ext);
            m_parameters->duplicateCtrlMetadata((void *)shot_ext);
            m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&newBuffer);

            if (m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
                camera2_node_group node_group_info;
                memset(&shot_ext->node_group, 0x0, sizeof(camera2_node_group));
                newFrame->getNodeGroupInfo(&node_group_info, m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex);

                /* Per - Leader */
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

                /* Per - Captures */
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
                        setMetaNodeCaptureRequest(shot_ext,  i, node_group_info.capture[i].request);
                        setMetaNodeCaptureVideoID(shot_ext, i, m_perframeMainNodeGroupInfo.perFrameCaptureInfo[i].perFrameVideoID);
                    }
                }
            }
        }

        ret = m_mainNode->putBuffer(&newBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s):putBuffer fail", __FUNCTION__);
            return ret;
            /* TODO: doing exception handling */
        }

        ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
        if (ret < 0) {
            CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
            return ret;
        }

        m_runningFrameList[newBuffer.index] = newFrame;
        m_numOfRunningFrame++;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeSCP::m_getBuffer(void)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = -1;
    int ret = 0;

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        ALOGD("DEBUG(%s[%d]): skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d", __FUNCTION__, __LINE__, m_flagStartPipe, m_numOfRunningFrame);
        return NO_ERROR;
    }

    ret = m_mainNode->getBuffer(&curBuffer, &index);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBuffer fail", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return ret;
    }

    if (index < 0) {
        CLOGE("ERR(%s[%d]):Invalid index(%d) fail", __FUNCTION__, __LINE__, index);
        return INVALID_OPERATION;
    }

    m_activityControl->activityAfterExecFunc(getPipeId(), (void *)&curBuffer);

    /* complete frame */
    ret = m_completeFrame(&curFrame, curBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s):m_comleteFrame fail", __FUNCTION__);
        /* TODO: doing exception handling */
    }

    if (curFrame == NULL) {
        CLOGE("ERR(%s):curFrame is fail", __FUNCTION__);
    }

    ret = curFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
        return ret;
    }

    m_outputFrameQ->pushProcessQ(&curFrame);

    return NO_ERROR;
}

}; /* namespace android */
