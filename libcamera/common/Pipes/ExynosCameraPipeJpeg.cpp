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
#define LOG_TAG "ExynosCameraPipeJpeg"
#include <cutils/log.h>

#include "ExynosCameraPipeJpeg.h"

/* For test */
#include "ExynosCameraBuffer.h"

namespace android {

ExynosCameraPipeJpeg::ExynosCameraPipeJpeg(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)
{
    m_cameraId = cameraId;
    m_parameters = obj_param;
    m_reprocessing = 1;

    m_csc = NULL;
}

ExynosCameraPipeJpeg::~ExynosCameraPipeJpeg()
{
    this->destroy();
}

status_t ExynosCameraPipeJpeg::create(int32_t *sensorIds)
{
    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeJpeg::m_mainThreadFunc, "JpegThread");

    m_mainNode = NULL;

    m_inputFrameQ = new frame_queue_t(m_mainThread);

    ALOGI("INFO(%s[%d]):create() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::destroy(void)
{

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ALOGI("INFO(%s[%d]):destroy() is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::start(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    /* TODO: check state ready for start */

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::stop(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_mainThread->requestExitAndWait();

    CLOGD("DEBUG(%s[%d]): thead exited", __FUNCTION__, __LINE__);

    m_inputFrameQ->release();

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::startThread(void)
{
    ALOGD("DEBUG(%s[%d])", __FUNCTION__, __LINE__);

    start();

    if (m_outputFrameQ == NULL) {
        ALOGE("ERR(%s):outputFrameQ is NULL, cannot start", __FUNCTION__);
        return INVALID_OPERATION;
    }

    m_mainThread->run();

    ALOGI("INFO(%s[%d]):startThread is succeed (%d)", __FUNCTION__, __LINE__, getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeJpeg::m_run(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = 0;
    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer yuvBuf;
    ExynosCameraBuffer jpegBuf;

    ExynosRect pictureRect;
    int jpegQuality = m_parameters->getJpegQuality();

    ExynosRect thumbnailRect;
    int thumbnailQuality = m_parameters->getThumbnailQuality();

    exif_attribute_t exifInfo;
    m_parameters->getFixedExifInfo(&exifInfo);
    struct camera2_shot_ext shot_ext;
    memset(&shot_ext, 0x00, sizeof(struct camera2_shot_ext));

    pictureRect.colorFormat = m_parameters->getPictureFormat();
    m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);
    m_parameters->getThumbnailSize(&thumbnailRect.w, &thumbnailRect.h);

    ALOGI("DEBUG(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    ALOGD("DEBUG(%s[%d]):picture size(%dx%d), thumbnail size(%dx%d)",
            __FUNCTION__, __LINE__, pictureRect.w, pictureRect.h, thumbnailRect.w, thumbnailRect.h);

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

    ret = newFrame->getSrcBuffer(getPipeId(), &yuvBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get src buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &jpegBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):frame get dst buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_jpegEnc.create()) {
        ALOGE("ERR(%s):m_jpegEnc.create() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setQuality(jpegQuality)) {
        ALOGE("ERR(%s):m_jpegEnc.setQuality() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setSize(pictureRect.w, pictureRect.h)) {
        ALOGE("ERR(%s):m_jpegEnc.setSize() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setColorFormat(pictureRect.colorFormat)) {
        ALOGE("ERR(%s):m_jpegEnc.setColorFormat() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setJpegFormat(V4L2_PIX_FMT_JPEG_422)) {
        ALOGE("ERR(%s):m_jpegEnc.setJpegFormat() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (thumbnailRect.w != 0 && thumbnailRect.h != 0) {
        exifInfo.enableThumb = true;
        if (pictureRect.w < 320 || pictureRect.h < 240) {
            thumbnailRect.w = 160;
            thumbnailRect.h = 120;
        }
        if (m_jpegEnc.setThumbnailSize(thumbnailRect.w, thumbnailRect.h)) {
            ALOGE("ERR(%s):m_jpegEnc.setThumbnailSize(%d, %d) fail", __FUNCTION__, thumbnailRect.w, thumbnailRect.h);
            ret = INVALID_OPERATION;
            goto jpeg_encode_done;
        }
        if (0 < thumbnailQuality && thumbnailQuality <= 100) {
            if (m_jpegEnc.setThumbnailQuality(thumbnailQuality)) {
                ret = INVALID_OPERATION;
                ALOGE("ERR(%s):m_jpegEnc.setThumbnailQuality(%d) fail", __FUNCTION__, thumbnailQuality);
            }
        }
    } else {
        exifInfo.enableThumb = false;
    }

    /* wait for medata update */
    if(newFrame->getMetaDataEnable() == false) {
        CLOGD("DEBUG(%s[%d]): Waiting for update jpeg metadata failed (%d) ", __FUNCTION__, __LINE__, ret);
    }

    /* get dynamic meters for make exif info */
    newFrame->getDynamicMeta(&shot_ext);
    newFrame->getUserDynamicMeta(&shot_ext);

    m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect,&shot_ext.shot.dm, &shot_ext.shot.udm);

    if (m_jpegEnc.setInBuf((int *)&(yuvBuf.fd), (int *)yuvBuf.size)) {
        ALOGE("ERR(%s):m_jpegEnc.setInBuf() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.setOutBuf(jpegBuf.fd[0], jpegBuf.size[0] + jpegBuf.size[1] + jpegBuf.size[2])) {
        ALOGE("ERR(%s):m_jpegEnc.setOutBuf() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.updateConfig()) {
        ALOGE("ERR(%s):m_jpegEnc.updateConfig() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    if (m_jpegEnc.encode((int *)&jpegBuf.size, &exifInfo, m_parameters->getDebugAttribute())) {
        ALOGE("ERR(%s):m_jpegEnc.encode() fail", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto jpeg_encode_done;
    }

    newFrame->setJpegSize(jpegBuf.size[0]);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):set entity state fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

jpeg_encode_done:
    if (ret != NO_ERROR) {
        ALOGD("[jpegBuf.fd[0] %d][jpegBuf.size[0] + jpegBuf.size[1] + jpegBuf.size[2] %d]",
            jpegBuf.fd[0], jpegBuf.size[0] + jpegBuf.size[1] + jpegBuf.size[2]);
        ALOGD("[pictureW %d][pictureH %d][pictureFormat %d]",
            pictureRect.w, pictureRect.h, pictureRect.colorFormat);
    }

    if (m_jpegEnc.flagCreate() == true)
        m_jpegEnc.destroy();

    ALOGI("DEBUG(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

bool ExynosCameraPipeJpeg::m_mainThreadFunc(void)
{
    int ret = 0;

    ret = m_run();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;
        ALOGE("ERR(%s):m_run fail", __FUNCTION__);
        /* TODO: doing exception handling */
        return false;
    }

    /* one time */
    return m_checkThreadLoop();
}

}; /* namespace android */
