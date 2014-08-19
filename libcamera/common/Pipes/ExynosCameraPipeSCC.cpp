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
#define LOG_TAG "ExynosCameraPipeSCC"
#include <cutils/log.h>

#include "ExynosCameraPipeSCC.h"

namespace android {

ExynosCameraPipeSCC::ExynosCameraPipeSCC(
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

ExynosCameraPipeSCC::~ExynosCameraPipeSCC()
{
    this->destroy();
}

status_t ExynosCameraPipeSCC::create(int32_t *sensorIds)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("SCC");
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

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeSCC::m_mainThreadFunc, "SCCThread");
    if ( m_parameters->getUseDynamicScc() )
        m_inputFrameQ = new frame_queue_t(m_mainThread);
    else {
        m_inputFrameQ = new frame_queue_t;
        m_inputFrameQ->setWaitTime(500000000); /* .5 sec */
    }

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    CLOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeSCC::destroy(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    if (m_mainNode != NULL) {
        if (m_mainNode->close() != NO_ERROR) {
            CLOGE("ERR(%s):close fail", __FUNCTION__);
            return INVALID_OPERATION;
        }
        delete m_mainNode;
        m_mainNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) closed", __FUNCTION__, FIMC_IS_VIDEO_SCC_NUM);
    }

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeSCC::setupPipe(camera_pipe_info_t *pipeInfos)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    /* TODO: check node state */
    /*       stream on? */

    /* initialize node */
    int maxW = pipeInfos[0].rectInfo.fullW;
    int maxH = pipeInfos[0].rectInfo.fullH;
    int colorFormat = pipeInfos[0].rectInfo.colorFormat;
    enum v4l2_buf_type bufType = (enum v4l2_buf_type)pipeInfos[0].bufInfo.type;
    enum v4l2_memory memType = (enum v4l2_memory)pipeInfos[0].bufInfo.memory;
    m_numBuffers = pipeInfos[0].bufInfo.count;

    m_mainNode->setSize(maxW, maxH);
    m_mainNode->setColorFormat(colorFormat, 2);
    m_mainNode->setBufferType(m_numBuffers, bufType, memType);

    m_mainNode->setFormat();
    m_mainNode->reqBuffers();

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_requestFrameQ.release();

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    CLOGI("INFO(%s[%d]):setupPipe() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeSCC::prepare(void)
{
    CLOGV("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

bool ExynosCameraPipeSCC::m_checkThreadLoop(void)
{
    bool loop = false;

    if (m_inputFrameQ->getSizeOfProcessQ() > 0)
        loop = true;

    if (m_inputFrameQ->getSizeOfProcessQ() == 0 &&
        m_numOfRunningFrame == 0)
        loop = false;

    if (m_isReprocessing() == false)
        loop = true;

#if 0
	if (m_parameters->getUseDynamicScc() == false)
		loop = true;
#endif

    return loop;
}

bool ExynosCameraPipeSCC::m_mainThreadFunc(void)
{
    int ret = 0;

    if (m_flagStartPipe == false) {
        /* waiting for pipe started */
        usleep(5000);
        return m_checkThreadLoop();
    }

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    ret = m_putBuffer();
    if (ret < 0) {
        if (ret != TIMED_OUT) {
            CLOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
	        /* TODO: doing exception handling */
	        return m_checkThreadLoop();
        }
    }

    ret = m_getBuffer();
    if (ret < 0) {
        CLOGE("ERR(%s):m_getBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return m_checkThreadLoop();
    }

    return m_checkThreadLoop();
}

status_t ExynosCameraPipeSCC::m_putBuffer(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    int ret = 0;
    entity_buffer_type_t entityBufType = ENTITY_BUFFER_INVALID;

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

    ret = newFrame->getEntityBufferType(getPipeId(), &entityBufType);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return INVALID_OPERATION;
    }

    /* check buffer index */
    if (newBuffer.index < 0) {
        if (entityBufType != ENTITY_BUFFER_DELIVERY) {
            ALOGE("ERR(%s[%d]): Entity buffer type is ENTITY_BUFFER_FIXED, but index (%d)", __FUNCTION__, __LINE__, newBuffer.index);
            return BAD_VALUE;
        }
        CLOGV("DEBUG(%s[%d]): no buffer to QBUF", __FUNCTION__, __LINE__);
        ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED);
        if (ret < 0) {
            CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
            return ret;
        }
    } else {
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

        if (entityBufType == ENTITY_BUFFER_FIXED || m_reprocessing == true) {
            ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): setDstBuffer state fail", __FUNCTION__, __LINE__);
                return ret;
            }
        } else {
            newBuffer.index = -1;
            ret = newFrame->setDstBuffer(getPipeId(), newBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): setDstBuffer state fail", __FUNCTION__, __LINE__);
                return ret;
            }

            ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): setDstBuffer state fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }

