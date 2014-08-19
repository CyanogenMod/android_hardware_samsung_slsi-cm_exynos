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
#define LOG_TAG "ExynosCameraPipeGSC"
#include <cutils/log.h>

#include "ExynosCameraPipeGSC.h"

namespace android {

ExynosCameraPipeGSC::ExynosCameraPipeGSC(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = isReprocessing ? 1 : 0;

    m_mainNodeNum = (nodeNums == NULL ? -1 : nodeNums[0]);

    m_csc = NULL;
    m_property = (nodeNums == NULL) ? CSC_HW_PROPERTY_DEFAULT : CSC_HW_PROPERTY_FIXED_NODE;
}

ExynosCameraPipeGSC::~ExynosCameraPipeGSC()
{
    this->destroy();
}

status_t ExynosCameraPipeGSC::create(int32_t *sensorIds)
{
    CSC_METHOD cscMethod = CSC_METHOD_HW;

    m_mainNode = NULL;

    m_csc = csc_init(cscMethod);
    if (m_csc == NULL) {
        ALOGE("ERR(%s):csc_init() fail", __FUNCTION__);
        return INVALID_OPERATION;
    }

    csc_set_hw_property(m_csc, m_property, m_mainNodeNum);

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeGSC::m_mainThreadFunc, "GSCThread");

    m_inputFrameQ = new frame_queue_t(m_mainThread);

    ALOGI("INFO(%s[%d]):create() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::destroy(void)
{
    if (m_csc != NULL)
        csc_deinit(m_csc);
    m_csc = NULL;

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::start(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::stop(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_mainThread->requestExitAndWait();

    CLOGD("DEBUG(%s[%d]): thead exited", __FUNCTION__, __LINE__);

    m_inputFrameQ->release();

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::startThread(void)
{
    ALOGV("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    if (m_outputFrameQ == NULL) {
        ALOGE("ERR(%s):outputFrameQ is NULL, cannot start", __FUNCTION__);
        return INVALID_OPERATION;
    }

    m_mainThread->run();

    ALOGI("INFO(%s[%d]):startThread is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::m_run(void)
{
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosRect srcRect;
    ExynosRect dstRect;

    int ret = 0;
    int rotation = 0;
    int flipHorizontal = m_parameters->getFlipHorizontal();
    int flipVertical = m_parameters->getFlipVertical();

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            ALOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            ALOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        ALOGE("ERR(%s):new frame is NULL", __FUNCTION__);
        return NO_ERROR;
    }

    ret = newFrame->getSrcRect(getPipeId(), &srcRect);
    ret = newFrame->getDstRect(getPipeId(), &dstRect);

    csc_set_src_format(m_csc,
        ALIGN_UP(srcRect.fullW, CAMERA_MAGIC_ALIGN),
        ALIGN_UP(srcRect.fullH, CAMERA_MAGIC_ALIGN),
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
        0);

    csc_set_dst_format(m_csc,
        dstRect.fullW, dstRect.fullH,
        dstRect.x, dstRect.y, dstRect.fullW, dstRect.fullH,
        V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat),
        0);

    ret = newFrame->getSrcBuffer(getPipeId(), &srcBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get src buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &dstBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get dst buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    csc_set_src_buffer(m_csc,
            (void **)srcBuffer.fd, CSC_MEMORY_TYPE);

    csc_set_dst_buffer(m_csc,
            (void **)dstBuffer.fd, CSC_MEMORY_TYPE);

    if (csc_convert_with_rotation(m_csc, rotation, flipHorizontal, flipVertical) != 0)
        ALOGE("ERR(%s):csc_convert() fail", __FUNCTION__);

    ALOGV("DEBUG(%s[%d]): ratation(%d), flip horizontal(%d), vertical(%d)",
            __FUNCTION__, __LINE__, rotation, flipHorizontal, flipVertical);

    ALOGV("DEBUG(%s[%d]):CSC(%d) converting done", __FUNCTION__, __LINE__, m_mainNodeNum);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):set entity state fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

bool ExynosCameraPipeGSC::m_mainThreadFunc(void)
{
    int ret = 0;

    ret = m_run();
    if (ret < 0) {
        if (ret != TIMED_OUT)
            ALOGE("ERR(%s):m_putBuffer fail", __FUNCTION__);
    }

    return m_checkThreadLoop();
}

}; /* namespace android */
