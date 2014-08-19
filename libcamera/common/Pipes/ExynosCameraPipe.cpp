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
#define LOG_TAG "ExynosCameraPipe"
#include <cutils/log.h>

#include "ExynosCameraPipe.h"

namespace android {

ExynosCameraPipe::ExynosCameraPipe()
{
    m_pipeId = 0;
    m_cameraId = -1;
    m_reprocessing = 0;
    m_parameters = NULL;
    m_prepareBufferCount = 0;
    m_numBuffers = 0;
    m_activityControl = NULL;

    m_inputFrameQ = NULL;
    m_outputFrameQ = NULL;

    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_mainNode = NULL;
    m_mainNodeNum = -1;

    m_threadCommand = 0;
    m_timeInterval = 0;
    m_threadState = 0;
    m_threadRenew = 0;
    memset(m_name, 0x00, sizeof(m_name));

    m_exynosconfig = NULL;

    m_flagStartPipe = false;
    m_flagTryStop = false;

    m_metadataTypeShot = true;

    m_dvfsLocked = false;

    memset(&m_perframeMainNodeGroupInfo, 0x00, sizeof(camera_pipe_perframe_node_group_info_t));

}

ExynosCameraPipe::~ExynosCameraPipe()
{
}

status_t ExynosCameraPipe::create(int32_t *sensorIds)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("main");
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    ret = m_mainNode->open(FIMC_IS_VIDEO_SS0_NUM);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    ALOGD("DEBUG(%s):Node(%d) opened", __FUNCTION__, FIMC_IS_VIDEO_SS0_NUM);

    ret = m_mainNode->setInput(sensorIds[OUTPUT_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[0], ret);
        return ret;
    }

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe::m_mainThreadFunc, "mainThread");

    m_inputFrameQ = new frame_queue_t;

    m_prepareBufferCount = 0;
    CLOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe::destroy(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    if (m_mainNode != NULL) {
        if (m_mainNode->close() != NO_ERROR) {
            CLOGE("ERR(%s): close fail", __FUNCTION__);
            return INVALID_OPERATION;
        }
        delete m_mainNode;
        m_mainNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) closed", __FUNCTION__, FIMC_IS_VIDEO_SS0_NUM);
    }

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    CLOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipe::setupPipe(camera_pipe_info_t *pipeInfos)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    /* TODO: check node state */

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

    m_mainNode->setFormat();
    m_mainNode->reqBuffers();

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_prepareBufferCount = 0;
    CLOGI("INFO(%s[%d]):setupPipe() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);
    return NO_ERROR;
}

status_t ExynosCameraPipe::prepare(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    for (uint32_t i = 0; i < m_prepareBufferCount; i++) {
        ret = m_putBuffer();
        if (ret < 0) {
            CLOGE("ERR(%s): m_putBuffer fail, ret(%d)", __FUNCTION__, ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::start(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    /* TODO: check state ready for start */

    int ret = 0;

    ret = m_mainNode->start();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s): Starting Node Error!", __FUNCTION__);
        return ret;
    }

    m_threadState = 0;
    m_threadRenew = 0;
    m_threadCommand = 0;
    m_timeInterval = 0;

    m_flagStartPipe = true;
    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipe::stop(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_flagStartPipe = false;

    ret = m_mainNode->stop();
    if (ret < 0) {
        CLOGE("ERR(%s): node stop fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    m_mainThread->requestExitAndWait();

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        CLOGE("ERR(%s): node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    CLOGD("DEBUG(%s[%d]): thead exited", __FUNCTION__, __LINE__);

    m_inputFrameQ->release();

    m_mainNode->removeItemBufferQ();

    for (uint32_t i = 0; i < m_numBuffers; i++)
        m_runningFrameList[i] = NULL;

    m_numOfRunningFrame = 0;

    m_threadState = 0;
    m_threadRenew = 0;
    m_threadCommand = 0;
    m_timeInterval = 0;
    m_flagTryStop= false;

    return NO_ERROR;
}

status_t ExynosCameraPipe::startThread(void)
{
    if (m_outputFrameQ == NULL) {
        CLOGE("ERR(%s): outputFrameQ is NULL, cannot start", __FUNCTION__);
        return INVALID_OPERATION;
    }

    m_timer.start();
    m_mainThread->run();

    CLOGI("INFO(%s[%d]):startThread is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipe::stopThread(void)
{
    m_mainThread->requestExit();
    m_inputFrameQ->sendCmd(WAKE_UP);

    m_dumpRunningFrameList();

    return NO_ERROR;
}

status_t ExynosCameraPipe::sensorStream(bool on)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    int value = on ? IS_ENABLE_STREAM: IS_DISABLE_STREAM;

    ret = m_mainNode->setControl(V4L2_CID_IS_S_STREAM, value);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s):m_mainNode->sensorStream failed", __FUNCTION__);

    return ret;
}

status_t ExynosCameraPipe::setControl(int cid, int value)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ret = m_mainNode->setControl(cid, value);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s):m_mainNode->setControl failed", __FUNCTION__);

    return ret;
}

status_t ExynosCameraPipe::getControl(int cid, int *value)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ret = m_mainNode->getControl(cid, value);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s):m_mainNode->getControl failed", __FUNCTION__);

    return ret;
}

status_t ExynosCameraPipe::setParam(struct v4l2_streamparm streamParam)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ret = m_mainNode->setParam(&streamParam);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s):m_mainNode->setControl failed", __FUNCTION__);

    return ret;
}

