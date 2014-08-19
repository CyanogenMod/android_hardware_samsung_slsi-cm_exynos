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
#define LOG_TAG "ExynosCameraPipe3AA_ISP"
#include <cutils/log.h>

#include "ExynosCameraPipe3AA_ISP.h"

namespace android {

ExynosCameraPipe3AA_ISP::ExynosCameraPipe3AA_ISP(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = isReprocessing ? 1 : 0;

    m_mainNodeNum = nodeNums[OUTPUT_NODE];
    m_ispNodeNum = nodeNums[SUB_NODE];

    m_subNode = NULL;
    m_ispNode = NULL;
    m_ispBufferQ = NULL;
    m_setfile = 0x0;

    memset(&m_perframeSubNodeGroupInfo, 0x00, sizeof(camera_pipe_perframe_node_group_info_t));
    memset(&m_perframeIspNodeGroupInfo, 0x00, sizeof(camera_pipe_perframe_node_group_info_t));

    m_activityControl = m_parameters->getActivityControl();
    m_exynosconfig = m_parameters->getConfig();
}

ExynosCameraPipe3AA_ISP::~ExynosCameraPipe3AA_ISP()
{
    this->destroy();
}

status_t ExynosCameraPipe3AA_ISP::create(int32_t *sensorIds)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    int fd = -1;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    /* ISP */
    m_ispNode = new ExynosCameraNode();
    ret = m_ispNode->create("ISP");
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): ispNode create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    ret = m_ispNode->open(m_ispNodeNum);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): ispNode open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    ALOGD("DEBUG(%s):Node(%d) opened", __FUNCTION__, FIMC_IS_VIDEO_ISP_NUM);

    m_ispThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe3AA_ISP::m_ispThreadFunc, "ISPThread", PRIORITY_URGENT_DISPLAY);

    m_ispBufferQ = new isp_buffer_queue_t;

    /* 3AA output */
    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("3AA_output");
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

    ret = m_mainNode->getFd(&fd);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):mainNode->getFd failed", __FUNCTION__, __LINE__);
        return ret;
    }

    /* 3AA capture */
    m_subNode = new ExynosCameraNode();
    ret = m_subNode->create("3AA_capture", fd);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): subNode create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe3AA_ISP::m_mainThreadFunc, "3AAThread", PRIORITY_URGENT_DISPLAY);

    ret = m_ispNode->setInput(sensorIds[SUB_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): ispNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[SUB_NODE], ret);
        return ret;
    }

    ret = m_mainNode->setInput(sensorIds[OUTPUT_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): mainNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[OUTPUT_NODE], ret);
        return ret;
    }

    ret = m_subNode->setInput(sensorIds[CAPTURE_NODE]);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): subNode setInput fail, sensorId(%d), ret(%d)", __FUNCTION__, __LINE__, sensorIds[CAPTURE_NODE], ret);
        return ret;
    }

    m_inputFrameQ = new frame_queue_t;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::destroy(void)
{
    int ret = 0;
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    if (m_ispNode != NULL) {
        ret = m_ispNode->close();
        if (ret < 0) {
            ALOGE("ERR(%s):isp node close fail(ret = %d)", __FUNCTION__, ret);
            return ret;
        }
        delete m_ispNode;
        m_ispNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) opened", __FUNCTION__, FIMC_IS_VIDEO_ISP_NUM);
    }
    ALOGD("DEBUG(%s[%d]):isp node destroyed", __FUNCTION__, __LINE__);

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
    ALOGD("DEBUG(%s[%d]):3AA output node destroyed", __FUNCTION__, __LINE__);

    if (m_subNode != NULL) {
        delete m_subNode;
        m_subNode = NULL;
    }
    ALOGD("DEBUG(%s[%d]):3AA capture node destroyed", __FUNCTION__, __LINE__);

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    if (m_ispBufferQ != NULL) {
        m_ispBufferQ->release();
        delete m_ispBufferQ;
        m_ispBufferQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::setupPipe(camera_pipe_info_t *pipeInfos)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    status_t ret = NO_ERROR;

#ifdef DEBUG_RAWDUMP
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
#endif
    /* TODO: check node state */

    /* initialize node */
    int maxW = pipeInfos[2].rectInfo.fullW;
    int maxH = pipeInfos[2].rectInfo.fullH;
    int colorFormat = pipeInfos[2].rectInfo.colorFormat;
    enum v4l2_buf_type bufType = (enum v4l2_buf_type)pipeInfos[2].bufInfo.type;
    enum v4l2_memory memType = (enum v4l2_memory)pipeInfos[2].bufInfo.memory;
    m_numBuffers = pipeInfos[2].bufInfo.count;

    m_perframeIspNodeGroupInfo = pipeInfos[2].perFrameNodeGroupInfo;

    if (maxW > 0 && maxH > 0) {
        /* isp */
        m_ispNode->setSize(maxW, maxH);
        m_ispNode->setColorFormat(colorFormat, 2);
        m_ispNode->setBufferType(m_numBuffers, bufType, memType);

#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            bytesPerLine[0] = (maxW + 16) * 2;
            m_ispNode->setFormat();
        } else
#endif /* DEBUG_RAWDUMP */
        {
            m_ispNode->setFormat(pipeInfos[2].bytesPerPlane);
        }
        m_ispNode->reqBuffers();
    }

    /* 3a1 output */
    maxW = pipeInfos[0].rectInfo.fullW;
    maxH = pipeInfos[0].rectInfo.fullH;
    colorFormat = pipeInfos[0].rectInfo.colorFormat;
    bufType = (enum v4l2_buf_type)pipeInfos[0].bufInfo.type;
    memType = (enum v4l2_memory)pipeInfos[0].bufInfo.memory;
    m_numBuffers = pipeInfos[0].bufInfo.count;

    m_perframeMainNodeGroupInfo = pipeInfos[0].perFrameNodeGroupInfo;

    m_mainNode->setSize(maxW, maxH);
    m_mainNode->setColorFormat(colorFormat, 2);
    m_mainNode->setBufferType(m_numBuffers, bufType, memType);
    m_mainNode->setFormat();
    m_mainNode->reqBuffers();

    /* 3a1 capture */
    maxW = pipeInfos[1].rectInfo.fullW;
    maxH = pipeInfos[1].rectInfo.fullH;
    colorFormat = pipeInfos[1].rectInfo.colorFormat;
    bufType = (enum v4l2_buf_type)pipeInfos[1].bufInfo.type;
    memType = (enum v4l2_memory)pipeInfos[1].bufInfo.memory;
    m_numBuffers = pipeInfos[1].bufInfo.count;

    m_perframeSubNodeGroupInfo = pipeInfos[1].perFrameNodeGroupInfo;

    if (maxW > 0 && maxH > 0) {
        m_subNode->setSize(maxW, maxH);
        m_subNode->setColorFormat(colorFormat, 2);
        m_subNode->setBufferType(m_numBuffers, bufType, memType);

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
    }

    /* setfile setting */
    int setfile = 0;
    int yuvRange = 0;
    m_parameters->getSetfileYuvRange(m_reprocessing, &setfile, &yuvRange);