        m_numOfRunningFrame++;
    }

    /* check request */
    if (entityBufType == ENTITY_BUFFER_FIXED || newFrame->getRequest(getPipeId()) == true) {
        m_requestFrameQ.pushProcessQ(&newFrame);
        ALOGV("DEBUG(%s[%d]): push reqeust Frame (frame cnt:%d, request cnt: %d)", __FUNCTION__, __LINE__, newFrame->getFrameCount(), m_requestFrameQ.getSizeOfProcessQ());
    } else {
        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):set entity state fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            return OK;
        }

        CLOGV("DEBUG(%s):entity pipeId(%d), frameCount(%d), numOfRunningFrame(%d), requestCount(%d)",
                __FUNCTION__, getPipeId(), newFrame->getFrameCount(), m_numOfRunningFrame, m_requestFrameQ.getSizeOfProcessQ());

        m_outputFrameQ->pushProcessQ(&newFrame);
    }

    if (m_numOfRunningFrame < 4 && getPipeId() < MAX_PIPE_NUM && m_parameters->getUseDynamicScc() == true) {
        CLOGW("DEBUG(%s):entity pipeId(%d), frameCount(%d),requestSCC(%d), numOfRunningFrame(%d), requestCount(%d)",
            __FUNCTION__, getPipeId(), newFrame->getFrameCount(), newFrame->getRequest(getPipeId()), m_numOfRunningFrame, m_requestFrameQ.getSizeOfProcessQ());
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeSCC::m_getBuffer(void)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = -1;
    int ret = 0;
    bool foundMatchedFrame = false;
    entity_buffer_type_t entityBufType = ENTITY_BUFFER_INVALID;

    int requestFrameSize = m_requestFrameQ.getSizeOfProcessQ();
    ALOGV("DEBUG(%s[%d]: request frame size(%d), numOfRunningFrame(%d)", __FUNCTION__, __LINE__, requestFrameSize, m_numOfRunningFrame);

    if (requestFrameSize == 0) {
        CLOGV("DEBUG(%s[%d]): no request", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        CLOGD("DEBUG(%s[%d]): skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d", __FUNCTION__, __LINE__, m_flagStartPipe, m_numOfRunningFrame);
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

    /* set runningFrameList for completeFrame */
    do {
        m_requestFrameQ.popProcessQ(&curFrame);
        if (curFrame == NULL) {
            CLOGE("ERR(%s[%d]): curFrame is NULL, size of requestFrameQ(%d)", __FUNCTION__, __LINE__, requestFrameSize);
            requestFrameSize = m_requestFrameQ.getSizeOfProcessQ();
        }

        ret = curFrame->getEntityBufferType(getPipeId(), &entityBufType);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):frame get buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            entityBufType = ENTITY_BUFFER_INVALID;
            continue;
        }

        if (curFrame->getRequest(getPipeId()) == false) {
            ret = curFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):set entity state fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            CLOGE("ERR(%s[%d]):frame drop pipeId(%d), frameCount(%d), numOfRunningFrame(%d), requestCount(%d)",
                    __FUNCTION__, __LINE__, getPipeId(), curFrame->getFrameCount(), m_numOfRunningFrame, requestFrameSize);

            curFrame->setRequest(getPipeId(), true);
            if (entityBufType == ENTITY_BUFFER_DELIVERY) {
                m_outputFrameQ->pushProcessQ(&curFrame);
                requestFrameSize = m_requestFrameQ.getSizeOfProcessQ();
                continue;
            }
        }

        if (m_runningFrameList[curBuffer.index] != NULL) {
            CLOGE("ERR(%s):new buffer is invalid, we already get buffer index(%d), curFrame->frameCount(%d)",
                    __FUNCTION__, index, curFrame->getFrameCount());
            m_dumpRunningFrameList();
            return BAD_VALUE;
        }
        m_runningFrameList[curBuffer.index] = curFrame;

        /* If we found match frame, quit loop */
        foundMatchedFrame = true;
        break;
    } while (requestFrameSize > 0 && entityBufType == ENTITY_BUFFER_DELIVERY);

    if (foundMatchedFrame == false || m_runningFrameList[curBuffer.index] == NULL) {
        CLOGE("ERR(%s):buffer is invalid, curFrame->frameCount(%d)",
                __FUNCTION__, curFrame->getFrameCount());
        m_dumpRunningFrameList();
        return BAD_VALUE;
    }

    /* complete frame */
    ret = m_completeFrame(&curFrame, curBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s):m_comleteFrame fail", __FUNCTION__);
        /* TODO: doing exception handling */
    }

    if (curFrame == NULL) {
        CLOGE("ERR(%s):curFrame is fail", __FUNCTION__);
        return BAD_VALUE;
    }

    if (entityBufType == ENTITY_BUFFER_DELIVERY) {
        /* set frame for dynamic capture */
        ret = curFrame->setDstBuffer(getPipeId(), curBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s): setDstBuffer fail", __FUNCTION__);
            return ret;
        }
    }

    ret = curFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
        return ret;
    }

    entity_buffer_state_t tempState;
    curFrame->getDstBufferState(getPipeId(), &tempState);

    if (getPipeId() < MAX_PIPE_NUM && m_parameters->getUseDynamicScc() == true) {
        CLOGD("DEBUG(%s[%d]): SCC pipe output done, curFrameCount(%d) - index(%d), bufState(%d)",
            __FUNCTION__, __LINE__, curFrame->getFrameCount(), curBuffer.index, tempState);
    }

    m_outputFrameQ->pushProcessQ(&curFrame);

    return NO_ERROR;
}


}; /* namespace android */