status_t ExynosCameraPipe::setStopFlag(void)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    m_flagTryStop = true;

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOn(int32_t numFrames)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;

    ret = m_mainNode->start();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): mainNode instantOn fail", __FUNCTION__, __LINE__);
        return ret;
    }

    return ret;
}

status_t ExynosCameraPipe::instantOnQbuf(ExynosCameraFrame **frame, BUFFER_POS::POS pos)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    int ret = 0;
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

    if(pos == BUFFER_POS::DST)
        ret = newFrame->getDstBuffer(getPipeId(), &newBuffer);
    else if(pos == BUFFER_POS::SRC)
        ret = newFrame->getSrcBuffer(getPipeId(), &newBuffer);

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_runningFrameList[newBuffer.index] != NULL) {
        CLOGE("ERR(%s):new buffer is invalid, we already get buffer index(%d), newFrame->frameCount(%d)",
            __FUNCTION__, newBuffer.index, newFrame->getFrameCount());
        return BAD_VALUE;
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(newBuffer.addr[1]);

    if (shot_ext != NULL) {
        newFrame->getMetaData(shot_ext);
        m_parameters->duplicateCtrlMetadata((void *)shot_ext);
        m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&newBuffer);

        /* set metadata for instant on */
        shot_ext->shot.ctl.scaler.cropRegion[0] = 0;
        shot_ext->shot.ctl.scaler.cropRegion[1] = 0;
        shot_ext->shot.ctl.scaler.cropRegion[2] = FASTEN_AE_WIDTH;
        shot_ext->shot.ctl.scaler.cropRegion[3] = FASTEN_AE_HEIGHT;

        setMetaCtlAeTargetFpsRange(shot_ext, FASTEN_AE_FPS, FASTEN_AE_FPS);
        setMetaCtlSensorFrameDuration(shot_ext, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)FASTEN_AE_FPS));

        shot_ext->shot.ctl.aa.afMode = AA_AFMODE_INFINITY;

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

    *frame = newFrame;

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOnDQbuf(ExynosCameraFrame **frame, BUFFER_POS::POS pos)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = -1;
    int ret = 0;

    if (m_numOfRunningFrame <= 0 ) {
        ALOGD("DEBUG(%s[%d]): skip getBuffer, numOfRunningFrame = %d", __FUNCTION__, __LINE__, m_numOfRunningFrame);
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

    ret = m_updateMetadataToFrame(curBuffer.addr[1], curBuffer.index);
    if (ret < 0)
        ALOGE("ERR(%s[%d]): updateMetadataToFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);


    if (curBuffer.index < 0) {
        CLOGE("ERR(%s):index(%d) is invalid", __FUNCTION__, curBuffer.index);
        return BAD_VALUE;
    }

    curFrame = m_runningFrameList[curBuffer.index];

    if (curFrame == NULL) {
        CLOGE("ERR(%s):Unknown buffer, frame is NULL", __FUNCTION__);
        dump();
        return BAD_VALUE;
    }

    *frame = curFrame;

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOff(void)
{
    int ret = 0;

    ret = m_mainNode->stop();

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA output node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    for( int i = 0 ; i < MAX_BUFFERS ; i++ ) {
        m_runningFrameList[i] = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOnPushFrameQ(BUFFERQ_TYPE::TYPE type, ExynosCameraFrame **frame)
{
    if( type == BUFFERQ_TYPE::OUTPUT )
        m_outputFrameQ->pushProcessQ(frame);
    else
        m_inputFrameQ->pushProcessQ(frame);

    return NO_ERROR;
}

status_t ExynosCameraPipe::pushFrame(ExynosCameraFrame **newFrame)
{
    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return BAD_VALUE;
    }

    m_inputFrameQ->pushProcessQ(newFrame);

    return NO_ERROR;
}

int ExynosCameraPipe::getCameraId(void)
{
    return this->m_cameraId;
}

uint32_t ExynosCameraPipe::getPipeId(void)
{
    return this->m_pipeId;
}

status_t ExynosCameraPipe::setPipeId(uint32_t id)
{
    this->m_pipeId = id;

    return NO_ERROR;
}

status_t ExynosCameraPipe::setPipeName(const char *pipeName)
{
    strncpy(m_name,  pipeName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    return NO_ERROR;
}

status_t ExynosCameraPipe::getInputFrameQ(frame_queue_t **inputFrameQ)
{
    *inputFrameQ = m_inputFrameQ;

    if (*inputFrameQ == NULL)
        CLOGE("ERR(%s[%d])inputFrameQ is NULL", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipe::getOutputFrameQ(frame_queue_t **outputFrameQ)
{
    *outputFrameQ = m_outputFrameQ;

    if (*outputFrameQ == NULL)
        CLOGE("ERR(%s[%d])outputFrameQ is NULL", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipe::setOutputFrameQ(frame_queue_t *outputFrameQ)
{
    m_outputFrameQ = outputFrameQ;
    return NO_ERROR;
}

status_t ExynosCameraPipe::getPipeInfo(int *fullW, int *fullH, int *colorFormat, int pipePosition)
{
    int planeCount = 0;
    int ret = NO_ERROR;

    ret = m_mainNode->getSize(fullW, fullH);
    if (ret < 0) {
        CLOGE("ERR(%s):getSize fail", __FUNCTION__);
        return ret;
    }

    ret = m_mainNode->getColorFormat(colorFormat, &planeCount);
    if (ret < 0) {
        CLOGE("ERR(%s):getColorFormat fail", __FUNCTION__);
        return ret;
    }

    return ret;
}

status_t ExynosCameraPipe::m_putBuffer(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    int ret = 0;

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

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_getBuffer(void)
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

    ret = m_updateMetadataToFrame(curBuffer.addr[1], curBuffer.index);
    if (ret < 0)
        ALOGE("ERR(%s[%d]): updateMetadataToFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);

    /* complete frame */
    ret = m_completeFrame(&curFrame, curBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s):m_comleteFrame fail", __FUNCTION__);
        /* TODO: doing exception handling */
    }

    if (curFrame == NULL) {
        CLOGE("ERR(%s):curFrame is fail", __FUNCTION__);
    }

    m_outputFrameQ->pushProcessQ(&curFrame);

    return NO_ERROR;
}

bool ExynosCameraPipe::m_mainThreadFunc(void)
{
    int ret = 0;

    /* TODO: check exit condition */
    /*       running list != empty */

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    ret = m_getBuffer();
    if (ret < 0) {
        CLOGE("ERR(%s):m_getBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return false;
    }

    ret = m_putBuffer();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;
        CLOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return false;
    }

    return true;
}

status_t ExynosCameraPipe::m_completeFrame(
        ExynosCameraFrame **frame,
        ExynosCameraBuffer buffer,
        bool isValid)
{
    int ret = 0;

    if (buffer.index < 0) {
        CLOGE("ERR(%s):index(%d) is invalid", __FUNCTION__, buffer.index);
        return BAD_VALUE;
    }

    *frame = m_runningFrameList[buffer.index];

    if (*frame == NULL) {
        CLOGE("ERR(%s):Unknown buffer, frame is NULL", __FUNCTION__);
        dump();
        return BAD_VALUE;
    }

    if (isValid == false) {
        ALOGD("DEBUG(%s[%d]):NOT DONE frameCount %d, buffer index(%d)", __FUNCTION__, __LINE__,
                (*frame)->getFrameCount(), buffer.index);
    }

    ret = (*frame)->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):set entity state fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    CLOGV("DEBUG(%s):entity pipeId(%d), buffer index(%d), frameCount(%d)",
            __FUNCTION__, buffer.index, getPipeId(),
            m_runningFrameList[buffer.index]->getFrameCount());

    m_runningFrameList[buffer.index] = NULL;
    m_numOfRunningFrame--;

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_getFrameByIndex(ExynosCameraFrame **frame, int index)
{
    *frame = m_runningFrameList[index];
    if (*frame == NULL) {
        CLOGE("ERR(%s[%d]):Unknown buffer, index %d frame is NULL", __FUNCTION__, __LINE__, index);
        dump();
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_updateMetadataToFrame(void *metadata, int index)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)metadata;
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]): metabuffer is null", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
    if (index < 0) {
        ALOGE("ERR(%s[%d]): Invalid index(%d)", __FUNCTION__, __LINE__, index);
        return BAD_VALUE;
    }

    if (m_metadataTypeShot == false) {
        ALOGV("DEBUG(%s[%d]): stream type do not need update metadata", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    ret = m_getFrameByIndex(&curFrame, index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): m_getFrameByIndex fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, index, ret);
        return ret;
    }

    ret = curFrame->storeDynamicMeta(shot_ext);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    ret = curFrame->storeUserDynamicMeta(shot_ext);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::getThreadState(int **threadState)
{
    *threadState = &m_threadState;

    return NO_ERROR;
}

status_t ExynosCameraPipe::getThreadInterval(uint64_t **timeInterval)
{
    *timeInterval = &m_timeInterval;

    return NO_ERROR;
}

status_t ExynosCameraPipe::getThreadRenew(int **timeRenew)
{
    *timeRenew = &m_threadRenew;

    return NO_ERROR;
}

status_t ExynosCameraPipe::incThreadRenew()
{
    m_threadRenew ++;

    return NO_ERROR;
}

void ExynosCameraPipe::dump()
{
    ALOGI("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    m_dumpRunningFrameList();

    if (m_mainNode != NULL)
        m_mainNode->dump();

    return;
}

bool ExynosCameraPipe::m_isReprocessing(void)
{
    return m_reprocessing == 1 ? true : false;
}

bool ExynosCameraPipe::m_checkThreadLoop(void)
{
    bool loop = false;

    if (m_isReprocessing() == false)
        loop = true;

    if (m_inputFrameQ->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;
}

void ExynosCameraPipe::m_dumpRunningFrameList(void)
{
    CLOGI("DEBUG(%s[%d]):*********runningFrameList dump***********", __FUNCTION__, __LINE__);
    CLOGI("DEBUG(%s[%d]):m_numBuffers : %d", __FUNCTION__, __LINE__, m_numBuffers);
    for (uint32_t i = 0; i < m_numBuffers; i++) {
        if (m_runningFrameList[i] == NULL) {
            CLOGI("DEBUG:runningFrameList[%d] is NULL", i);
        } else {
            CLOGI("DEBUG:runningFrameList[%d]: fcount = %d",
                    i, m_runningFrameList[i]->getFrameCount());
        }
    }
}

void ExynosCameraPipe::m_dumpPerframeNodeGroupInfo(const char *name, camera_pipe_perframe_node_group_info_t nodeInfo)
{
    if (name != NULL)
        CLOGI("DEBUG(%s[%d]):(%s) ++++++++++++++++++++", __FUNCTION__, __LINE__, name);

    CLOGI("\t\t perframeSupportNodeNum : %d", nodeInfo.perframeSupportNodeNum);
    CLOGI("\t\t perFrameLeaderInfo.perframeInfoIndex : %d", nodeInfo.perFrameLeaderInfo.perframeInfoIndex);
    CLOGI("\t\t perFrameLeaderInfo.perFrameVideoID : %d", nodeInfo.perFrameLeaderInfo.perFrameVideoID);
    CLOGI("\t\t perFrameCaptureInfo[0].perFrameVideoID : %d", nodeInfo.perFrameCaptureInfo[0].perFrameVideoID);
    CLOGI("\t\t perFrameCaptureInfo[1].perFrameVideoID : %d", nodeInfo.perFrameCaptureInfo[1].perFrameVideoID);

    if (name != NULL)
        CLOGI("DEBUG(%s[%d]):(%s) ------------------------------", __FUNCTION__, __LINE__, name);
}

void ExynosCameraPipe::m_configDvfs(void) {
    bool newDvfs = m_parameters->getDvfsLock();

    if (newDvfs != m_dvfsLocked) {
        setControl(V4L2_CID_IS_DVFS_LOCK, 533000);
        m_dvfsLocked = newDvfs;
    }
}

}; /* namespace android */