#ifdef SET_SETFILE_BY_SHOT
    m_setfile = mergeSetfileYuvRange(setfile, yuvRange);
#else
#if SET_SETFILE_BY_SET_CTRL_3AA_ISP
    ret = m_ispNode->setControl(V4L2_CID_IS_SET_SETFILE, setfile);
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]):setControl(%d) fail(ret = %d)", __FUNCTION__, __LINE__, setfile, ret);
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
    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::prepare(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    for (uint32_t i = 0; i < m_prepareBufferCount; i++) {
        ret = m_putBuffer();
        if (ret < 0) {
            ALOGE("ERR(%s):m_putBuffer fail(ret = %d)", __FUNCTION__, ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::instantOn(int32_t numFrames)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer newBuffer;

    if (m_inputFrameQ->getSizeOfProcessQ() != numFrames) {
        ALOGE("ERR(%s[%d]): instantOn need %d Frames, but %d Frames are queued",
                __FUNCTION__, __LINE__, numFrames, m_inputFrameQ->getSizeOfProcessQ());
        return INVALID_OPERATION;
    }

    ret = m_mainNode->start();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): mainNode instantOn fail", __FUNCTION__, __LINE__);
        return ret;
    }
    ret = m_ispNode->start();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): mainNode instantOn fail", __FUNCTION__, __LINE__);
        return ret;
    }

    for (int i = 0; i < numFrames; i++) {
        ret = m_inputFrameQ->popProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            return ret;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        ret = newFrame->getSrcBuffer(getPipeId(), &newBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):frame get buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            return INVALID_OPERATION;
        }

        ALOGD("DEBUG(%s[%d]): put instantOn Buffer (index %d)", __FUNCTION__, __LINE__, newBuffer.index);
        ret = m_mainNode->putBuffer(&newBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):putBuffer fail", __FUNCTION__, __LINE__);
            return ret;
            /* TODO: doing exception handling */
        }
    }

    return ret;
}

