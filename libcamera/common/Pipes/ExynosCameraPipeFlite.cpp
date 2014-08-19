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
#define LOG_TAG "ExynosCameraPipeFlite"
#include <cutils/log.h>

#include "ExynosCameraPipeFlite.h"

namespace android {

ExynosCameraPipeFlite::ExynosCameraPipeFlite(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = isReprocessing ? 1 : 0;
    m_mainNodeNum = nodeNums[CAPTURE_NODE];

    m_activityControl = m_parameters->getActivityControl();

    m_exynosconfig = m_parameters->getConfig();
}

ExynosCameraPipeFlite::~ExynosCameraPipeFlite()
{
    this->destroy();
}

status_t ExynosCameraPipeFlite::create(int32_t *sensorIds)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("flite");
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

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeFlite::m_mainThreadFunc, "fliteThread", PRIORITY_URGENT_DISPLAY);

    m_inputFrameQ = new frame_queue_t;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::destroy(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    if (m_mainNode != NULL) {
        if (m_mainNode->close() != NO_ERROR) {
            ALOGE("ERR(%s):close fail", __FUNCTION__);
            return INVALID_OPERATION;
        }
        delete m_mainNode;
        m_mainNode = NULL;
        ALOGD("DEBUG(%s):Node(%d) closed", __FUNCTION__, m_mainNodeNum);
    }

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::setupPipe(camera_pipe_info_t *pipeInfos)
{
    CLOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
#ifdef DEBUG_RAWDUMP
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
#endif

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

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = (maxW) * 2;
        m_mainNode->setFormat();
    } else
#endif /* DEBUG_RAWDUMP */
    {
        m_mainNode->setFormat(pipeInfos[0].bytesPerPlane);
    }
    m_mainNode->reqBuffers();

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):setupPipe() is succeed (%d) setupPipe (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::sensorStream(bool on)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    int value = on ? IS_ENABLE_STREAM: IS_DISABLE_STREAM;

    ret = m_mainNode->setControl(V4L2_CID_IS_S_STREAM, value);
    if (ret != NO_ERROR)
        ALOGE("ERR(%s):sensor S_STREAM(%d) fail", __FUNCTION__, value);

    return ret;
}

bool ExynosCameraPipeFlite::m_mainThreadFunc(void)
{
    int ret = 0;

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    ret = m_getBuffer();
    if (ret < 0) {
        ALOGE("ERR(%s):m_getBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
    }

    if (m_numOfRunningFrame < 2) {
        int cnt = m_inputFrameQ->getSizeOfProcessQ();
        do {
            ret = m_putBuffer();
            if (ret < 0) {
                if (ret == TIMED_OUT)
                    return true;
                ALOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
                /* TODO: doing exception handling */
            }
            cnt--;
        } while (cnt > 0);
    } else {
        ret = m_putBuffer();
        if (ret < 0) {
            if (ret == TIMED_OUT)
                return true;
            ALOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
            /* TODO: doing exception handling */
        }
     }

    return true;
}

}; /* namespace android */
