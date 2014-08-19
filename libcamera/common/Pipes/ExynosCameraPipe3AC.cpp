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
#define LOG_TAG "ExynosCameraPipe3AC"
#include <cutils/log.h>

#include "ExynosCameraPipe3AC.h"

namespace android {

ExynosCameraPipe3AC::ExynosCameraPipe3AC(
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

ExynosCameraPipe3AC::~ExynosCameraPipe3AC()
{
        this->destroy();
}

status_t ExynosCameraPipe3AC::create(int32_t *sensorIds)
{
    ALOGD("[%s(%d)]", __FUNCTION__, __LINE__);
    int ret = 0;

    if (sensorIds == NULL) {
        ALOGE("ERR(%s[%d]): Pipe need sensorId", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    m_mainNode = new ExynosCameraNode();
    ret = m_mainNode->create("3AC");
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

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe3AC::m_mainThreadFunc, "3ACThread");

    m_inputFrameQ = new frame_queue_t(m_mainThread);

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    ALOGI("INFO(%s[%d]):create() is succeed (%d) prepare (%d)", __FUNCTION__, __LINE__, getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe3AC::destroy(void)
{
    ALOGD("[%s(%d)]", __FUNCTION__, __LINE__);
    if (m_mainNode != NULL) {
        if (m_mainNode->close() != NO_ERROR) {
            ALOGE("ERR(%s): close fail", __FUNCTION__);
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

status_t ExynosCameraPipe3AC::sensorStream(bool on)
{
    ALOGD("[%s(%d)]", __FUNCTION__, __LINE__);

    int ret = 0;
    int value = on ? IS_ENABLE_STREAM: IS_DISABLE_STREAM;

    ret = m_mainNode->setControl(V4L2_CID_IS_S_STREAM, value);
    if (ret != NO_ERROR)
        ALOGE("ERR(%s): sensor S_STREAM(%d) fail", __FUNCTION__, value);

    return ret;
}

bool ExynosCameraPipe3AC::m_checkThreadLoop(void)
{
    bool loop = false;

    if (m_isReprocessing() == false)
        loop = true;

    if (m_inputFrameQ->getSizeOfProcessQ() > 0)
        loop = true;

    if (m_inputFrameQ->getSizeOfProcessQ() == 0 &&
        m_numOfRunningFrame == 0)
        loop = false;

    return loop;
}

bool ExynosCameraPipe3AC::m_mainThreadFunc(void)
{
    int ret = 0;

    if (m_flagStartPipe == false) {
        /* waiting for pipe started */
        usleep(5000);
        return m_checkThreadLoop();
    }

    if (m_numOfRunningFrame == 0 && m_inputFrameQ->getSizeOfProcessQ() != 0) {
        ret = prepare();
        if (ret < 0)
            ALOGE("ERR(%s[%d]):FLITE prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_getBuffer();
    if (ret < 0) {
        ALOGE("ERR(%s): m_getBuffer fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return m_checkThreadLoop();
    }

    if (m_numOfRunningFrame < m_prepareBufferCount - 1) {
        int cnt = m_inputFrameQ->getSizeOfProcessQ();
        do {
            ret = m_putBuffer();
            if (ret < 0) {
                if (ret == TIMED_OUT)
                    return m_checkThreadLoop();
                ALOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
                /* TODO: doing exception handling */
                return m_checkThreadLoop();
            }
            cnt--;
        } while (cnt > 0);
    } else {
        ret = m_putBuffer();
        if (ret < 0) {
            ALOGE("ERR(%s): m_putBuffer fail", __FUNCTION__);
            /* TODO: doing exception handling */
            return m_checkThreadLoop();
        }
    }

    return m_checkThreadLoop();
}

}; /* namespace android */