status_t ExynosCameraPipe3AA_ISP::instantOff(void)
{
    int ret = 0;
    ret = m_mainNode->stop();
    ret = m_ispNode->stop();

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA output node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_subNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA capture node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_ispNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):isp node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::setControl(int cid, int value)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ret = m_mainNode->setControl(cid, value);
    if (ret != NO_ERROR)
        ALOGE("ERR(%s[%d]):mainNode->setControl failed", __FUNCTION__, __LINE__);

    ret = m_subNode->setControl(cid, value);
    if (ret != NO_ERROR)
        ALOGE("ERR(%s[%d]):m_subNode->setControl failed", __FUNCTION__, __LINE__);

    ret = m_ispNode->setControl(cid, value);
    if (ret != NO_ERROR)
        ALOGE("ERR(%s[%d]):m_ispNode->setControl failed", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCameraPipe3AA_ISP::start(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    /* TODO: check state ready for start */
    int ret = 0;

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

    ret = m_ispNode->start();
    if (ret != NO_ERROR) {
        ALOGE("ERR(%s[%d]): Starting ispNode Error!", __FUNCTION__, __LINE__);
        return ret;
    }

    m_flagStartPipe = true;
    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::stop(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_flagStartPipe = false;
    m_flagTryStop = false;

    /* 3AA output stop */
    ret = m_mainNode->stop();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA output node stop fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA output node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    /* 3AA capture stop */
    ret = m_subNode->stop();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA capture node stop fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_subNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):3AA capture node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    /* isp output stop */
    ret = m_ispNode->stop();
    if (ret < 0) {
        ALOGE("ERR(%s):isp node stop fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    ret = m_ispNode->clrBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s):isp node clrBuffers fail, ret(%d)", __FUNCTION__, ret);
        return ret;
    }

    m_mainThread->requestExitAndWait();
    m_ispThread->requestExitAndWait();

    m_inputFrameQ->release();
    m_ispBufferQ->release();

    for (uint32_t i = 0; i < m_numBuffers; i++)
        m_runningFrameList[i] = NULL;

    m_numOfRunningFrame = 0;

    CLOGD("DEBUG(%s[%d]): thead exited", __FUNCTION__, __LINE__);

    m_mainNode->removeItemBufferQ();
    m_subNode->removeItemBufferQ();
    m_ispNode->removeItemBufferQ();

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::startThread(void)
{
    if (m_outputFrameQ == NULL) {
        ALOGE("ERR(%s):outputFrameQ is NULL, cannot start", __FUNCTION__);
        return INVALID_OPERATION;
    }

    m_mainThread->run();
    m_ispThread->run();

    ALOGI("INFO(%s[%d]):startThread is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::stopThread(void)
{
    /* stop thread */
    m_mainThread->requestExit();
    m_ispThread->requestExit();

    m_inputFrameQ->sendCmd(WAKE_UP);
    m_ispBufferQ->sendCmd(WAKE_UP);

    m_dumpRunningFrameList();

    return NO_ERROR;
}

void ExynosCameraPipe3AA_ISP::dump()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    m_dumpRunningFrameList();
    m_mainNode->dump();
    m_subNode->dump();
    m_ispNode->dump();

    return;
}

status_t ExynosCameraPipe3AA_ISP::m_putBuffer(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer fliteBuffer;
    ExynosCameraBuffer ispBuffer;
    int ret = 0;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            ALOGW("WARN(%s): wait timeout", __FUNCTION__);
            m_mainNode->dumpState();
            m_subNode->dumpState();
        } else {
            ALOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        ALOGE("ERR(%s):new frame is NULL", __FUNCTION__);
        //return INVALID_OPERATION;
        return NO_ERROR;
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
        ALOGE("ERR(%s):new buffer is invalid, we already get buffer index(%d), newFrame->frameCount(%d)",
            __FUNCTION__, fliteBuffer.index, newFrame->getFrameCount());
        m_dumpRunningFrameList();
        return BAD_VALUE;
    }

    ret = m_subNode->putBuffer(&ispBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s):capture putBuffer fail ret(%d)", __FUNCTION__, ret);
        /* TODO: doing exception handling */
        return ret;
    }

    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)(fliteBuffer.addr[1]);

    if (shot_ext != NULL) {
        int previewW = 0, previewH = 0;
        int pictureW = 0, pictureH = 0;
        int cropW = 0, cropH = 0, cropX = 0, cropY = 0;

        m_parameters->getHwPreviewSize(&previewW, &previewH);
        m_parameters->getPictureSize(&pictureW, &pictureH);
        m_parameters->getHwBayerCropRegion(&cropW, &cropH, &cropX, &cropY);

        newFrame->getMetaData(shot_ext);
        ret = m_parameters->duplicateCtrlMetadata((void *)shot_ext);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):duplicate Ctrl metadata fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        setMetaSetfile(shot_ext, m_setfile);

        m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&fliteBuffer);

        if (m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
            int zoomParamInfo = m_parameters->getZoomLevel();
            int zoomFrameInfo = 0;
            int previewW = 0, previewH = 0;
            ExynosRect sensorSize;
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

                m_parameters->getHwPreviewSize(&previewW, &previewH);
                m_parameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);
                m_parameters->getPreviewBdsSize(&bdsSize);
                newFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, &zoomFrameInfo);

                updateNodeGroupInfoMainPreview(
                    m_cameraId,
                    &node_group_info,
                    &node_group_info_isp,
                    bayerCropSize,
                    bdsSize,
                    previewW, previewH);

                newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_3AA, zoomParamInfo);
                newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP, zoomParamInfo);
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

            /* ALOGI("INFO(%s[%d]):fcount(%d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount); */
            /* newFrame->dumpNodeGroupInfo("3AA_ISP"); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeIspNodeGroupInfo", m_perframeIspNodeGroupInfo); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeMainNodeGroupInfo", m_perframeMainNodeGroupInfo); */
        }
    }

    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s): setDstBuffer state fail", __FUNCTION__);
        return ret;
    }

    ret = m_mainNode->putBuffer(&fliteBuffer);
    if (ret < 0) {
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

status_t ExynosCameraPipe3AA_ISP::m_getBuffer(void)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraFrame *perframeFrame = NULL;
    ExynosCameraBuffer fliteBuffer;
    ExynosCameraBuffer ispBuffer;
    int index = 0;
    int ret = 0;
    int error = 0;
    camera2_node_group node_group_info;

    memset(&node_group_info, 0x0, sizeof(camera2_node_group));

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

    m_activityControl->activityAfterExecFunc(getPipeId(), (void *)&fliteBuffer);

    ret = m_updateMetadataToFrame(fliteBuffer.addr[1], fliteBuffer.index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): updateMetadataToFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_getFrameByIndex(&perframeFrame, fliteBuffer.index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): getFrameByIndex fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    } else {
        perframeFrame->getNodeGroupInfo(&node_group_info, m_perframeIspNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex);
#if 0
    ret = m_checkShotDone((struct camera2_shot_ext*)fliteBuffer.addr[1]);
    if (ret < 0) {
        ALOGE("ERR(%s):Shot done invalid, frame skip", __FUNCTION__);
        /* TODO: doing exception handling */

        /* complete frame */
        ret = m_completeFrame(&curFrame, fliteBuffer, false);
        if (ret < 0) {
            ALOGE("ERR(%s):m_completeFrame is fail", __FUNCTION__);
            /* TODO: doing exception handling */
            return ret;
        }

        if (curFrame == NULL) {
            ALOGE("ERR(%s):curFrame is fail", __FUNCTION__);
        }

        m_outputFrameQ->pushProcessQ(&curFrame);

        return NO_ERROR;
#endif
    }

    /* TODO: Is it necessary memcpy shot.ctl from parameter? */
    camera2_shot_ext *shot_ext_src = (struct camera2_shot_ext *)(fliteBuffer.addr[1]);
    camera2_shot_ext *shot_ext_dst = (struct camera2_shot_ext *)(ispBuffer.addr[1]);

    if ((shot_ext_src != NULL) && (shot_ext_dst != NULL)) {
        int previewW, previewH;
        m_parameters->getHwPreviewSize(&previewW, &previewH);
        memcpy(&shot_ext_dst->shot.ctl, &shot_ext_src->shot.ctl, sizeof(struct camera2_ctl) - sizeof(struct camera2_entry_ctl));
        memcpy(&shot_ext_dst->shot.udm, &shot_ext_src->shot.udm, sizeof(struct camera2_udm));
        memcpy(&shot_ext_dst->shot.dm, &shot_ext_src->shot.dm, sizeof(struct camera2_dm));

        if (m_perframeIspNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
            memset(&shot_ext_dst->node_group, 0x0, sizeof(camera2_node_group));

            /* Per - ISP */
            if (node_group_info.leader.request == 1) {
                setMetaNodeLeaderInputSize(shot_ext_dst,
                    node_group_info.leader.input.cropRegion[0],
                    node_group_info.leader.input.cropRegion[1],
                    node_group_info.leader.input.cropRegion[2],
                    node_group_info.leader.input.cropRegion[3]);
                setMetaNodeLeaderOutputSize(shot_ext_dst,
                    node_group_info.leader.output.cropRegion[0],
                    node_group_info.leader.output.cropRegion[1],
                    node_group_info.leader.output.cropRegion[2],
                    node_group_info.leader.output.cropRegion[3]);
                setMetaNodeLeaderRequest(shot_ext_dst,
                    node_group_info.leader.request);
                setMetaNodeLeaderVideoID(shot_ext_dst,
                    m_perframeIspNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID);
            }

            /* Per - SCP */
            for (int i = 0; i < m_perframeIspNodeGroupInfo.perframeSupportNodeNum - 1; i ++) {
                if (node_group_info.capture[i].request == 1) {
                    setMetaNodeCaptureInputSize(shot_ext_dst, i,
                        node_group_info.capture[i].input.cropRegion[0],
                        node_group_info.capture[i].input.cropRegion[1],
                        node_group_info.capture[i].input.cropRegion[2],
                        node_group_info.capture[i].input.cropRegion[3]);
                    setMetaNodeCaptureOutputSize(shot_ext_dst, i,
                        node_group_info.capture[i].output.cropRegion[0],
                        node_group_info.capture[i].output.cropRegion[1],
                        node_group_info.capture[i].output.cropRegion[2],
                        node_group_info.capture[i].output.cropRegion[3]);
                    setMetaNodeCaptureRequest(shot_ext_dst, i, node_group_info.capture[i].request);
                    setMetaNodeCaptureVideoID(shot_ext_dst, i, m_perframeIspNodeGroupInfo.perFrameCaptureInfo[i].perFrameVideoID);
                }
            }
            /* ALOGE("INFO(%s[%d]):fcount(%d)", __FUNCTION__, __LINE__, shot_ext_dst->shot.dm.request.frameCount); */
            /* perframeFrame->dumpNodeGroupInfo("ISP__"); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeIspNodeGroupInfo", m_perframeIspNodeGroupInfo); */
            /* m_dumpPerframeNodeGroupInfo("m_perframeMainNodeGroupInfo", m_perframeMainNodeGroupInfo); */
        }

        shot_ext_dst->setfile = shot_ext_src->setfile;
        shot_ext_dst->drc_bypass = shot_ext_src->drc_bypass;
        shot_ext_dst->dis_bypass = shot_ext_src->dis_bypass;
        shot_ext_dst->dnr_bypass = shot_ext_src->dnr_bypass;
        shot_ext_dst->fd_bypass = shot_ext_src->fd_bypass;

        shot_ext_dst->shot.dm.request.frameCount = shot_ext_src->shot.dm.request.frameCount;
        shot_ext_dst->shot.magicNumber= shot_ext_src->shot.magicNumber;
    } else {
        ALOGE("ERR(%s[%d]):shot_ext is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&ispBuffer);

    ret = m_ispNode->putBuffer(&ispBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s):m_ispNode->putBuffer fail ret(%d)", __FUNCTION__, ret);
        /* TODO: doing exception handling */
        return ret;
    }

    m_ispBufferQ->pushProcessQ(&ispBuffer);

    return NO_ERROR;
}

status_t ExynosCameraPipe3AA_ISP::m_checkShotDone(struct camera2_shot_ext *shot_ext)
{
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (shot_ext->node_group.leader.request != 1) {
        ALOGW("WARN(%s[%d]): 3a1 NOT DONE, frameCount(%d)", __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount(shot_ext));
        /* TODO: doing exception handling */
        return INVALID_OPERATION;
    }

    return OK;
}

status_t ExynosCameraPipe3AA_ISP::m_getIspBuffer(void)
{
    ExynosCameraFrame *curFrame = NULL;
    ExynosCameraBuffer newBuffer;
    ExynosCameraBuffer curBuffer;
    int index = 0;
    int ret = 0;

    ret = m_ispBufferQ->waitAndPopProcessQ(&curBuffer);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            ALOGW("WARN(%s):wait timeout", __FUNCTION__);
            m_mainNode->dumpState();
            m_subNode->dumpState();
        } else {
            ALOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    ret = m_ispNode->getBuffer(&newBuffer, &index);
    if (ret != NO_ERROR || index < 0) {
        ALOGE("ERR(%s[%d]):m_ispNode->getBuffer fail ret(%d)", __FUNCTION__, __LINE__, ret);
        camera2_shot_ext *shot_ext;
        shot_ext = (struct camera2_shot_ext *)(newBuffer.addr[1]);
        newBuffer.index = index;
        ALOGE("ERR(%s[%d]):Shot done invalid, frame(cnt:%d, index(%d)) skip", __FUNCTION__, __LINE__, getMetaDmRequestFrameCount(shot_ext), index);

        /* complete frame */
        ret = m_completeFrame(&curFrame, newBuffer, false);
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

    if (curBuffer.index != newBuffer.index) {
        ALOGW("ERR(%s[%d]):Frame mismatch, we expect index %d, but we got index %d",
            __FUNCTION__, __LINE__, curBuffer.index, newBuffer.index);
        /* TODO: doing exception handling */
    }

    ret = m_updateMetadataToFrame(curBuffer.addr[1], curBuffer.index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): updateMetadataToFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    /* complete frame */
    ret = m_completeFrame(&curFrame, newBuffer);
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

bool ExynosCameraPipe3AA_ISP::m_mainThreadFunc(void)
{
    int ret = 0;

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    /* deliver buffer from 3AA node to ISP node */
    ret = m_getBuffer();
    if (ret < 0) {
        ALOGE("ERR(%s):m_getBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return true;
    }

    /* put buffer to 3AA node */
    ret = m_putBuffer();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;
        ALOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return false;
    }

    return true;
}

bool ExynosCameraPipe3AA_ISP::m_ispThreadFunc(void)
{
    int ret = 0;

    /* get buffer from ISP node */
    ret = m_getIspBuffer();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;
        ALOGE("ERR(%s):m_getIspBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return false;
    }

    m_timer.stop();
    m_timeInterval = m_timer.durationMsecs();
    m_timer.start();

    /* update renew count */
    if (ret >= 0)
        m_threadRenew = 0;

    return true;
}

}; /* namespace android */
