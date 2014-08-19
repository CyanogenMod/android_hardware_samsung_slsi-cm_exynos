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
#define LOG_TAG "ExynosCamera"
#include <cutils/log.h>

#include "ExynosCamera.h"

namespace android {

ExynosCamera::ExynosCamera(int cameraId, camera_device_t *dev)
{
    BUILD_DATE();

    checkAndroidVersion();

    m_cameraId = cameraId;
    m_exynosCameraParameters = new ExynosCameraParameters(m_cameraId);
    ALOGD("DEBUG(%s):Parameters(Id=%d) created", __FUNCTION__, m_cameraId);

    m_exynosCameraActivityControl = m_exynosCameraParameters->getActivityControl();

    m_previewFrameFactory      = NULL;
    m_reprocessingFrameFactory = NULL;

    m_ionAllocator = NULL;
    m_grAllocator  = NULL;
    m_mhbAllocator = NULL;

    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");
    m_createInternalBufferManager(&m_3aaBufferMgr, "3A1_BUF");
    m_createInternalBufferManager(&m_ispBufferMgr, "ISP_BUF");

    /* reprocessing Buffer */
    m_createInternalBufferManager(&m_ispReprocessingBufferMgr, "ISP_RE_BUF");
    m_createInternalBufferManager(&m_sccReprocessingBufferMgr, "SCC_RE_BUF");

    m_createInternalBufferManager(&m_sccBufferMgr, "SCC_BUF");
    m_createInternalBufferManager(&m_gscBufferMgr, "GSC_BUF");
    m_createInternalBufferManager(&m_jpegBufferMgr, "JPEG_BUF");

    /* preview Buffer */
    m_scpBufferMgr = NULL;
    m_createInternalBufferManager(&m_previewCallbackBufferMgr, "PREVIEW_CB_BUF");
    m_createInternalBufferManager(&m_highResolutionCallbackBufferMgr, "HIGH_RESOLUTION_CB_BUF");

    /* recording Buffer */
    m_recordingCallbackHeap = NULL;
    m_createInternalBufferManager(&m_recordingBufferMgr, "REC_BUF");

    m_createThreads();

    m_pipeFrameDoneQ     = new frame_queue_t;
    dstIspReprocessingQ  = new frame_queue_t;
    dstSccReprocessingQ  = new frame_queue_t;
    dstGscReprocessingQ  = new frame_queue_t;

    dstJpegReprocessingQ = new frame_queue_t;

    m_previewQ     = new frame_queue_t(m_previewThread);
    m_previewFrontQ = new frame_queue_t(m_previewThread);
    m_recordingQ   = new frame_queue_t(m_recordingThread);
    m_postPictureQ = new frame_queue_t(m_postPictureThread);
    m_jpegCallbackQ = new jpeg_callback_queue_t;

    /* set the wait time to 2000ms */
    dstIspReprocessingQ->setWaitTime(2000000000);
    dstSccReprocessingQ->setWaitTime(50000000);
    dstGscReprocessingQ->setWaitTime(2000000000);
    dstJpegReprocessingQ->setWaitTime(2000000000);

    m_jpegCallbackQ->setWaitTime(1000000000);

    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));
    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

    m_exitAutoFocusThread = false;
    m_autoFocusRunning    = false;
    m_previewEnabled   = false;
    m_pictureEnabled   = false;
    m_recordingEnabled = false;
    m_zslPictureEnabled   = false;
    m_flagStartFaceDetection = false;
    m_captureSelector = NULL;
    m_sccCaptureSelector = NULL;
    m_autoFocusType = 0;
    m_hdrEnabled = false;
    m_doCscRecording = true;

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    m_stopBurstShot = false;
    m_burst[JPEG_SAVE_THREAD0] = false;
    m_burst[JPEG_SAVE_THREAD1] = false;
    m_burst[JPEG_SAVE_THREAD2] = false;

    m_running[JPEG_SAVE_THREAD0] = false;
    m_running[JPEG_SAVE_THREAD1] = false;
    m_running[JPEG_SAVE_THREAD2] = false;
    m_callbackState = 0;
    m_callbackStateOld = 0;
    m_callbackMonitorCount = 0;

    m_highResolutionCallbackRunning = false;
    m_highResolutionCallbackQ = new frame_queue_t(m_highResolutionCallbackThread);
    m_highResolutionCallbackQ->setWaitTime(50000000);
    m_skipReprocessing = false;
    m_isFirstStart = true;


    m_exynosconfig = NULL;
    m_setConfigInform();

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_dynamicSccCount = 0;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;
}

status_t  ExynosCamera::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.front_num_bayer_buffers = FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.front_num_picture_buffers = FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = 3;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = 3;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = 1;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = 3;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE_FRONT] = 3;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_FRONT] = 1;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_FRONT] = 3;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = 3;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = 1;
    if( getCameraId() == CAMERA_ID_BACK ) {
        /* Config HIGH_SPEED 60 buffer & pipe info */
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers = NUM_BAYER_BUFFERS + 5;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS * 2;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + 5;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers = NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS + 5;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.front_num_bayer_buffers = FRONT_NUM_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.front_num_picture_buffers = FRONT_NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = 3;
        /* Config HIGH_SPEED 120 buffer & pipe info */
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = NUM_BAYER_BUFFERS * 3;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS * 3;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS * 3;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS *3;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.front_num_bayer_buffers = FRONT_NUM_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.front_num_picture_buffers = FRONT_NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA_ISP] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = 3;
    } else {
        /* Config HIGH_SPEED 60 buffer & pipe info */
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers = NUM_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers = NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.front_num_bayer_buffers = FRONT_NUM_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.front_num_picture_buffers = FRONT_NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE_FRONT] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_FRONT] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = 3;
        /* Config HIGH_SPEED 120 buffer & pipe info */
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = NUM_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.front_num_bayer_buffers = FRONT_NUM_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.front_num_picture_buffers = FRONT_NUM_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA_ISP] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP] = exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = 3;
    }

     m_exynosCameraParameters->setConfig(&exynosConfig);

     m_exynosconfig = m_exynosCameraParameters->getConfig();

    return NO_ERROR;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainThread = new mainCameraThread(this, &ExynosCamera::m_mainThreadFunc, "ExynosCameraThread", PRIORITY_URGENT_DISPLAY);
    ALOGD("DEBUG(%s):mainThread created", __FUNCTION__);

    m_previewThread = new mainCameraThread(this, &ExynosCamera::m_previewThreadFunc, "previewThread", PRIORITY_DISPLAY);
    ALOGD("DEBUG(%s):previewThread created", __FUNCTION__);

    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera::m_setBuffersThreadFunc, "setBuffersThread");
    ALOGD("DEBUG(%s):setBuffersThread created", __FUNCTION__);

    m_startPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_startPictureInternalThreadFunc, "startPictureInternalThread");
    ALOGD("DEBUG(%s):startPictureInternalThread created", __FUNCTION__);

    m_prePictureThread = new mainCameraThread(this, &ExynosCamera::m_prePictureThreadFunc, "prePictureThread");
    ALOGD("DEBUG(%s):prePictureThread created", __FUNCTION__);

    m_pictureThread = new mainCameraThread(this, &ExynosCamera::m_pictureThreadFunc, "PictureThread");
    ALOGD("DEBUG(%s):pictureThread created", __FUNCTION__);

    m_postPictureThread = new mainCameraThread(this, &ExynosCamera::m_postPictureThreadFunc, "postPictureThread");
    ALOGD("DEBUG(%s):postPictureThread created", __FUNCTION__);

    m_recordingThread = new mainCameraThread(this, &ExynosCamera::m_recordingThreadFunc, "recordingThread");
    ALOGD("DEBUG(%s):recordingThread created", __FUNCTION__);

    m_autoFocusThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusThreadFunc, "AutoFocusThread");
    ALOGD("DEBUG(%s):autoFocusThread created", __FUNCTION__);

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "monitorThread");
    ALOGD("DEBUG(%s):monitorThread created", __FUNCTION__);

    m_jpegCallbackThread = new mainCameraThread(this, &ExynosCamera::m_jpegCallbackThreadFunc, "jpegCallbackThread");
    ALOGD("DEBUG(%s):jpegCallbackThread created", __FUNCTION__);

    /* high resolution preview callback Thread */
    m_highResolutionCallbackThread = new mainCameraThread(this, &ExynosCamera::m_highResolutionCallbackThreadFunc, "m_highResolutionCallbackThread");
    ALOGD("DEBUG(%s):highResolutionCallbackThread created", __FUNCTION__);
}

ExynosCamera::~ExynosCamera()
{
    this->release();
}

void ExynosCamera::release()
{
    ALOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_previewFrameFactory != NULL) {
        ret = m_previewFrameFactory->destroy();
        if (ret < 0)
            ALOGE("ERR(%s[%d]):mainCameraFrameFactory destroy fail", __FUNCTION__, __LINE__);

        delete m_previewFrameFactory;
        m_previewFrameFactory = NULL;
        ALOGD("DEBUG(%s):FrameFactory(previewFrameFactory) destroyed", __FUNCTION__);
    }

    if (m_reprocessingFrameFactory != NULL) {
        ret = m_reprocessingFrameFactory->destroy();
        if (ret < 0)
            ALOGE("ERR(%s[%d]):frameReprocessingFactory destroy fail", __FUNCTION__, __LINE__);
        delete m_reprocessingFrameFactory;
        m_reprocessingFrameFactory = NULL;
        ALOGD("DEBUG(%s):FrameFactory(reprocessingFrameFactory) destroyed", __FUNCTION__);
    }

    if (m_exynosCameraParameters != NULL) {
        delete m_exynosCameraParameters;
        m_exynosCameraParameters = NULL;
        ALOGD("DEBUG(%s):Parameters(Id=%d) destroyed", __FUNCTION__, m_cameraId);
    }

    /* free all buffers */
    m_releaseBuffers();

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    if (m_grAllocator != NULL) {
        delete m_grAllocator;
        m_grAllocator = NULL;
    }

    if (m_pipeFrameDoneQ != NULL) {
        delete m_pipeFrameDoneQ;
        m_pipeFrameDoneQ = NULL;
    }

    if (dstIspReprocessingQ != NULL) {
        delete dstIspReprocessingQ;
        dstIspReprocessingQ = NULL;
    }

    if (dstSccReprocessingQ != NULL) {
        delete dstSccReprocessingQ;
        dstSccReprocessingQ = NULL;
    }

    if (dstGscReprocessingQ != NULL) {
        delete dstGscReprocessingQ;
        dstGscReprocessingQ = NULL;
    }

    if (dstJpegReprocessingQ != NULL) {
        delete dstJpegReprocessingQ;
        dstJpegReprocessingQ = NULL;
    }

    if (m_postPictureQ != NULL) {
        delete m_postPictureQ;
        m_postPictureQ = NULL;
    }

    if (m_jpegCallbackQ != NULL) {
        delete m_jpegCallbackQ;
        m_jpegCallbackQ = NULL;
    }

    if (m_previewQ != NULL) {
        delete m_previewQ;
        m_previewQ = NULL;
    }

    if (m_previewFrontQ != NULL) {
        delete m_previewFrontQ;
        m_previewFrontQ = NULL;
    }

    if (m_recordingQ != NULL) {
        delete m_recordingQ;
        m_recordingQ = NULL;
    }

    if (m_highResolutionCallbackQ != NULL) {
        delete m_highResolutionCallbackQ;
        m_highResolutionCallbackQ = NULL;
    }

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(bayerBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_3aaBufferMgr != NULL) {
        delete m_3aaBufferMgr;
        m_3aaBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(3aaBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_ispBufferMgr != NULL) {
        delete m_ispBufferMgr;
        m_ispBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(ispBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_scpBufferMgr != NULL) {
        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(scpBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_ispReprocessingBufferMgr != NULL) {
        delete m_ispReprocessingBufferMgr;
        m_ispReprocessingBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(ispReprocessingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_sccReprocessingBufferMgr != NULL) {
        delete m_sccReprocessingBufferMgr;
        m_sccReprocessingBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(sccReprocessingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_sccBufferMgr != NULL) {
        delete m_sccBufferMgr;
        m_sccBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(sccBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_gscBufferMgr != NULL) {
        delete m_gscBufferMgr;
        m_gscBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(gscBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_jpegBufferMgr != NULL) {
        delete m_jpegBufferMgr;
        m_jpegBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(jpegBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_previewCallbackBufferMgr != NULL) {
        delete m_previewCallbackBufferMgr;
        m_previewCallbackBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(previewCallbackBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        delete m_highResolutionCallbackBufferMgr;
        m_highResolutionCallbackBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(m_highResolutionCallbackBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_recordingBufferMgr != NULL) {
        delete m_recordingBufferMgr;
        m_recordingBufferMgr = NULL;
        ALOGD("DEBUG(%s):BufferManager(recordingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_captureSelector != NULL) {
        delete m_captureSelector;
        m_captureSelector = NULL;
    }

    if (m_sccCaptureSelector != NULL) {
        delete m_sccCaptureSelector;
        m_sccCaptureSelector = NULL;
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        delete m_recordingCallbackHeap;
        m_recordingCallbackHeap = NULL;
        ALOGD("DEBUG(%s):BufferManager(recordingCallbackHeap) destroyed", __FUNCTION__);
    }

    m_isFirstStart = true;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    ALOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);
}

int ExynosCamera::getCameraId() const
{
    return m_cameraId;
}

bool ExynosCamera::isReprocessing(void) const
{
    int camId = getCameraId();
    bool reprocessing = false;

    if (camId == CAMERA_ID_BACK)
        reprocessing = MAIN_CAMERA_REPROCESSING;
    else
        reprocessing = FRONT_CAMERA_REPROCESSING;

    return reprocessing;
}

bool ExynosCamera::isSccCapture(void) const
{
    int camId = getCameraId();
    bool sccCapture = false;

    if (camId == CAMERA_ID_BACK)
        sccCapture = MAIN_CAMERA_SCC_CAPTURE;
    else
        sccCapture = FRONT_CAMERA_SCC_CAPTURE;

    return sccCapture;
}
status_t ExynosCamera::setPreviewWindow(preview_stream_ops *w)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    int width, height;
    int halPreviewFmt = 0;
    bool flagRestart = false;
    buffer_manager_type bufferType = BUFFER_MANAGER_ION_TYPE;

    if (previewEnabled() == true) {
        ALOGW("WRN(%s[%d]): preview is started, we forcely re-start preview", __FUNCTION__, __LINE__);
        flagRestart = true;
        stopPreview();
    }

    m_previewWindow = w;

    if (m_scpBufferMgr != NULL) {
        ALOGD("DEBUG(%s[%d]): scp buffer manager need recreate", __FUNCTION__, __LINE__);
        m_scpBufferMgr->deinit();

        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
    }

    if (w == NULL) {
        bufferType = BUFFER_MANAGER_ION_TYPE;
        ALOGW("WARN(%s[%d]):window NULL, create internal buffer for preview", __FUNCTION__, __LINE__);
    } else {
        halPreviewFmt = m_exynosCameraParameters->getHalPixelFormat();
        bufferType = BUFFER_MANAGER_GRALLOC_TYPE;
        m_exynosCameraParameters->getHwPreviewSize(&width, &height);

        if (m_grAllocator == NULL)
            m_grAllocator = new ExynosCameraGrallocAllocator();

        ret = m_grAllocator->init(m_previewWindow, m_exynosconfig->current->bufInfo.num_preview_buffers);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):gralloc init fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto func_exit;
        }

        ret = m_grAllocator->setBuffersGeometry(width, height, halPreviewFmt);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):gralloc setBufferGeomety fail, size(%dx%d), fmt(%d), ret(%d)",
                __FUNCTION__, __LINE__, width, height, halPreviewFmt, ret);
            goto func_exit;
        }
    }

    m_createBufferManager(&m_scpBufferMgr, "SCP_BUF", bufferType);

    if (bufferType == BUFFER_MANAGER_GRALLOC_TYPE)
        m_scpBufferMgr->setAllocator(m_grAllocator);

    if (flagRestart == true)
        startPreview();

func_exit:

    return ret;
}

void ExynosCamera::setCallbacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    ALOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    int ret = 0;

    m_notifyCb        = notify_cb;
    m_dataCb          = data_cb;
    m_dataCbTimestamp = data_cb_timestamp;
    m_getMemoryCb     = get_memory;
    m_callbackCookie  = user;

    if (m_mhbAllocator == NULL)
        m_mhbAllocator = new ExynosCameraMHBAllocator();

    ret = m_mhbAllocator->init(get_memory);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]:m_mhbAllocator init failed", __FUNCTION__, __LINE__);
    }
}

void ExynosCamera::enableMsgType(int32_t msgType)
{
    if (m_exynosCameraParameters) {
        ALOGV("INFO(%s[%d]): enable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_exynosCameraParameters->enableMsgType(msgType);
    }
}

void ExynosCamera::disableMsgType(int32_t msgType)
{
    if (m_exynosCameraParameters) {
        ALOGV("INFO(%s[%d]): disable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_exynosCameraParameters->disableMsgType(msgType);
    }
}

bool ExynosCamera::msgTypeEnabled(int32_t msgType)
{
    bool IsEnabled = false;

    if (m_exynosCameraParameters) {
        ALOGV("INFO(%s[%d]): Msg type enabled (%x)", __FUNCTION__, __LINE__, msgType);
        IsEnabled = m_exynosCameraParameters->msgTypeEnabled(msgType);
    }

    return IsEnabled;
}

status_t ExynosCamera::startPreview()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;

    struct camera2_shot_ext *initMetaData = NULL;

    m_hdrSkipedFcount = 0;
    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_curMinFps = 0;
    m_isNeedAllocPictureBuffer = false;

    if (m_previewEnabled == true) {
        return INVALID_OPERATION;
    }

    m_fdCallbackHeap = m_getMemoryCb(-1, sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES, 1, m_callbackCookie);

    {
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        if (m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        else
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);

        if ((m_exynosCameraParameters->getRestartPreview() == true) ||
            m_previewBufferCount != m_exynosCameraParameters->getPreviewBufferCount()) {
            ret = setPreviewWindow(m_previewWindow);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }

            ALOGE("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);
            if (ret < 0) {
                ALOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }

            m_previewBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
        }

        ALOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
        m_setBuffersThread->run(PRIORITY_DEFAULT);

        if (m_captureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;

            if (getCameraId() == CAMERA_ID_BACK)
                bufMgr = m_bayerBufferMgr;
            else
                bufMgr = m_sccBufferMgr;

            m_captureSelector = new ExynosCameraFrameSelector(m_exynosCameraParameters, bufMgr);

            if (isReprocessing() == true) {
                ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
                if (ret < 0)
                    ALOGE("ERR(%s[%d]): setFrameHoldCount(%d) is fail", __FUNCTION__, __LINE__, REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

        if (m_sccCaptureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;

            /* TODO: Dynamic select buffer manager for capture */
            bufMgr = m_sccBufferMgr;

            m_sccCaptureSelector = new ExynosCameraFrameSelector(m_exynosCameraParameters, bufMgr);
        }

        m_captureSelector->release();
        m_sccCaptureSelector->release();

        if (m_previewFrameFactory == NULL) {
            m_previewFrameFactory = ExynosCameraFrameFactory::createFrameFactory(m_cameraId, m_exynosCameraParameters);

            ret = m_previewFrameFactory->create();
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
            ALOGD("DEBUG(%s):FrameFactory(previewFrameFactory) created", __FUNCTION__);
        }

        if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
                m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK &&
                m_exynosCameraParameters->getDualMode() == false &&
                m_exynosCameraParameters->getRecordingHint() == false &&
                m_isFirstStart == true) {

            ret = m_fastenAeStable();
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
            } else {
                skipFrameCount = 0;
                m_exynosCameraParameters->setUseFastenAeStable(false);
            }
        }

        initMetaData = new struct camera2_shot_ext;
        if (initMetaData != NULL) {
            m_exynosCameraParameters->duplicateCtrlMetadata(initMetaData);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[0], PIPE_FLITE);
            if (ret < 0)
                ALOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[1], PIPE_FLITE);
            if (ret < 0)
                ALOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_SCENE_MODE, initMetaData->shot.ctl.aa.sceneMode, PIPE_FLITE);
            if (ret < 0)
                ALOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            delete initMetaData;
            initMetaData = NULL;
        } else {
            ALOGE("ERR(%s[%d]):initMetaData is NULL", __FUNCTION__, __LINE__);
        }

        m_exynosCameraParameters->setFrameSkipCount(skipFrameCount);

        m_setBuffersThread->join();

        ret = m_startPreviewInternal();
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_startPreviewInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        if (isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
            m_startPictureInternalThread->run(PRIORITY_DEFAULT);
#endif
        } else {
            m_pictureFrameFactory = m_previewFrameFactory;
            ALOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        }

        if (m_previewWindow != NULL)
            m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

        m_mainThread->run(PRIORITY_DEFAULT);
        m_monitorThread->run(PRIORITY_DEFAULT);

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == false)) {
            ALOGD("DEBUG(%s[%d]):High resolution preview callback start", __FUNCTION__, __LINE__);
            if (skipFrameCount > 0)
                m_skipReprocessing = true;
            m_highResolutionCallbackRunning = true;
            m_startPictureInternalThread->run(PRIORITY_DEFAULT);

            m_startPictureInternalThread->join();
            m_prePictureThread->run(PRIORITY_DEFAULT);
        }

        /* FD-AE is always on */
#ifdef USE_FD_AE
        if ((m_exynosCameraParameters->getDualMode() == true && m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK) ||
                (m_exynosCameraParameters->getRecordingHint() == true)) {
            m_startFaceDetection(false);
        } else {
            m_startFaceDetection(true);
        }
#endif

        if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
                m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK &&
                m_exynosCameraParameters->getDualMode() == false &&
                m_exynosCameraParameters->getRecordingHint() == false &&
                m_isFirstStart == true) {
            /* AF mode is setted as INFINITY in fastenAE, and we should update that mode */
            m_exynosCameraActivityControl->setAutoFocusMode(FOCUS_MODE_INFINITY);

            m_exynosCameraParameters->setUseFastenAeStable(false);
            m_exynosCameraActivityControl->setAutoFocusMode(m_exynosCameraParameters->getFocusMode());
            m_isFirstStart = false;
        }
    }

    return NO_ERROR;

err:
    m_setBuffersThread->join();

    m_releaseBuffers();

    return ret;
}

void ExynosCamera::stopPreview()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    if (m_previewEnabled == false) {
        ALOGD("DEBUG(%s[%d]): preview is not enabled", __FUNCTION__, __LINE__);
        return;
    }

    {
        m_previewFrameFactory->setStopFlag();
        m_exynosCameraActivityControl->cancelAutoFocus();

        m_takePictureCounter.clearCount();
        m_reprocessingCounter.clearCount();
        m_pictureCounter.clearCount();
        m_jpegCounter.clearCount();
        m_captureSelector->cancelPicture();

        ALOGD("DEBUG(%s[%d]): (%d, %d)", __FUNCTION__, __LINE__, m_flashMgr->getNeedCaptureFlash(), m_pictureEnabled);
        if (m_flashMgr->getNeedCaptureFlash() == true && m_pictureEnabled == true) {
            ALOGE("DEBUG(%s[%d]): force flash off", __FUNCTION__, __LINE__);
            m_exynosCameraActivityControl->cancelFlash();
            autoFocusMgr->stopAutofocus();
            m_isTryStopFlash = true;
            m_exitAutoFocusThread = true;

            usleep(150000);

            m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
        }

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
            m_skipReprocessing = false;
            m_highResolutionCallbackRunning = false;
            ALOGD("DEBUG(%s[%d]):High resolution preview callback stop", __FUNCTION__, __LINE__);

            m_prePictureThread->requestExitAndWait();
            m_highResolutionCallbackQ->release();
        }

        m_startPictureInternalThread->join();
        ret = m_stopPictureInternal();
        if (ret < 0)
            ALOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

        m_exynosCameraActivityControl->stopAutoFocus();
        m_autoFocusThread->requestExitAndWait();

        m_previewThread->requestExitAndWait();

        if (m_previewQ != NULL) {
            m_previewQ->release();
        }

        if (m_previewFrontQ != NULL) {
            m_previewFrontQ->release();
        }

        m_mainThread->requestExitAndWait();
        m_monitorThread->requestExitAndWait();

        ret = m_stopPreviewInternal();
        if (ret < 0)
            ALOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
    }

    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->resetBuffers();
    }

    /* realloc reprocessing buffer for change burst panorama <-> normal mode */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->deinit();
    }

    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }
    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

#if 0
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    ALOGE(" m_setBuffers free all buffers");
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
#endif
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();

    m_hdrSkipedFcount = 0;
    m_dynamicSccCount = 0;

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_isNeedAllocPictureBuffer = false;

    m_fdCallbackHeap->release(m_fdCallbackHeap);
}

bool ExynosCamera::previewEnabled()
{
    ALOGI("INFO(%s[%d]):m_previewEnabled=%d",
        __FUNCTION__, __LINE__, (int)m_previewEnabled);

    /* in scalable mode, we should controll out state */
    if (m_exynosCameraParameters != NULL &&
        (m_exynosCameraParameters->getScalableSensorMode() == true) &&
        (m_scalableSensorMgr.getMode() == EXYNOS_CAMERA_SCALABLE_CHANGING))
        return true;
    else
        return m_previewEnabled;
}

status_t ExynosCamera::storeMetaDataInBuffers(bool enable)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return OK;
}

status_t ExynosCamera::startRecording()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    m_recordingStateLock.lock();
    if (m_recordingEnabled == true) {
        m_recordingStateLock.unlock();
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    m_recordingStateLock.unlock();

#ifdef USE_FD_AE
    m_startFaceDetection(false);
#endif


    /* Do start recording process */
    ret = m_startRecordingInternal();
    if (ret < 0) {
        ALOGE("ERR");
        return ret;
    }

    m_lastRecordingTimeStamp  = 0;
    m_recordingStartTimeStamp = 0;
    m_recordingFrameSkipCount = 0;

    m_recordingStateLock.lock();
    m_recordingEnabled = true;
    m_recordingStateLock.unlock();

    autoFocusMgr->setRecordingHint(true);

func_exit:

    return NO_ERROR;
}

void ExynosCamera::stopRecording()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    m_recordingStateLock.lock();
    if (m_recordingEnabled == false) {
        m_recordingStateLock.unlock();
        return;
    }

    m_recordingEnabled = false;
    m_recordingStateLock.unlock();

    /* Do stop recording process */

    ret = m_stopRecordingInternal();
    if (ret < 0)
        ALOGE("ERR(%s[%d]):m_stopRecordingInternal fail", __FUNCTION__, __LINE__);

#ifdef USE_FD_AE
    if ((m_exynosCameraParameters->getDualMode() == true) &&
        (m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK)) {
        m_startFaceDetection(false);
    } else {
        m_startFaceDetection(true);
    }
#endif

    autoFocusMgr->setRecordingHint(false);
}

bool ExynosCamera::recordingEnabled()
{
    ALOGI("INFO(%s[%d]):m_recordingEnabled=%d",
        __FUNCTION__, __LINE__, (int)m_recordingEnabled);

    return m_recordingEnabled;
}

void ExynosCamera::releaseRecordingFrame(const void *opaque)
{
    m_recordingStateLock.lock();
    if (m_recordingEnabled == false) {
        m_recordingStateLock.unlock();
        return;
    }
    m_recordingStateLock.unlock();

    if (m_recordingCallbackHeap == NULL) {
        ALOGW("WARN(%s[%d]):recordingCallbackHeap equals NULL", __FUNCTION__, __LINE__);
        return;
    }

    bool found = false;
    struct addrs *recordAddrs  = (struct addrs *)m_recordingCallbackHeap->data;
    struct addrs *releaseFrame = (struct addrs *)opaque;

    if (recordAddrs != NULL) {
        for (uint32_t i = 0; i < m_exynosconfig->current->bufInfo.num_recording_buffers; i++) {
            if ((char *)(&(recordAddrs[i])) == (char *)opaque) {
                found = true;
                ALOGV("DEBUG(%s[%d]):releaseFrame->bufIndex=%d", __FUNCTION__, __LINE__, releaseFrame->bufIndex);

                if (m_doCscRecording == true)
                    m_releaseRecordingBuffer(releaseFrame->bufIndex);

                break;
            }
            m_isFirstStart = false;
        }
    }

    if (found == false)
        ALOGW("WARN(%s[%d]):**** releaseFrame not founded ****", __FUNCTION__, __LINE__);

}

status_t ExynosCamera::autoFocus()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    /* waiting previous AF is over */
    m_autoFocusLock.lock();

    m_autoFocusType = AUTO_FOCUS_SERVICE;
    m_autoFocusThread->requestExitAndWait();

#if 0
    if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_AUTO) {
        ALOGI("INFO(%s[%d]) ae awb lock", __FUNCTION__, __LINE__);
        m_exynosCameraParameters->m_setAutoExposureLock(true);
        m_exynosCameraParameters->m_setAutoWhiteBalanceLock(true);
    }
#endif

    m_autoFocusThread->run(PRIORITY_DEFAULT);

    return NO_ERROR;
}

status_t ExynosCamera::cancelAutoFocus()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    m_autoFocusRunning = false;

#if 0
    if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_AUTO) {
        ALOGI("INFO(%s[%d]) ae awb unlock", __FUNCTION__, __LINE__);
        m_exynosCameraParameters->m_setAutoExposureLock(false);
        m_exynosCameraParameters->m_setAutoWhiteBalanceLock(false);
    }
#endif

    if (m_exynosCameraActivityControl->cancelAutoFocus() == false) {
        ALOGE("ERR(%s):Fail on m_secCamera->cancelAutoFocus()", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ExynosCamera::takePicture()
{
    int ret = 0;
    int takePictureCount = m_takePictureCounter.getCount();
    int seriesShotCount = m_exynosCameraParameters->getSeriesShotCount();
    int currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();
    ExynosCameraFrame *newFrame = NULL;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();

    int retryCount = 0;

    /* wait autoFocus is over for turning on preFlash */
    m_autoFocusThread->join();

    /* HACK Reset Preview Flag*/
    while ((m_resetPreview == true) && (retryCount < 10)) {
        usleep(200000);
        retryCount ++;
        ALOGI("INFO(%s[%d]) retryCount(%d) m_resetPreview(%d)", __FUNCTION__, __LINE__, retryCount, m_resetPreview);
    }

    if (takePictureCount < 0) {
        ALOGE("ERR(%s[%d]): takePicture is called too much. takePictureCount(%d) / seriesShotCount(%d) . so, fail",
            __FUNCTION__, __LINE__, takePictureCount, seriesShotCount);
        return INVALID_OPERATION;
    } else if (takePictureCount == 0) {
        if (seriesShotCount == 0) {
            m_captureLock.lock();
            if (m_pictureEnabled == true) {
                ALOGE("ERR(%s[%d]): take picture is inprogressing", __FUNCTION__, __LINE__);
                /* return NO_ERROR; */
                m_captureLock.unlock();
                return INVALID_OPERATION;
            }
            m_captureLock.unlock();

            /* general shot */
            seriesShotCount = 1;
        }
        if (currentSeriesShotMode != SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode != SERIES_SHOT_MODE_SIS)
            m_takePictureCounter.setCount(seriesShotCount);
        else
            m_takePictureCounter.setCount(1);
    }

    ALOGI("INFO(%s[%d]): takePicture start m_takePictureCounter(%d), seriesShotCount(%d)",
        __FUNCTION__, __LINE__, m_takePictureCounter.getCount(), seriesShotCount);

    if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        m_hdrEnabled = true;
    } else {
        m_hdrEnabled = false;
    }

    /* TODO: Dynamic bayer capture, currently support only single shot */
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequestFLITE(true);
            ret = generateFrame(-1, &newFrame);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }
            m_previewFrameFactory->setRequestFLITE(false);

            ret = m_setupEntity(pipeId, newFrame);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
    }

    if (m_takePictureCounter.getCount() == seriesShotCount) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();

        m_stopBurstShot = false;

#if 0
        if (isReprocessing() == false || m_exynosCameraParameters->getSeriesShotCount() > 0 ||
                m_hdrEnabled == true) {
            m_pictureFrameFactory = m_previewFrameFactory;
            if (m_exynosCameraParameters->getUseDynamicScc() == true) {
                m_previewFrameFactory->setRequestSCC(true);

                /* boosting dynamic SCC */
                if (m_hdrEnabled == false &&
                    currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
                    ret = m_boostDynamicCapture();
                    if (ret < 0)
                        ALOGW("WRN(%s[%d]): fail to boosting dynamic capture", __FUNCTION__, __LINE__);
                }

            }
        } else {
            m_pictureFrameFactory = m_reprocessingFrameFactory;

        }
#endif

        if (m_exynosCameraParameters->getScalableSensorMode()) {
            m_exynosCameraParameters->setScalableSensorMode(false);
            stopPreview();
            setPreviewWindow(m_previewWindow);
            startPreview();
            m_exynosCameraParameters->setScalableSensorMode(true);
        }

        ALOGI("INFO(%s[%d]): takePicture enabled, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_pictureEnabled = true;
        m_takePictureCounter.decCount();
        m_isNeedAllocPictureBuffer = true;

        if (isReprocessing() == true) {
            m_startPictureInternalThread->join();
        }

        if (m_hdrEnabled == true) {
            seriesShotCount = HDR_REPROCESSING_COUNT;
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            m_sCaptureMgr->resetHdrStartFcount();
            m_exynosCameraParameters->setFrameSkipCount(13);
        } else if (m_flashMgr->getNeedCaptureFlash() == true &&
                        currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            if (m_flashMgr->checkPreFlash() == false && m_isTryStopFlash == false) {
                m_flashMgr->setCaptureStatus(true);
                ALOGD("DEBUG(%s[%d]): checkPreFlash(false), Start auto focus internally", __FUNCTION__, __LINE__);
                m_autoFocusType = AUTO_FOCUS_HAL;
                m_flashMgr->setFlashTrigerPath(ExynosCameraActivityFlash::FLASH_TRIGGER_SHORT_BUTTON);

                /* execute autoFocus for preFlash */
                m_autoFocusThread->requestExitAndWait();
                m_autoFocusThread->run(PRIORITY_DEFAULT);
            }
        }

        m_reprocessingCounter.setCount(seriesShotCount);
        if (m_prePictureThread->isRunning() == false) {
            if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                ALOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }
        }

        m_jpegCounter.setCount(seriesShotCount);
        m_pictureCounter.setCount(seriesShotCount);
        if (m_pictureThread->isRunning() == false)
            ret = m_pictureThread->run();
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
            return INVALID_OPERATION;
        }

        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS)) {
            m_jpegCallbackThread->join();
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
    } else {
        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS)) {
            /* series shot : push buffer to callback thread. */
            m_jpegCallbackThread->join();
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
        ALOGI("INFO(%s[%d]): series shot takePicture, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_takePictureCounter.decCount();

        /* TODO : in case of no reprocesssing, we make zsl scheme or increse buf */
        if (isReprocessing() == false)
            m_pictureEnabled = true;
    }

    return NO_ERROR;
}

status_t ExynosCamera::cancelPicture()
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

/*
    m_takePictureCounter.clearCount();
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();
    m_jpegCounter.clearCount();
 */

    return NO_ERROR;
}

status_t ExynosCamera::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

#ifdef SCALABLE_ON
    m_exynosCameraParameters->setScalableSensorMode(true);
#else
    m_exynosCameraParameters->setScalableSensorMode(false);
#endif

    ret = m_exynosCameraParameters->setParameters(params);
#if 1
    /* HACK Reset Preview Flag*/
    if (m_exynosCameraParameters->getRestartPreview() == true && m_previewEnabled == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        ALOGI("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);

        if (ret < 0)
            ALOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
    }
#endif
    return ret;

}

CameraParameters ExynosCamera::getParameters() const
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return m_exynosCameraParameters->getParameters();
}

int ExynosCamera::getMaxNumDetectedFaces(void)
{
    return m_exynosCameraParameters->getMaxNumDetectedFaces();
}

bool ExynosCamera::m_startFaceDetection(bool toggle)
{
    ALOGD("DEBUG(%s[%d]) toggle : %d", __FUNCTION__, __LINE__, toggle);

    if (toggle == true) {
        m_exynosCameraParameters->setFdEnable(true);
        m_exynosCameraParameters->setFdMode(FACEDETECT_MODE_FULL);
    } else {
        m_exynosCameraParameters->setFdEnable(false);
        m_exynosCameraParameters->setFdMode(FACEDETECT_MODE_OFF);
    }

    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));

    return true;
}

bool ExynosCamera::startFaceDetection(void)
{
    if (m_flagStartFaceDetection == true) {
        ALOGD("DEBUG(%s):Face detection already started..", __FUNCTION__);
        return true;
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
#else
    m_startFaceDetection(true);
#endif
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    if (autoFocusMgr->setFaceDetection(true) == false) {
        ALOGE("ERR(%s[%d]):setFaceDetection(%d)", __FUNCTION__, __LINE__, true);
    } else {
        /* restart CAF when FD mode changed */
        switch (autoFocusMgr->getAutofocusMode()) {
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
            if (autoFocusMgr->flagAutofocusStart() == true &&
                autoFocusMgr->flagLockAutofocus() == false) {
                autoFocusMgr->stopAutofocus();
                autoFocusMgr->startAutofocus();
            }
            break;
        default:
            break;
        }
    }

    m_flagStartFaceDetection = true;

    return true;
}

bool ExynosCamera::stopFaceDetection(void)
{
    if (m_flagStartFaceDetection == false) {
        ALOGD("DEBUG(%s [%d]):Face detection already stopped..", __FUNCTION__, __LINE__);
        return true;
    }

    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    if (autoFocusMgr->setFaceDetection(false) == false) {
        ALOGE("ERR(%s[%d]):setFaceDetection(%d)", __FUNCTION__, __LINE__, false);
    } else {
        /* restart CAF when FD mode changed */
        switch (autoFocusMgr->getAutofocusMode()) {
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
            if (autoFocusMgr->flagAutofocusStart() == true &&
                autoFocusMgr->flagLockAutofocus() == false) {
                autoFocusMgr->stopAutofocus();
                autoFocusMgr->startAutofocus();
            }
            break;
        default:
            break;
        }
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
#else
    m_startFaceDetection(false);
#endif
    m_flagStartFaceDetection = false;

    return true;
}

int ExynosCamera::m_calibratePosition(int w, int new_w, int pos)
{
    return (float)(pos * new_w) / (float)w;
}

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrame *frame)
{
    struct camera2_shot_ext *meta_shot_ext = new struct camera2_shot_ext;
    memset(meta_shot_ext, 0x00, sizeof(struct camera2_shot_ext));
    frame->getDynamicMeta(meta_shot_ext);

    if (m_flagStartFaceDetection == true) {
        int id[NUM_OF_DETECTED_FACES];
        int score[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedLeftEye[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedRightEye[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedMouth[NUM_OF_DETECTED_FACES];
        int numOfDetectedFaces = 0;
        int num = 0;
        struct camera2_dm *dm = NULL;
        int previewW, previewH;

        memset(&id, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);
        memset(&score, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);

        m_exynosCameraParameters->getHwPreviewSize(&previewW, &previewH);

        dm = &(meta_shot_ext->shot.dm);
        if (dm == NULL) {
            ALOGE("ERR(%s[%d]) dm data is null", __FUNCTION__, __LINE__);
            delete meta_shot_ext;
            meta_shot_ext = NULL;
            return INVALID_OPERATION;
        }

        ALOGV("DEBUG(%s[%d]) faceDetectMode(%d)", __FUNCTION__, __LINE__, dm->stats.faceDetectMode);
        ALOGV("[%d %d]", dm->stats.faceRectangles[0][0], dm->stats.faceRectangles[0][1]);
        ALOGV("[%d %d]", dm->stats.faceRectangles[0][2], dm->stats.faceRectangles[0][3]);

       num = NUM_OF_DETECTED_FACES;
        if (getMaxNumDetectedFaces() < num)
            num = getMaxNumDetectedFaces();

        switch (dm->stats.faceDetectMode) {
        case FACEDETECT_MODE_SIMPLE:
        case FACEDETECT_MODE_FULL:
            break;
        case FACEDETECT_MODE_OFF:
        default:
            num = 0;
            break;
        }

        for (int i = 0; i < num; i++) {
            if (dm->stats.faceIds[i]) {
                switch (dm->stats.faceDetectMode) {
                case FACEDETECT_MODE_OFF:
                    break;
                case FACEDETECT_MODE_SIMPLE:
                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                    numOfDetectedFaces++;
                    break;
                case FACEDETECT_MODE_FULL:
                    id[i] = dm->stats.faceIds[i];
                    score[i] = dm->stats.faceScores[i];

                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];

                    detectedLeftEye[i].x1
                        = detectedLeftEye[i].y1
                        = detectedLeftEye[i].x2
                        = detectedLeftEye[i].y2 = -1;

                    detectedRightEye[i].x1
                        = detectedRightEye[i].y1
                        = detectedRightEye[i].x2
                        = detectedRightEye[i].y2 = -1;

                    detectedMouth[i].x1
                        = detectedMouth[i].y1
                        = detectedMouth[i].x2
                        = detectedMouth[i].y2 = -1;

                    numOfDetectedFaces++;
                    break;
                default:
                    break;
                }
            }
        }

        if (0 < numOfDetectedFaces) {
            /*
             * camera.h
             * width   : -1000~1000
             * height  : -1000~1000
             * if eye, mouth is not detectable : -2000, -2000.
             */
            memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

            int realNumOfDetectedFaces = 0;

            for (int i = 0; i < numOfDetectedFaces; i++) {
                /*
                 * over 50s, we will catch
                 * if (score[i] < 50)
                 *    continue;
                */
                m_faces[realNumOfDetectedFaces].rect[0] = m_calibratePosition(previewW, 2000, detectedFace[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].rect[1] = m_calibratePosition(previewH, 2000, detectedFace[i].y1) - 1000;
                m_faces[realNumOfDetectedFaces].rect[2] = m_calibratePosition(previewW, 2000, detectedFace[i].x2) - 1000;
                m_faces[realNumOfDetectedFaces].rect[3] = m_calibratePosition(previewH, 2000, detectedFace[i].y2) - 1000;

                m_faces[realNumOfDetectedFaces].id = id[i];
                m_faces[realNumOfDetectedFaces].score = score[i] > 100 ? 100 : score[i];

                m_faces[realNumOfDetectedFaces].left_eye[0] = (detectedLeftEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedLeftEye[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].left_eye[1] = (detectedLeftEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedLeftEye[i].y1) - 1000;

                m_faces[realNumOfDetectedFaces].right_eye[0] = (detectedRightEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedRightEye[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].right_eye[1] = (detectedRightEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedRightEye[i].y1) - 1000;

                m_faces[realNumOfDetectedFaces].mouth[0] = (detectedMouth[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedMouth[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].mouth[1] = (detectedMouth[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedMouth[i].y1) - 1000;

                ALOGV("face posision(cal:%d,%d %dx%d)(org:%d,%d %dx%d), id(%d), score(%d)",
                    m_faces[realNumOfDetectedFaces].rect[0], m_faces[realNumOfDetectedFaces].rect[1],
                    m_faces[realNumOfDetectedFaces].rect[2], m_faces[realNumOfDetectedFaces].rect[3],
                    detectedFace[i].x1, detectedFace[i].y1,
                    detectedFace[i].x2, detectedFace[i].y2,
                    m_faces[realNumOfDetectedFaces].id,
                    m_faces[realNumOfDetectedFaces].score);

                ALOGV("DEBUG(%s[%d]): left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
                        __FUNCTION__, __LINE__,
                        m_faces[realNumOfDetectedFaces].left_eye[0],
                        m_faces[realNumOfDetectedFaces].left_eye[1],
                        m_faces[realNumOfDetectedFaces].right_eye[0],
                        m_faces[realNumOfDetectedFaces].right_eye[1],
                        m_faces[realNumOfDetectedFaces].mouth[0],
                        m_faces[realNumOfDetectedFaces].mouth[1],
                        realNumOfDetectedFaces
                     );

                realNumOfDetectedFaces++;
            }

            m_frameMetadata.number_of_faces = realNumOfDetectedFaces;
            m_frameMetadata.faces = m_faces;

            m_faceDetected = true;
        } else {
            if (m_faceDetected == true && m_fdThreshold < NUM_OF_DETECTED_FACES_THRESHOLD) {
                /* waiting the unexpected fail about face detected */
                m_fdThreshold++;
            } else {
                if (0 < m_frameMetadata.number_of_faces)
                    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

                m_frameMetadata.number_of_faces = 0;
                m_frameMetadata.faces = m_faces;
                m_fdThreshold = 0;
                m_faceDetected = false;
            }
        }
    } else {
        if (0 < m_frameMetadata.number_of_faces)
            memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

        m_frameMetadata.number_of_faces = 0;
        m_frameMetadata.faces = m_faces;

        m_fdThreshold = 0;

        m_faceDetected = false;
    }

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA)) {
        int bufIndex = -2;
        int hwPreviewW = 0, hwPreviewH = 0;
        int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();

        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
        m_dataCb(CAMERA_MSG_PREVIEW_METADATA, m_fdCallbackHeap, 0, &m_frameMetadata, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
    }

    if (meta_shot_ext != NULL) {
        delete meta_shot_ext;
        meta_shot_ext = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCamera::sendCommand(int32_t command, int32_t arg1, int32_t arg2)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;

    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    /* TO DO : implemented based on the command */
    switch(command) {
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        if (getMaxNumDetectedFaces() == 0) {
            ALOGE("ERR(%s):getMaxNumDetectedFaces == 0", __FUNCTION__);
            return BAD_VALUE;
        }

        if (arg1 == CAMERA_FACE_DETECTION_SW) {
            ALOGE("ERR(%s):only support HW face dectection", __FUNCTION__);
            return BAD_VALUE;
        }

        if (command == CAMERA_CMD_START_FACE_DETECTION) {
            ALOGD("sendCommand: CAMERA_CMD_START_FACE_DETECTION is called!");
            if (m_flagStartFaceDetection == false
                && startFaceDetection() == false) {
                ALOGE("ERR(%s):startFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        } else {
            ALOGD("sendCommand: CAMERA_CMD_STOP_FACE_DETECTION is called!");
            if ( m_flagStartFaceDetection == true
                && stopFaceDetection() == false) {
                ALOGE("ERR(%s):stopFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        }
        break;
    case 1351: /*CAMERA_CMD_AUTO_LOW_LIGHT_SET */
        ALOGE("sendCommand: CAMERA_CMD_AUTO_LOW_LIGHT_SET is called!");
        break;
    /* 1510: CAMERA_CMD_SET_FLIP */
    case 1510 :
        ALOGD("DEBUG(%s):CAMERA_CMD_SET_FLIP is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setFlipHorizontal(arg1);
        break;
    /* 1521: CAMERA_CMD_DEVICE_ORIENTATION */
    case 1521:
        ALOGD("DEBUG(%s):CAMERA_CMD_DEVICE_ORIENTATION is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setDeviceOrientation(arg1);
        uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
        if (uctlMgr != NULL)
            uctlMgr->setDeviceRotation(m_exynosCameraParameters->getFdOrientation());
        break;
    /*1642: CAMERA_CMD_AUTOFOCUS_MACRO_POSITION*/
    case 1642:
        ALOGD("DEBUG(%s):CAMERA_CMD_AUTOFOCUS_MACRO_POSITION is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setAutoFocusMacroPosition(arg1);
        break;
    default:
        ALOGV("DEBUG(%s):unexpectect command(%d)", __FUNCTION__, command);
        break;
    }

    return NO_ERROR;
}

status_t ExynosCamera::generateFrame(int32_t frameCount, ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    *newFrame = NULL;

    if (frameCount >= 0) {
        ret = m_searchFrameFromList(&m_processList, frameCount, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    if (*newFrame == NULL) {
        *newFrame = m_previewFrameFactory->createNewFrame();

        if (*newFrame == NULL) {
            ALOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        if ((*newFrame)->getRequest(PIPE_SCC) == true) {
            m_dynamicSccCount++;
            ALOGV("DEBUG(%s[%d]):dynamicSccCount inc(%d) frameCount(%d)", __FUNCTION__, __LINE__, m_dynamicSccCount, (*newFrame)->getFrameCount());
        }

        m_processList.push_back(*newFrame);
    }

    return ret;
}

status_t ExynosCamera::m_setupEntity(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *srcBuf,
        ExynosCameraBuffer *dstBuf)
{
    int ret = 0;
    entity_buffer_state_t entityBufferState;

    /* set SRC buffer */
    ret = newFrame->getSrcBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setSrcBuffer(pipeId, newFrame, srcBuf);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
    }

    /* set DST buffer */
    ret = newFrame->getDstBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setDstBuffer(pipeId, newFrame, dstBuf);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_PROCESSING);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setSrcBuffer(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *buffer)
{
    int ret = 0;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuf;

    if (buffer == NULL) {
        buffer = &srcBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            ALOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setDstBuffer(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *buffer)
{
    int ret = 0;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer dstBuf;

    if (buffer == NULL) {
        buffer = &dstBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            ALOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            ExynosCameraFrameEntity *curEntity = newFrame->searchEntityByPipeId(pipeId);
            if (curEntity->getBufType() == ENTITY_BUFFER_DELIVERY) {
                ALOGV("DEBUG(%s[%d]): pipe(%d) buffer is empty for delivery", __FUNCTION__, __LINE__, pipeId);
                buffer->index = -1;
            } else {
                ALOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
                return ret;
            }
        }
    }

    /* set buffers */
    {
        ret = newFrame->setDstBuffer(pipeId, *buffer);
    }
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::generateFrameReprocessing(ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    struct ExynosCameraBuffer tempBuffer;
    int bufIndex = -1;

     /* 1. Make Frame */
    *newFrame = m_reprocessingFrameFactory->createNewFrame();
    if (*newFrame == NULL) {
        ALOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_fastenAeStable(void)
{
    int ret = 0;
    ExynosCameraBuffer fastenAeBuffer[INITIAL_SKIP_FRAME];
    ExynosCameraBufferManager *skipBufferMgr = NULL;
    m_createInternalBufferManager(&skipBufferMgr, "SKIP_BUF");

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    planeSize[0] = 32 * 64 * 2;
    int planeCount  = 2;
    int bufferCount = NUM_FASTAESTABLE_BUFFER;

    if (skipBufferMgr == NULL) {
        ALOGE("ERR(%s[%d]):createBufferManager failed", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_allocBuffers(skipBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, true, false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    for (int i = 0; i < bufferCount; i++) {
        int index = i;
        ret = skipBufferMgr->getBuffer(&index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &fastenAeBuffer[i]);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getBuffer fail", __FUNCTION__, __LINE__);
            goto done;
        }
    }

    ret = m_previewFrameFactory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret < 0) {
        // doing some error handling
    }

done:
    skipBufferMgr->deinit();
    delete skipBufferMgr;
    skipBufferMgr = NULL;

func_exit:
    return ret;
}

status_t ExynosCamera::m_startPreviewInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    uint32_t minFrameNum = 0;
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer dstBuf;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_displayPreviewToggle = 0;

    minFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    switch (reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
            ALOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            ALOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(true);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
            ALOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(true);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            ALOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
            ALOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        default :
            ALOGE("ERR(%s[%d]): Unknown dynamic bayer mode", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequest3AC(false);
            break;
    }

    ret = m_previewFrameFactory->initPipes();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_previewFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    for (uint32_t i = 0; i < minFrameNum; i++) {
        ret = generateFrame(i, &newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            ALOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_fliteFrameCount++;
        m_3aa_ispFrameCount++;
        m_sccFrameCount++;
        m_scpFrameCount++;

        if (getCameraId() == CAMERA_ID_BACK) {
            if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
                m_setupEntity(m_getBayerPipeId(), newFrame);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, m_getBayerPipeId());
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, m_getBayerPipeId());
            }

            m_setupEntity(PIPE_3AA_ISP, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA_ISP);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA_ISP);

            m_setupEntity(PIPE_SCP, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        } else {
            m_setupEntity(PIPE_FLITE_FRONT, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE_FRONT);

            m_setupEntity(PIPE_3AA_FRONT, newFrame);
            m_setupEntity(PIPE_ISP_FRONT, newFrame);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP_FRONT);

            m_setupEntity(PIPE_SCC_FRONT, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCC_FRONT);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCC_FRONT);

            m_setupEntity(PIPE_SCP_FRONT, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP_FRONT);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP_FRONT);
        }
    }

    /* prepare pipes */
    ret = m_previewFrameFactory->preparePipes();
    if (ret < 0) {
        ALOGE("ERR(%s):preparePipe fail", __FUNCTION__);
        return ret;
    }

#ifndef START_PICTURE_THREAD
    if (isReprocessing() == true) {
        m_startPictureInternal();
    }
#endif

    /* stream on pipes */
    ret = m_previewFrameFactory->startPipes();
    if (ret < 0) {
        ALOGE("ERR(%s):startPipe fail", __FUNCTION__);
        return ret;
    }

    /* start all thread */
    ret = m_previewFrameFactory->startInitialThreads();
    if (ret < 0) {
        ALOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        return ret;
    }

    m_previewEnabled = true;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    if (m_exynosCameraParameters->getFocusModeSetting() == true) {
        ALOGD("set Focus Mode(%s[%d])", __FUNCTION__, __LINE__);
        int focusmode = m_exynosCameraParameters->getFocusMode();
        m_exynosCameraActivityControl->setAutoFocusMode(focusmode);
        m_exynosCameraParameters->setFocusModeSetting(false);
    }

    ALOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPreviewInternal(void)
{
    int ret = 0;

    ALOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    ret = m_previewFrameFactory->stopPipes();
    if (ret < 0) {
        ALOGE("ERR(%s):stopPipe fail", __FUNCTION__);
        return ret;
    }

    ALOGD("DEBUG(%s[%d]):clear process Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_processList);
    if (ret < 0) {
        ALOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_pipeFrameDoneQ->release();

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;

    m_previewEnabled = false;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    ALOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_startPictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPictureW, hwPictureH;
    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_zslPictureEnabled == true) {
        ALOGD("DEBUG(%s[%d]): zsl picture is already initialized", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        if( m_exynosCameraParameters->getHighSpeedRecording() ) {
            m_exynosCameraParameters->getHwSensorSize(&hwPictureW, &hwPictureH);
            ALOGI("(%s):HW Picture(HighSpeed) width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        } else {
            m_exynosCameraParameters->getMaxSensorSize(&hwPictureW, &hwPictureH);
            ALOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        }

        planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 2;
        planeCount  = 2;
        minBufferCount = 1;
        maxBufferCount = NUM_PICTURE_BUFFERS;

        if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            /* SCC Reprocessing Buffer realloc for high resolution callback */
            minBufferCount = 2;
        }

        ret = m_allocBuffers(m_sccReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_sccReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }

        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
            ret = m_setReprocessingBuffer();
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):m_setReprocessing Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }
    }

    if (m_reprocessingFrameFactory == NULL) {
        m_reprocessingFrameFactory = (ExynosCameraFrameFactory *)new ExynosCameraFrameReprocessingFactory(m_cameraId, m_exynosCameraParameters);

        ret = m_reprocessingFrameFactory->create();
        if (ret < 0) {
            ALOGE("ERR(%s):m_reprocessingFrameFactory->create() failed", __FUNCTION__);
            return ret;
        }

        m_pictureFrameFactory = m_reprocessingFrameFactory;
        ALOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
    }

    ret = m_reprocessingFrameFactory->initPipes();
    if (ret < 0) {
        ALOGE("ERR(%s):m_reprocessingFrameFactory->initPipes() failed", __FUNCTION__);
        return ret;
    }

    ret = m_reprocessingFrameFactory->preparePipes();
    if (ret < 0) {
        ALOGE("ERR(%s):m_reprocessingFrameFactory preparePipe fail", __FUNCTION__);
        return ret;
    }

    /* stream on pipes */
    ret = m_reprocessingFrameFactory->startPipes();
    if (ret < 0) {
        ALOGE("ERR(%s):m_reprocessingFrameFactory startPipe fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = true;

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPictureInternal(void)
{
    ALOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_prePictureThread->join();
    m_pictureThread->join();
    m_postPictureThread->join();

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true)
        m_highResolutionCallbackThread->join();

    m_jpegCallbackThread->join();

    if (m_zslPictureEnabled == true) {
        ret = m_reprocessingFrameFactory->stopPipes();
        if (ret < 0) {
            ALOGE("ERR(%s):m_reprocessingFrameFactory0>stopPipe() fail", __FUNCTION__);
        }
    }

    dstIspReprocessingQ->release();
    dstSccReprocessingQ->release();
    dstGscReprocessingQ->release();

    dstJpegReprocessingQ->release();

    m_jpegCallbackQ->release();

    ALOGD("DEBUG(%s[%d]):clear postProcess(Picture) Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_postProcessList);
    if (ret < 0) {
        ALOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = false;

    /* TODO: need timeout */
    return NO_ERROR;
}

status_t ExynosCamera::m_startRecordingInternal(void)
{
    int ret = 0;

    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int videoW = 0, videoH = 0;
    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_SILENT;
    int heapFd = 0;

    for (uint32_t i = 0; i < m_exynosconfig->current->bufInfo.num_recording_buffers ; i++)
        m_recordingTimeStamp[i] = 0L;

    m_exynosCameraParameters->getVideoSize(&videoW, &videoH);
    ALOGD("DEBUG(%s[%d]):videoSize = %d x %d",  __FUNCTION__, __LINE__, videoW, videoH);

    m_doCscRecording = true;
    if (m_exynosCameraParameters->doCscRecording() == true) {
        ALOGI("INFO(%s[%d]):do Recording CSC !!!", __FUNCTION__, __LINE__);
    } else {
        ALOGI("INFO(%s[%d]):skip Recording CSC !!!", __FUNCTION__, __LINE__);
        m_doCscRecording = false;
    }

    /* alloc recording Callback Heap */
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(struct addrs), m_exynosconfig->current->bufInfo.num_recording_buffers, &heapFd);

    if (m_doCscRecording == true) {
        /* alloc recording Image buffer */
        planeSize[0] = ROUND_UP(videoW, CAMERA_MAGIC_ALIGN) * ROUND_UP(videoH, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(videoW, CAMERA_MAGIC_ALIGN) * ROUND_UP(videoH / 2, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeCount   = 2;
        minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;

        ret = m_allocBuffers(m_recordingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_recordingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
    }

        return ret;
    }

    if (m_doCscRecording == true) {
        int recPipeId = 0;
        if (getCameraId() == CAMERA_ID_BACK)
            recPipeId = PIPE_GSC_VIDEO;
        else
            recPipeId = PIPE_GSC_VIDEO_FRONT;

        m_previewFrameFactory->startThread(recPipeId);
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_stopRecordingInternal(void)
{
    if (m_doCscRecording == true) {
        int recPipeId = 0;
        if (getCameraId() == CAMERA_ID_BACK)
            recPipeId = PIPE_GSC_VIDEO;
        else
            recPipeId = PIPE_GSC_VIDEO_FRONT;

        m_previewFrameFactory->stopThread(recPipeId);

        m_recordingThread->requestExitAndWait();
        m_recordingQ->release();

        m_recordingBufferMgr->deinit();
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_restartPreviewInternal(void)
{
    ALOGI("INFO(%s[%d]): Internal restart preview", __FUNCTION__, __LINE__);
    int ret = 0;
    int err = 0;

    m_startPictureInternalThread->join();
    m_previewFrameFactory->setStopFlag();
    m_mainThread->requestExitAndWait();

    ret = m_stopPictureInternal();
    if (ret < 0)
        ALOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

    ret = m_stopPreviewInternal();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

#if 0
    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
#endif

    /* skip to free and reallocate buffers */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->resetBuffers();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->resetBuffers();
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->resetBuffers();
    }

    /* skip to free and reallocate buffers */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->resetBuffers();
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->resetBuffers();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }

    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

    if( m_exynosCameraParameters->getHighSpeedRecording() && m_exynosCameraParameters->getReallocBuffer() ) {
        ALOGD("DEBUG(%s): realloc buffer all buffer deinit ", __FUNCTION__);
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
        if (m_3aaBufferMgr != NULL) {
            m_3aaBufferMgr->deinit();
        }
        if (m_ispBufferMgr != NULL) {
            m_ispBufferMgr->deinit();
        }
        if (m_sccBufferMgr != NULL) {
            m_sccBufferMgr->deinit();
        }
/*
        if (m_highResolutionCallbackBufferMgr != NULL) {
            m_highResolutionCallbackBufferMgr->deinit();
        }
*/
        if (m_gscBufferMgr != NULL) {
            m_gscBufferMgr->deinit();
        }
        if (m_jpegBufferMgr != NULL) {
            m_jpegBufferMgr->deinit();
        }
        if (m_recordingBufferMgr != NULL) {
            m_recordingBufferMgr->deinit();
        }
    }

    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    ALOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
    m_setBuffersThread->run(PRIORITY_DEFAULT);
    m_setBuffersThread->join();

    ret = m_startPreviewInternal();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_startPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    m_mainThread->run(PRIORITY_DEFAULT);

    return err;
}

bool ExynosCamera::m_mainThreadFunc(void)
{
    int ret = 0;
    int index = 0;
    ExynosCameraFrame *newFrame = NULL;

    if (m_previewEnabled == false) {
        ALOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        return false;
    }

    ret = m_pipeFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            ALOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            ALOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

/* HACK Reset Preview Flag*/
#if 0
    if (m_exynosCameraParameters->getRestartPreview() == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        ALOGE("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);
        if (ret < 0)
            ALOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);

        return true;
    }
#endif

    if (newFrame == NULL) {
        ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        ret = m_handlePreviewFrame(newFrame);
    } else {
        ret = m_handlePreviewFrameFront(newFrame);
    }
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):handle preview frame fail", __FUNCTION__, __LINE__);
        return ret;
    }

    if (newFrame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (newFrame->getFrameLockState() == false)
        {
            delete newFrame;
            newFrame = NULL;
        }
    }

    m_checkFpsAndUpdatePipeWaitTime();

    /* Continuous Auto-focus */
    if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_PICTURE)
    {
        int afstatus = 0;
        static int afResult = 1;
        int prev_afstatus = afResult;
        afstatus = m_exynosCameraActivityControl->getCAFResult();
        afResult = afstatus;

        if (afstatus == 3 && (prev_afstatus == 0 || prev_afstatus == 1)) {
            afResult = 4;
        }

        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)
            && (prev_afstatus != afstatus)) {
            ALOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)",
                __FUNCTION__, afResult, m_exynosCameraParameters->getFocusMode());
            m_notifyCb(CAMERA_MSG_FOCUS, afResult, 0, m_callbackCookie);
        }
    }

    return true;
}

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrame *frame)
{
    int ret = 0;
    int frameSkipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer buffer;
    int pipeID = 0;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }

    /* TODO: remove hard coding */
    switch(entity->getPipeId()) {
    case PIPE_3AA_ISP:
        m_debugFpsCheck(entity->getPipeId());
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }
        ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }

        ALOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        ret = m_putBuffers(m_ispBufferMgr, buffer.index);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            break;
        }
		
        frame->setMetaDataEnable(true);
		
        /* Face detection */
        if(!m_exynosCameraParameters->getHighSpeedRecording()) {
            if( m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) ) {
                ret = m_doFdCallbackFunc(frame);
                if (ret < 0) {
                    ALOGE("ERR(%s[%d]):m_doFdCallbackFunc fail, ret(%d)",
                        __FUNCTION__, __LINE__, ret);
                    return ret;
                }
            }
        }

        if (m_3aaBufferMgr->getNumOfAvailableBuffer() > 0 &&
                m_ispBufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = generateFrame(m_3aa_ispFrameCount, &newFrame);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }

            ret = m_setupEntity(PIPE_3AA_ISP, newFrame);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                break;
            }

            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA_ISP);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA_ISP);

            m_3aa_ispFrameCount++;
        }
        break;
    case PIPE_3AC:
    case PIPE_FLITE:
        m_debugFpsCheck(entity->getPipeId());
        if( m_exynosCameraParameters->getHighSpeedRecording() ) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                break;
            }
        } else {
            ret = m_captureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }

        /* TODO: Dynamic bayer capture, currently support only single shot */
        if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
            m_previewFrameFactory->stopThread(entity->getPipeId());
        }

        if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
            if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = generateFrame(m_fliteFrameCount, &newFrame);
                if (ret < 0) {
                    ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                ret = m_setupEntity(entity->getPipeId(), newFrame);
                if (ret < 0) {
                    ALOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                    break;
                }

                m_previewFrameFactory->pushFrameToPipe(&newFrame, entity->getPipeId());
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, entity->getPipeId());

                m_fliteFrameCount++;
            }
        }
        break;
    case PIPE_SCP:
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            /* TO DO : skip frame for HDR */
            shot_stream = (struct camera2_stream *)buffer.addr[2];
            if (shot_stream != NULL) {
                getStreamFrameValid(shot_stream, &fvalid);
                getStreamFrameCount(shot_stream, &fcount);
            } else {
                ALOGE("ERR(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
                fvalid = false;
                fcount = 0;
            }

            /* drop preview frame if lcd supported frame rate < scp frame rate */
            frame->getFpsRange(&minFps, &maxFps);
            if (dispFps < maxFps) {
                ratio = (int)((maxFps * 10 / dispFps) / 10);
                m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                skipPreview = (m_displayPreviewToggle == 0) ? true : false;
#ifdef DEBUG
                ALOGE("DEBUG(%s[%d]):preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d, maxFps=%d, dispFps=%d, ratio=%d, skipPreview=%d)",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), m_displayPreviewToggle, maxFps, dispFps, ratio, (int)skipPreview);
#endif
            }

            m_exynosCameraParameters->getFrameSkipCount(&frameSkipCount);

            if (frameSkipCount > 0) {
            ALOGD("INFO(%s[%d]):frameSkipCount=%d buffer.index(%d)", __FUNCTION__, __LINE__, frameSkipCount, buffer.index);
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            } else {
                if (m_skipReprocessing == true)
                    m_skipReprocessing = false;
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                m_recordingStateLock.lock();
                if (m_recordingEnabled == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        ALOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                    } else {
                        if (m_exynosCameraParameters->doCscRecording() == true) {
                            /* get Recording Image buffer */
                            int bufIndex = -2;
                            ExynosCameraBuffer recordingBuffer;
                            ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                            if (ret < 0 || bufIndex < 0) {
                                if ((++m_recordingFrameSkipCount % 100) == 0) {
                                    ALOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                    m_recordingBufferMgr->printBufferQState();
                                }
                            } else {
                                if (m_recordingFrameSkipCount != 0) {
                                    ALOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex, m_recordingQ->getSizeOfProcessQ());
                                    m_recordingFrameSkipCount = 0;
                                    m_recordingBufferMgr->printBufferQState();
                                }
                                m_recordingTimeStamp[bufIndex] = timeStamp;

                                ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer);
                                if (ret < 0) {
                                    ALOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                }
                            }
                        } else {
                            m_recordingTimeStamp[buffer.index] = timeStamp;
                            if (m_recordingStartTimeStamp == 0)
                                m_recordingStartTimeStamp = timeStamp;

                            if ((0L < timeStamp)
                                && (m_lastRecordingTimeStamp < timeStamp)
                                && (m_recordingStartTimeStamp <= timeStamp)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
                                    ALOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld",
                                        __FUNCTION__, __LINE__, buffer.index, timeStamp);
#endif
#ifdef DEBUG
                                    ALOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                                        __FUNCTION__, __LINE__,
                                        m_lastRecordingTimeStamp,
                                        systemTime(SYSTEM_TIME_MONOTONIC),
                                        m_recordingStartTimeStamp);
#endif
                                    struct addrs *recordAddrs = NULL;

                                    recordAddrs = (struct addrs *)m_recordingCallbackHeap->data;
                                    recordAddrs[buffer.index].type        = kMetadataBufferTypeCameraSource;
                                    recordAddrs[buffer.index].fdPlaneY    = (unsigned int)buffer.fd[0];
                                    recordAddrs[buffer.index].fdPlaneCbcr = (unsigned int)buffer.fd[1];
                                    recordAddrs[buffer.index].bufIndex    = buffer.index;

                                    m_dataCbTimestamp(
                                            timeStamp,
                                            CAMERA_MSG_VIDEO_FRAME,
                                            m_recordingCallbackHeap,
                                            buffer.index,
                                            m_callbackCookie);
                                    m_lastRecordingTimeStamp = timeStamp;
                            }
                        }
                    }
                }
                m_recordingStateLock.unlock();

            if (skipPreview == true) {
                ALOGD("INFO(%s[%d]):frameSkipCount=%d buffer.index(%d)", __FUNCTION__, __LINE__, frameSkipCount, buffer.index);
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            } else {
                    ExynosCameraBuffer callbackBuffer;
                    ExynosCameraFrame *callbackFrame = NULL;
                    struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;
                    callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP);

                    frame->getDstBuffer(PIPE_SCP, &callbackBuffer);
                    frame->getMetaData(shot_ext);
                    callbackFrame->storeDynamicMeta(shot_ext);
                    callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);
                    delete shot_ext;

                    ALOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                    m_previewQ->pushProcessQ(&callbackFrame);
                }
            }

            ALOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else {
            ALOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            m_previewFrameFactory->dump();
        }

        ALOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

        ret = generateFrame(m_scpFrameCount, &newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        ret = m_setupEntity(PIPE_SCP, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        /*check preview drop...*/
        newFrame->getDstBuffer(PIPE_SCP, &resultBuffer);
        if (resultBuffer.index < 0) {
            newFrame->setRequest(PIPE_SCP, false);
            newFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
            node_group_info_isp.capture[PERFRAME_BACK_SCP_POS].request = 0;
            newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

            m_previewFrameFactory->dump();

            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            m_exynosCameraParameters->setFrameSkipCount(3);
        }
        /*check preview drop...*/

        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);

        m_scpFrameCount++;
        break;
    default:
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    return NO_ERROR;
}

bool ExynosCamera::m_previewThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = false;
    int  pipeId    = 0;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    frame_queue_t *previewQ;

    if (getCameraId() == CAMERA_ID_BACK) {
        pipeId    = PIPE_SCP;
        pipeIdCsc = PIPE_GSC;
        previewQ = m_previewQ;
    } else {
        pipeId    = PIPE_SCP_FRONT;
        pipeIdCsc = PIPE_GSC_FRONT;
        previewQ = m_previewFrontQ;
    }

    ALOGV("INFO(%s[%d]):wait previewQ", __FUNCTION__, __LINE__);
    ret = previewQ->waitAndPopProcessQ(&frame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    if (frame == NULL) {
        ALOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ALOGV("INFO(%s[%d]):get frame from previewQ", __FUNCTION__, __LINE__);
    timeStamp = (nsecs_t)frame->getTimeStamp();
    frameCount = frame->getFrameCount();
    ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }
    /* ------------- frome here "frame" cannot use ------------- */
    ALOGV("INFO(%s[%d]):push frame to previewReturnQ", __FUNCTION__, __LINE__);
    if( m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE ) {
        maxbuffers = m_exynosCameraParameters->getPreviewBufferCount();

    } else {
        maxbuffers = (int)m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    if (buffer.index < 0 || buffer.index >= maxbuffers ) {
        ALOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, maxbuffers, buffer.index);
        goto func_exit;
    }

    ALOGV("INFO(%s[%d]):m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)", __FUNCTION__, __LINE__,
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    if ((m_recordingEnabled == true) ||
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 4 &&
        previewQ->getSizeOfProcessQ() < 2) ||
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1)) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false) {
            ExynosCameraBuffer previewCbBuffer;

            ret = m_setPreviewCallbackBuffer();
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }

            int bufIndex = -2;
            m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);

            ExynosCameraFrame  *newFrame = NULL;

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipeIdCsc);
            if (newFrame == NULL) {
                ALOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
                return UNKNOWN_ERROR;
            }

            ret = m_doPreviewToCallbackFunc(pipeIdCsc, newFrame, buffer, previewCbBuffer);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):m_doPreviewToCallbackFunc fail", __FUNCTION__, __LINE__);
            } else {
                if (m_exynosCameraParameters->getCallbackNeedCopy2Rendering() == true) {
                    ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, buffer);
                    if (ret < 0)
                        ALOGE("ERR(%s[%d]):m_doCallbackToPreviewFunc fail", __FUNCTION__, __LINE__);
                }
            }

            m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        }

    if (m_previewWindow != NULL) {
        if (timeStamp > 0L) {
            m_previewWindow->set_timestamp(m_previewWindow, (int64_t)timeStamp);
        } else {
            uint32_t fcount = 0;
            getStreamFrameCount((struct camera2_stream *)buffer.addr[2], &fcount);
            ALOGW("WRN(%s[%d]): frameCount(%d)(%d), Invalid timeStamp(%lld)",
                    __FUNCTION__, __LINE__,
                    frameCount,
                    fcount,
                    timeStamp);
        }

        /* display the frame */
        ret = m_putBuffers(m_scpBufferMgr, buffer.index);
        if (ret < 0) {
            /* TODO: error handling */
            ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }
    }
    } else {

        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
    }

func_exit:

    if (frame != NULL) {
        delete frame;
        frame = NULL;
    }

    if (previewQ->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;
}

status_t ExynosCamera::m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr)
{
    /*
     * If it is not 16-aligend, shrink down it as 16 align. ex) 1080 -> 1072
     * But, memory is set on Android format. so, not aligned area will be black.
     */
    int dst_width = 0, dst_height = 0, dst_crop_width = 0, dst_crop_height = 0;
    int dst_format = m_exynosCameraParameters->getPreviewFormat();

    m_exynosCameraParameters->getPreviewSize(&dst_width, &dst_height);
    dst_crop_width = dst_width;
    dst_crop_height = dst_height;

    if (dst_format == V4L2_PIX_FMT_NV21 ||
        dst_format == V4L2_PIX_FMT_NV21M) {

        callbackBuf->size[0] = (dst_width * dst_height);
        callbackBuf->size[1] = (dst_width * dst_height) / 2;

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
    } else if (dst_format == V4L2_PIX_FMT_YVU420 ||
               dst_format == V4L2_PIX_FMT_YVU420M) {
        callbackBuf->size[0] = dst_width * dst_height;
        callbackBuf->size[1] = dst_width / 2 * dst_height / 2;
        callbackBuf->size[2] = callbackBuf->size[1];

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
        callbackBuf->addr[2] = callbackBuf->addr[1] + callbackBuf->size[1];
    }

    ALOGV("DEBUG(%s): preview size(%dx%d)", __FUNCTION__, hwPreviewW, hwPreviewH);
    ALOGV("DEBUG(%s): dst_size(%dx%d), dst_crop_size(%dx%d)", __FUNCTION__, dst_width, dst_height, dst_crop_width, dst_crop_height);

    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToCallbackFunc(
        int32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf)
{
    ALOGV("DEBUG(%s): converting preview to callback buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    bool useCSC = m_exynosCameraParameters->getCallbackNeedCSC();

    ExynosCameraDurationTimer probeTimer;
    int probeTimeMSEC;
    uint32_t fcount = 0;

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    ExynosRect srcRect, dstRect;

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_calcPreviewGSCRect(&srcRect, &dstRect);

    if (useCSC) {
        frame_queue_t gscFrameDoneQ;

        ret = newFrame->setSrcRect(pipeId, &srcRect);
        ret = newFrame->setDstRect(pipeId, &dstRect);

        ret = m_setupEntity(pipeId, newFrame, &previewBuf, &callbackBuf);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        m_previewFrameFactory->setOutputFrameQToPipe(&gscFrameDoneQ, pipeId);

        ALOGV("INFO(%s[%d]):wait preview callback output", __FUNCTION__, __LINE__);
        ret = gscFrameDoneQ.waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            statusRet = INVALID_OPERATION;
            goto done;
        }
        if (newFrame == NULL) {
            ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        ALOGV("INFO(%s[%d]):preview callback done", __FUNCTION__, __LINE__);

#if 0
        int remainedH = m_orgPreviewRect.h - dst_height;

        if (remainedH != 0) {
            char *srcAddr = NULL;
            char *dstAddr = NULL;
            int planeDiver = 1;

            for (int plane = 0; plane < 2; plane++) {
                planeDiver = (plane + 1) * 2 / 2;

                srcAddr = previewBuf.virt.extP[plane] + (ALIGN_UP(hwPreviewW, CAMERA_ISP_ALIGN) * dst_crop_height / planeDiver);
                dstAddr = callbackBuf->virt.extP[plane] + (m_orgPreviewRect.w * dst_crop_height / planeDiver);

                for (int i = 0; i < remainedH; i++) {
                    memcpy(dstAddr, srcAddr, (m_orgPreviewRect.w / planeDiver));

                    srcAddr += (ALIGN_UP(hwPreviewW, CAMERA_ISP_ALIGN) / planeDiver);
                    dstAddr += (m_orgPreviewRect.w                   / planeDiver);
                }
            }
        }
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            ALOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = previewBuf.addr[plane];
            dstAddr = callbackBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);
        }
    }

    probeTimer.start();
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
    }
    probeTimer.stop();
    getStreamFrameCount((struct camera2_stream *)previewBuf.addr[2], &fcount);
    probeTimeMSEC = (int)probeTimer.durationMsecs();

    if (probeTimeMSEC > 33 && probeTimeMSEC <= 66)
        ALOGV("(%s[%d]):(%d) %5d ", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());
    else if(probeTimeMSEC > 66)
        ALOGE("(%s[%d]):(%d) %5d ", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());
    else
        ALOGV("(%s[%d]):(%d) %5d ", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());

done:
    previewCallbackHeap->release(previewCallbackHeap);

    return statusRet;
}

status_t ExynosCamera::m_doCallbackToPreviewFunc(
        int32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer callbackBuf,
        ExynosCameraBuffer previewBuf)
{
    ALOGV("DEBUG(%s): converting callback to preview buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    bool useCSC = m_exynosCameraParameters->getCallbackNeedCSC();

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (useCSC) {
#if 0
        if (m_exynosPreviewCSC) {
            csc_set_src_format(m_exynosPreviewCSC,
                    ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_MAGIC_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_MAGIC_ALIGN),
                    0, 0, ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_MAGIC_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_MAGIC_ALIGN),
                    V4L2_PIX_2_HAL_PIXEL_FORMAT(m_orgPreviewRect.colorFormat),
                    1);

            csc_set_dst_format(m_exynosPreviewCSC,
                    previewW, previewH,
                    0, 0, previewW, previewH,
                    V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat),
                    0);

            csc_set_src_buffer(m_exynosPreviewCSC,
                    (void **)callbackBuf->virt.extP, CSC_MEMORY_USERPTR);

            csc_set_dst_buffer(m_exynosPreviewCSC,
                    (void **)previewBuf.fd.extFd, CSC_MEMORY_TYPE);

            if (csc_convert_with_rotation(m_exynosPreviewCSC, 0, m_flip_horizontal, 0) != 0)
                ALOGE("ERR(%s):csc_convert() from callback to lcd fail", __FUNCTION__);
        } else {
            ALOGE("ERR(%s):m_exynosPreviewCSC == NULL", __FUNCTION__);
        }
#else
        ALOGW("WRN(%s[%d]): doCallbackToPreview use CSC is not yet possible", __FUNCTION__, __LINE__);
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            ALOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = callbackBuf.addr[plane];
            dstAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);
        }
    }

done:
    previewCallbackHeap->release(previewCallbackHeap);

    return statusRet;
}

status_t ExynosCamera::m_handlePreviewFrameFront(ExynosCameraFrame *frame)
{
    int ret = 0;
    int frameSkipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer buffer;
    int pipeID = 0;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):current entity is NULL, frameCount(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount());
        /* TODO: doing exception handling */
        return true;
    }

    pipeID = entity->getPipeId();

    switch(entity->getPipeId()) {
    case PIPE_ISP_FRONT:
        m_debugFpsCheck(entity->getPipeId());
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        frame->setMetaDataEnable(true);

//        if (isReprocessing() == true && m_exynosCameraParameters->getSeriesShotCount() == 0 &&
//                frame->getRequest(PIPE_SCC) == false && m_hdrEnabled == false) {
        if (isReprocessing() == true) {
            ret = m_captureSelector->manageFrameHoldList(frame, pipeID, true);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        } else {
            /* TODO: This is unusual case, flite buffer and isp buffer */
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        ret = m_putBuffers(m_ispBufferMgr, buffer.index);

        /* Face detection */
        ret = m_doFdCallbackFunc(frame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_doFdCallbackFunc fail, ret(%d)",
                    __FUNCTION__, __LINE__, ret);
            return ret;
        }

        ret = generateFrame(m_3aa_ispFrameCount, &newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        m_setupEntity(PIPE_FLITE_FRONT, newFrame);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE_FRONT);

        m_setupEntity(PIPE_3AA_FRONT, newFrame);
        m_setupEntity(PIPE_ISP_FRONT, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP_FRONT);

        m_3aa_ispFrameCount++;

        /* HACK: When SCC pipe is stopped, generate frame for SCC */
        if ((m_sccBufferMgr->getNumOfAvailableBuffer() > 2)) {
            ALOGW("WRN(%s[%d]): Too many available SCC buffers, generating frame for SCC", __FUNCTION__, __LINE__);
            while (m_sccBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = generateFrame(m_sccFrameCount, &newFrame);
                if (ret < 0) {
                    ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_setupEntity(PIPE_SCC_FRONT, newFrame);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCC_FRONT);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCC_FRONT);

                m_sccFrameCount++;
            }
        }

        break;
    case PIPE_SCC_FRONT:
        m_debugFpsCheck(entity->getPipeId());
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            ret = m_sccCaptureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }

        while (m_sccBufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = generateFrame(m_sccFrameCount, &newFrame);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_setupEntity(PIPE_SCC_FRONT, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCC_FRONT);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCC_FRONT);

            m_sccFrameCount++;
        }
        break;
    case PIPE_SCP_FRONT:
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            m_exynosCameraParameters->getFrameSkipCount(&frameSkipCount);
            if (frameSkipCount > 0) {
                ALOGD("INFO(%s[%d]):frameSkipCount=%d", __FUNCTION__, __LINE__, frameSkipCount);
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            } else {
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                m_recordingStateLock.lock();
                if (m_recordingEnabled == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        ALOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                    } else {
                        /* get Recording Image buffer */
                        int bufIndex = -2;
                        ExynosCameraBuffer recordingBuffer;
                        ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                        if (ret < 0 || bufIndex < 0) {
                            if ((++m_recordingFrameSkipCount % 100) == 0) {
                                ALOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                            }
                        } else {
                            if (m_recordingFrameSkipCount != 0) {
                                ALOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d)",
                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                m_recordingFrameSkipCount = 0;
                            }
                            m_recordingTimeStamp[bufIndex] = timeStamp;
                            ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO_FRONT, buffer, recordingBuffer);
                            if (ret < 0) {
                                ALOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                            }
                        }
                    }
                }
                m_recordingStateLock.unlock();

                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;
                struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;

                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP_FRONT);
                frame->getDstBuffer(PIPE_SCP_FRONT, &callbackBuffer);
                frame->getMetaData(shot_ext);
                callbackFrame->storeDynamicMeta(shot_ext);
                callbackFrame->setDstBuffer(PIPE_SCP_FRONT, callbackBuffer);
                delete shot_ext;

                ALOGV("INFO(%s[%d]):push frame to front previewQ", __FUNCTION__, __LINE__);
                m_previewFrontQ->pushProcessQ(&callbackFrame);

                ALOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            }
        } else {
            ALOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            m_previewFrameFactory->dump();
        }

        ret = generateFrame(m_scpFrameCount, &newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        m_setupEntity(PIPE_SCP_FRONT, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        /*check preview drop...*/
        newFrame->getDstBuffer(PIPE_SCP_FRONT, &resultBuffer);
        if (resultBuffer.index < 0) {
           newFrame->setRequest(PIPE_SCP_FRONT, false);
           newFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
           node_group_info_isp.capture[PERFRAME_FRONT_SCP_POS].request = 0;
           newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

           m_previewFrameFactory->dump();

           /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
           m_exynosCameraParameters->setFrameSkipCount(3);
        }

        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP_FRONT);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP_FRONT);

        m_scpFrameCount++;
        break;
    default:
        break;
    }

    if (ret < 0) {
        ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    return NO_ERROR;
}

bool ExynosCamera::m_setBuffersThreadFunc(void)
{
    int ret = 0;

    ret = m_setBuffers();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_setBuffers failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

bool ExynosCamera::m_startPictureInternalThreadFunc(void)
{
    int ret = 0;

    ret = m_startPictureInternal();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_setBuffers failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

bool ExynosCamera::m_prePictureThreadFunc(void)
{
    bool loop = false;
    bool isProcessed = true;

    ExynosCameraDurationTimer m_burstPrePictureTimer;
    uint64_t m_burstPrePictureTimerTime;
    status_t ret = NO_ERROR;

    uint64_t seriesShotDuration = m_exynosCameraParameters->getSeriesShotDuration();

    // Minimum guaranted delay between shot for burst case
    uint64_t subDuration = 0;
    if (m_exynosCameraParameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
        subDuration = seriesShotDuration > 0 ? seriesShotDuration / 4 : 0;
    }

    if (m_isNeedAllocPictureBuffer == true) {

        /* alloc gsc/jpeg buffer */
        ret = m_setPictureBuffer();
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): set picture buffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            return UNKNOWN_ERROR;
        }
        ALOGD("DEBUG(%s[%d]): Deferred allocation of JPEG/GSC buffer completed.", __FUNCTION__, __LINE__);
        m_isNeedAllocPictureBuffer = false;
    }

    m_burstPrePictureTimer.start();

//    if (isReprocessing() && m_exynosCameraParameters->getSeriesShotCount() == 0 &&
//            m_hdrEnabled == false)
    if (isReprocessing())
        loop = m_reprocessingPrePictureInternal();
    else
        loop = m_prePictureInternal(&isProcessed);

    m_burstPrePictureTimer.stop();
    m_burstPrePictureTimerTime = m_burstPrePictureTimer.durationUsecs();

    if(isProcessed == false) {
        // HACK: If m_prePictureInternal reported frame processing failure, do not wait on the while loop.
        m_burstPrePictureTimerTime = seriesShotDuration;
        ALOGD("DEBUG(%s[%d]): m_prePictureInternal failed to get frame.", __FUNCTION__, __LINE__);
    } else if(subDuration > 0 && m_burstPrePictureTimerTime > subDuration) {
        /*
         * HACK: Making the frame delay between shot is at least 3/4 of getSeriesShotDuration()
         *       to avoid large variance in interval between burst shot pictures.
         */
        ALOGD("DEBUG(%s[%d]): Remaining time for next shot is too short(%lld)ms. Extended to (%lld)ms"
            , __FUNCTION__, __LINE__, seriesShotDuration - m_burstPrePictureTimerTime, seriesShotDuration - subDuration);
        m_burstPrePictureTimerTime = subDuration;
    }

    while (seriesShotDuration > m_burstPrePictureTimerTime && m_reprocessingCounter.getCount() > 0) {
        ALOGD("DEBUG(%s[%d]): waiting next shot(%lld)", __FUNCTION__, __LINE__, seriesShotDuration - m_burstPrePictureTimerTime);
        m_burstPrePictureTimerTime += 10000;
        usleep(10000);
    }

    return loop;
}

/*
 * pIsProcessed : out parameter
 *                true if the frame is properly handled.
 *                false if frame processing is failed or there is no frame to process
 */
bool ExynosCamera::m_prePictureInternal(bool* pIsProcessed)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ALOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    ExynosCameraFrame *newFrame = NULL;
    camera2_shot_ext *shot_ext = NULL;

    ExynosCameraBuffer fliteReprocessingBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    int pipeId = 0;
    bool isSrc = false;
    int retryCount = 3;

    if (m_hdrEnabled)
        retryCount = 15;

    if (getCameraId() == CAMERA_ID_BACK)
        pipeId = PIPE_SCC;
    else
        pipeId = PIPE_SCC_FRONT;

    int postProcessQSize = m_postPictureQ->getSizeOfProcessQ();
    if (postProcessQSize > 2) {
        ALOGW("DEBUG(%s[%d]): post picture is delayed(stacked %d frames), skip", __FUNCTION__, __LINE__, postProcessQSize);
        usleep(WAITING_TIME);
        goto CLEAN;
    }

    newFrame = m_sccCaptureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
    if (newFrame == NULL) {
        ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }
    newFrame->frameLock();

    ALOGE("DEBUG(%s[%d]):Frame Count (%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

    m_postProcessList.push_back(newFrame);
    dstSccReprocessingQ->pushProcessQ(&newFrame);

    m_reprocessingCounter.decCount();

    ALOGI("INFO(%s[%d]):prePicture complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if (m_reprocessingCounter.getCount() > 0) {
        loop = true;
#if 0
    } else {
        if (m_exynosCameraParameters->getUseDynamicScc() == true) {
            ALOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestSCC(false);
        }
        m_sccCaptureSelector->clearList(pipeId, isSrc);
#endif
    }

    *pIsProcessed = true;
    return loop;

CLEAN:
    if (newFrame != NULL && newFrame->isComplete() == true) {
        newFrame->printEntity();
        ALOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        delete newFrame;
        newFrame = NULL;
    }

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    ALOGI("INFO(%s[%d]): prePicture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());
    *pIsProcessed = false;   // Notify failure
    return loop;

}

bool ExynosCamera::m_reprocessingPrePictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ALOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    int retry = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    uint32_t bayerFrameCount = 0;
    struct camera2_node_output output_crop_info;

    ExynosCameraBufferManager *bufferMgr = NULL;

    int bayerPipeId = 0;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        if (m_highResolutionCallbackRunning == true) {
            /* will be removed */
            while (m_skipReprocessing == true) {
                usleep(WAITING_TIME);
                if (m_skipReprocessing == false) {
                    ALOGD("DEBUG(%s[%d]:stop skip frame for high resolution preview callback", __FUNCTION__, __LINE__);
                    break;
                }
            }
        } else if (m_highResolutionCallbackRunning == false) {
            ALOGW("WRN(%s[%d]): m_reprocessingThreadfunc stop for high resolution preview callback", __FUNCTION__, __LINE__);
            loop = false;
            goto CLEAN_FRAME;
        }
    }

    /* Get Bayer buffer for reprocessing */
    ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): getBayerBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

    ALOGD("DEBUG(%s[%d]):bayerBuffer index %d", __FUNCTION__, __LINE__, bayerBuffer.index);

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        m_captureSelector->clearList(m_getBayerPipeId(), false);
    }

    /* Generate reprocessing Frame */
    ret = generateFrameReprocessing(&newFrame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):generateFrameReprocessing fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        int sensorMaxW, sensorMaxH;
        bool bRet;
        char filePath[70];

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",m_cameraId, m_fliteFrameCount);
        m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
            sensorMaxW += 16;
            sensorMaxH += 10;
        }
        bRet = dumpToFile((char *)filePath,
            bayerBuffer.addr[0],
            sensorMaxW * sensorMaxH * 2);
        if (bRet != true)
            ALOGE("couldn't make a raw file");
    }
#endif /* DEBUG_RAWDUMP */

    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == false) {
        /* TODO: HACK: Will be removed, this is driver's job */
        ret = m_convertingStreamToShotExt(&bayerBuffer, &output_crop_info);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): shot_stream to shot_ext converting fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN_FRAME;
        }

        camera2_node_group node_group_info;
        ExynosRect srcRect , dstRect;
        int pictureW = 0, pictureH = 0;

        memset(&node_group_info, 0x0, sizeof(camera2_node_group));
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);

        newFrame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
        node_group_info.leader.input.cropRegion[0] = output_crop_info.cropRegion[0];
        node_group_info.leader.input.cropRegion[1] = output_crop_info.cropRegion[1];
        node_group_info.leader.input.cropRegion[2] = output_crop_info.cropRegion[2];
        node_group_info.leader.input.cropRegion[3] = output_crop_info.cropRegion[3];
        node_group_info.leader.output.cropRegion[0] = 0;
        node_group_info.leader.output.cropRegion[1] = 0;
        node_group_info.leader.output.cropRegion[2] = node_group_info.leader.input.cropRegion[2];
        node_group_info.leader.output.cropRegion[3] = node_group_info.leader.input.cropRegion[3];

        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0] = node_group_info.leader.output.cropRegion[0];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1] = node_group_info.leader.output.cropRegion[1];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0] = 0;
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1] = 0;
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2] = pictureW;
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3] = pictureH;

        ALOGV("DEBUG(%s[%d]): isp capture input(%d %d %d %d), output(%d %d %d %d)", __FUNCTION__, __LINE__,
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3]);

        if (node_group_info.leader.output.cropRegion[2] < node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2]) {
            ALOGI("INFO(%s[%d]:(%d -> %d))", __FUNCTION__, __LINE__,
                node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2],
                node_group_info.leader.output.cropRegion[2]);

            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
        }
        if (node_group_info.leader.output.cropRegion[3] < node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3]) {
            ALOGI("INFO(%s[%d]:(%d -> %d))", __FUNCTION__, __LINE__,
                node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3],
                node_group_info.leader.output.cropRegion[3]);

            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        }

        newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[1]);

    /* Meta setting */
    if (shot_ext != NULL) {
        ret = newFrame->storeDynamicMeta(shot_ext);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN_FRAME;
        }

        ret = newFrame->storeUserDynamicMeta(shot_ext);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN_FRAME;
        }

        newFrame->getMetaData(shot_ext);
        m_exynosCameraParameters->duplicateCtrlMetadata((void *)shot_ext);

        ALOGD("DEBUG(%s[%d]):meta_shot_ext->shot.dm.request.frameCount : %d",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount(shot_ext));
    } else {
        ALOGE("DEBUG(%s[%d]):shot_ext is NULL", __FUNCTION__, __LINE__);
    }

    /* SCC */
    m_getBufferManager(PIPE_SCC_REPROCESSING, &bufferMgr, DST_BUFFER_DIRECTION);

    ret = m_checkBufferAvailable(PIPE_SCC_REPROCESSING, bufferMgr);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): Waiting buffer timeout, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, PIPE_SCC_REPROCESSING, ret);
        goto CLEAN_FRAME;
    }

    ret = m_setupEntity(PIPE_SCC_REPROCESSING, newFrame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]:setupEntity fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, PIPE_SCC_REPROCESSING, ret);
        goto CLEAN_FRAME;
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        m_reprocessingFrameFactory->setOutputFrameQToPipe(m_highResolutionCallbackQ, PIPE_SCC_REPROCESSING);
    } else {
        m_reprocessingFrameFactory->setOutputFrameQToPipe(dstSccReprocessingQ, PIPE_SCC_REPROCESSING);
    }
    /* push frame to SCC pipe */
    m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCC_REPROCESSING);

    /* Add frame to post processing list */
    newFrame->frameLock();
    m_postProcessList.push_back(newFrame);

    ret = m_reprocessingFrameFactory->startInitialThreads();
    if (ret < 0) {
        ALOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        goto CLEAN;
    }

    ret = newFrame->ensureDstBufferState(PIPE_SCC_REPROCESSING, ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    /* Get bayerPipeId at first entity */
    bayerPipeId = newFrame->getFirstEntity()->getPipeId();
    ALOGD("DEBUG(%s[%d]): bayer Pipe ID(%d)", __FUNCTION__, __LINE__, bayerPipeId);

    /* Check available buffer */
    ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN;
    }
    if (bufferMgr != NULL) {
        ret = m_checkBufferAvailable(bayerPipeId, bufferMgr);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): Waiting buffer timeout, bayerPipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, bayerPipeId, ret);
            goto CLEAN;
        }
    }

    ret = m_setupEntity(bayerPipeId, newFrame, &bayerBuffer, NULL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]:setupEntity fail, bayerPipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, bayerPipeId, ret);
        goto CLEAN;
    }

    m_reprocessingFrameFactory->setOutputFrameQToPipe(dstIspReprocessingQ, PIPE_ISP_REPROCESSING);

    /* push the newFrameReprocessing to pipe */
    m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, bayerPipeId);

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true ||
        isSccCapture() == true)
        m_reprocessingFrameFactory->startThread(bayerPipeId);

    /* wait ISP done */
    ALOGI("INFO(%s[%d]):wait ISP output", __FUNCTION__, __LINE__);
    ret = dstIspReprocessingQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):ISP wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        /* goto CLEAN; */
    }
    if (newFrame == NULL) {
        ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, bayerPipeId, ret);
        return ret;
    }

    ALOGI("INFO(%s[%d]):ISP output done", __FUNCTION__, __LINE__);

    newFrame->setMetaDataEnable(true);

    /* put bayer buffer */
    ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): 3AA src putBuffer fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, bayerBuffer.index, ret);
        goto CLEAN;
    }

    /* put isp buffer */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
        if (bufferMgr != NULL) {
            ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_ispReprocessingBufferMgr, ispReprocessingBuffer.index);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]): ISP src putBuffer fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, bayerBuffer.index, ret);
                goto CLEAN;
            }
        }
    }

    m_reprocessingCounter.decCount();

    ALOGI("INFO(%s[%d]):reprocessing complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    /* one shot */
    return loop;

CLEAN_FRAME:
    /* newFrame is not pushed any pipes, we can delete newFrame */
    if (newFrame != NULL && newFrame->getFrameLockState() == false) {
        newFrame->printEntity();
        ALOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        delete newFrame;
        newFrame = NULL;
    }

CLEAN:
    if (bayerBuffer.index != -2 && m_bayerBufferMgr != NULL)
        m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
    if (ispReprocessingBuffer.index != -2 && m_ispReprocessingBufferMgr != NULL)
        m_putBuffers(m_ispReprocessingBufferMgr, ispReprocessingBuffer.index);

    /* newFrame is already pushed some pipes, we can not delete newFrame until frame is complete */
    if (newFrame != NULL && newFrame->isComplete() == true) {
        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        newFrame->printEntity();
        ALOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        delete newFrame;
        newFrame = NULL;
    }

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    ALOGI("INFO(%s[%d]): reprocessing fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    return loop;
}

bool ExynosCamera::m_pictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int bufIndex = -2;

    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    struct camera2_stream *shot_stream = NULL;
    ExynosRect srcRect, dstRect;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;

    sccReprocessingBuffer.index = -2;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;
    bool isSrc = false;

//    if (isReprocessing() == true && m_exynosCameraParameters->getSeriesShotCount() == 0 &&
//            m_hdrEnabled == false) {
    if (isReprocessing() == true) {
        pipeId_scc = PIPE_SCC_REPROCESSING;
        pipeId_gsc = PIPE_GSC_REPROCESSING;
        isSrc = true;
    } else {
        switch (getCameraId()) {
        case CAMERA_ID_FRONT:
            pipeId_scc = PIPE_SCC_FRONT;
            pipeId_gsc = PIPE_GSC_PICTURE_FRONT;
            break;
        default:
            ALOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                    __FUNCTION__, __LINE__, getCameraId(), isReprocessing());
            break;
        }
    }

    /* wait SCC */
    ALOGI("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    int retry = 0;
    do {
        ret = dstSccReprocessingQ->waitAndPopProcessQ(&newFrame);
        retry++;
    } while (ret == TIMED_OUT && retry < 40 &&
             (m_takePictureCounter.getCount() > 0 || m_exynosCameraParameters->getSeriesShotCount() == 0));

    if (ret < 0) {
        ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d), retry(%d), takePictuerCount(%d), seriesShotCount(%d)",
                __FUNCTION__, __LINE__, ret, retry, m_takePictureCounter.getCount(), m_exynosCameraParameters->getSeriesShotCount());
        // TODO: doing exception handling
        goto CLEAN;
    }

    if (newFrame == NULL || m_postProcessList.size() <= 0) {
        ALOGE("ERR(%s[%d]):newFrame is NULL or postPictureList size(%d)", __FUNCTION__, __LINE__, m_postProcessList.size());
        goto CLEAN;
    }

    /* When Front camera dose not setEntityState because Front camera share preview and capture frames */
    if (getCameraId() == CAMERA_ID_BACK) {
        ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            return ret;
        }
    }

    ALOGI("INFO(%s[%d]):SCC output done, frame Count(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

    if (needGSCForCapture(getCameraId()) == true) {
        /* set GSC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[1]);
        if (shot_stream != NULL) {
            ALOGD("DEBUG(%s[%d]):(%d %d %d %d)", __FUNCTION__, __LINE__,
                shot_stream->fcount,
                shot_stream->rcount,
                shot_stream->findex,
                shot_stream->fvalid);
            ALOGD("DEBUG(%s[%d]):(%d %d %d %d)(%d %d %d %d)", __FUNCTION__, __LINE__,
                shot_stream->input_crop_region[0],
                shot_stream->input_crop_region[1],
                shot_stream->input_crop_region[2],
                shot_stream->input_crop_region[3],
                shot_stream->output_crop_region[0],
                shot_stream->output_crop_region[1],
                shot_stream->output_crop_region[2],
                shot_stream->output_crop_region[3]);
        } else {
            ALOGE("DEBUG(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        int retry = 0;
        m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
        do {
            ret = -1;
            retry++;
            if (bufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, NULL);
            } else {
                /* wait available SCC buffer */
                usleep(WAITING_TIME);
            }

            if (retry % 10 == 0) {
                ALOGW("WRAN(%s[%d]):retry setupEntity for GSC postPictureQ(%d)",
                        __FUNCTION__, __LINE__,
                        m_postPictureQ->getSizeOfProcessQ());
            }
        } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

        if (ret < 0) {
            if (retry >= (TOTAL_WAITING_TIME/WAITING_TIME)) {
                ALOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, retry, ret, m_stopBurstShot);
            } else {
                ALOGD("DEBUG(%s[%d]):setupEntity stopped, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, retry, ret, m_stopBurstShot);
            }
            goto CLEAN;
        }
/* should change size calculation code in pure bayer */
#if 0
        if (shot_stream != NULL) {
            ret = m_calcPictureRect(&srcRect, &dstRect);
            ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
            ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
        }
#else
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_exynosCameraParameters->getPictureFormat();

        srcRect.x = shot_stream->input_crop_region[0];
        srcRect.y = shot_stream->input_crop_region[1];
        srcRect.w = shot_stream->input_crop_region[2];
        srcRect.h = shot_stream->input_crop_region[3];
        srcRect.fullW = shot_stream->input_crop_region[2];
        srcRect.fullH = shot_stream->input_crop_region[3];
        srcRect.colorFormat = pictureFormat;

        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.w = pictureW;
        dstRect.h = pictureH;
        dstRect.fullW = pictureW;
        dstRect.fullH = pictureH;
        dstRect.colorFormat = JPEG_INPUT_COLOR_FMT;

        ret = getCropRectAlign(srcRect.w,  srcRect.h,
                               pictureW,   pictureH,
                               &srcRect.x, &srcRect.y,
                               &srcRect.w, &srcRect.h,
                               2, 2, 0);

        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
#endif

        ALOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
        ALOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);
        m_pictureFrameFactory->setOutputFrameQToPipe(dstGscReprocessingQ, pipeId_gsc);

        /* wait GSC */
        newFrame = NULL;
        ALOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        ret = dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
        if (newFrame == NULL) {
            ALOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto CLEAN;
        }
        ALOGI("INFO(%s[%d]):GSC output done", __FUNCTION__, __LINE__);

        /* put SCC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
        ret = m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
        if (ret < 0) {
            ALOGE("ERR(%s)(%d):m_putBuffers fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
    }

    /* push postProcess */
    m_postPictureQ->pushProcessQ(&newFrame);

    /* Shutter Callback */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
        ALOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback ", __FUNCTION__, __LINE__);
        m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
    }

    m_pictureCounter.decCount();

    ALOGI("INFO(%s[%d]):picture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0) {
        loop = true;
    } else {
//        if (isReprocessing() == true && m_exynosCameraParameters->getSeriesShotCount() == 0 &&
//                m_hdrEnabled == false)
        if (isReprocessing() == true)
            ALOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
        else {
            if (m_exynosCameraParameters->getUseDynamicScc() == true) {
                ALOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
                m_previewFrameFactory->setRequestSCC(false);
            }
            m_sccCaptureSelector->clearList(pipeId_scc, isSrc);
        }

        dstSccReprocessingQ->release();
    }

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2) {
        ALOGD("DEBUG(%s[%d]): putBuffer sccReprocessingBuffer(index:%d) in error state",
                __FUNCTION__, __LINE__, sccReprocessingBuffer.index);
        m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
    }

#if 0
    if (newFrame != NULL) {
        ALOGD("DEBUG(%s[%d]): print Entity state in error state", __FUNCTION__, __LINE__);
        newFrame->printEntity();
        ALOGD("DEBUG(%s[%d]): delete newFrame in error state", __FUNCTION__, __LINE__);
        delete newFrame;
        newFrame = NULL;
    }
#endif

    ALOGI("INFO(%s[%d]):take picture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0)
        loop = true;

    /* one shot */
    return loop;
}

camera_memory_t *ExynosCamera::m_getJpegCallbackHeap(ExynosCameraBuffer jpegBuf, int seriesShotNumber)
{
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    camera_memory_t *jpegCallbackHeap = NULL;

    {
        ALOGD("DEBUG(%s[%d]):general callback : size (%d)", __FUNCTION__, __LINE__, jpegBuf.size[0]);

        jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
        if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
            ALOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, jpegBuf.size[0]);
            goto done;
        }

        if (jpegBuf.fd[0] < 0)
            memcpy(jpegCallbackHeap->data, jpegBuf.addr[0], jpegBuf.size[0]);
    }

done:
    if (jpegCallbackHeap == NULL ||
        jpegCallbackHeap->data == MAP_FAILED) {

        if (jpegCallbackHeap) {
            jpegCallbackHeap->release(jpegCallbackHeap);
            jpegCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }

    ALOGD("INFO(%s[%d]):making callback buffer done", __FUNCTION__, __LINE__);

    return jpegCallbackHeap;
}

bool ExynosCamera::m_postPictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int bufIndex = -2;

    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer jpegReprocessingBuffer;

    gscReprocessingBuffer.index = -2;
    jpegReprocessingBuffer.index = -2;

    int pipeId_gsc = 0;
    int pipeId_jpeg = 0;

    int currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();

//    if (isReprocessing() == true && m_exynosCameraParameters->getSeriesShotCount() == 0 &&
//            m_hdrEnabled == false) {
    if (isReprocessing() == true) {
        pipeId_gsc = PIPE_SCC_REPROCESSING;
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        switch (getCameraId()) {
            case CAMERA_ID_BACK:
                if (needGSCForCapture(getCameraId()) == true)
                    pipeId_gsc = PIPE_GSC_PICTURE;
                else
                    pipeId_gsc = PIPE_SCC;

                pipeId_jpeg = PIPE_JPEG;
                break;
            case CAMERA_ID_FRONT:
                if (needGSCForCapture(getCameraId()) == true)
                    pipeId_gsc = PIPE_GSC_PICTURE_FRONT;
                else
                    pipeId_gsc = PIPE_SCC_FRONT;

                pipeId_jpeg = PIPE_JPEG_FRONT;
                break;
            default:
                ALOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",__FUNCTION__, __LINE__, getCameraId(), isReprocessing());
                break;
        }
    }

    ExynosCameraBufferManager *bufferMgr = NULL;
    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        return ret;
    }

    ALOGI("INFO(%s[%d]):wait postPictureQ output", __FUNCTION__, __LINE__);
    ret = m_postPictureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    if (newFrame == NULL) {
        ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_jpegCounter.getCount() <= 0) {
        ALOGD("DEBUG(%s[%d]): Picture canceled", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ALOGI("INFO(%s[%d]):postPictureQ output done", __FUNCTION__, __LINE__);

    /* put picture callback buffer */
    /* get gsc dst buffers */
    ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        goto CLEAN;
    }

    /* callback */
    if (m_hdrEnabled == false && m_exynosCameraParameters->getSeriesShotCount() <= 0) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
            ALOGD("DEBUG(%s[%d]): RAW callabck", __FUNCTION__, __LINE__);
            camera_memory_t *rawCallbackHeap = NULL;
            rawCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, rawCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            rawCallbackHeap->release(rawCallbackHeap);
        }

        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY)) {
            ALOGD("DEBUG(%s[%d]): RAW_IMAGE_NOTIFY callabck", __FUNCTION__, __LINE__);

            m_notifyCb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, m_callbackCookie);
        }

        if ((m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))
           ) {
            ALOGD("DEBUG(%s[%d]): POSTVIEW callabck", __FUNCTION__, __LINE__);

            camera_memory_t *postviewCallbackHeap = NULL;
            postviewCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            postviewCallbackHeap->release(postviewCallbackHeap);
        }
    }

    /* Make compressed image */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_exynosCameraParameters->getSeriesShotCount() > 0 ||
        m_hdrEnabled == true) {

        /* HDR callback */
        if (m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS) {
            ALOGD("DEBUG(%s[%d]): HDR callback", __FUNCTION__, __LINE__);

            /* send yuv image with jpeg callback */
            camera_memory_t    *jpegCallbackHeap = NULL;
            jpegCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);

            m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);

            jpegCallbackHeap->release(jpegCallbackHeap);

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            m_jpegCounter.decCount();
        } else {
            int retry = 0;

            /* 1. get wait available JPEG src buffer */
            do {
                bufIndex = -2;
                retry++;

                if (m_pictureEnabled == false) {
                    ALOGI("INFO(%s[%d]):m_pictureEnable is false", __FUNCTION__, __LINE__);
                    goto CLEAN;
                }
                if (m_jpegBufferMgr->getNumOfAvailableBuffer() > 0)
                    m_jpegBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &jpegReprocessingBuffer);

                if (bufIndex < 0) {
                    usleep(WAITING_TIME);

                    if (retry % 20 == 0) {
                        ALOGW("WRN(%s[%d]):retry JPEG getBuffer(%d) postPictureQ(%d)",
                                __FUNCTION__, __LINE__, bufIndex,
                                m_postPictureQ->getSizeOfProcessQ());
                        m_jpegBufferMgr->dump();
                    }
                }
                /* this will retry until 300msec */
            } while (bufIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_stopBurstShot == false);

            if (bufIndex < 0) {
                if (retry >= (TOTAL_WAITING_TIME / WAITING_TIME)) {
                    ALOGE("ERR(%s[%d]):getBuffer totally fail, retry(%d), m_stopBurstShot(%d)",
                            __FUNCTION__, __LINE__, retry, m_stopBurstShot);
                } else {
                    ALOGD("DEBUG(%s[%d]):getBuffer stopped, retry(%d), m_stopBurstShot(%d)",
                            __FUNCTION__, __LINE__, retry, m_stopBurstShot);
                }
                goto CLEAN;
            }

            /* 2. setup Frame Entity */
            ret = m_setupEntity(pipeId_jpeg, newFrame, &gscReprocessingBuffer, &jpegReprocessingBuffer);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                goto CLEAN;
            }

            /* 3. Q Set-up */
            m_pictureFrameFactory->setOutputFrameQToPipe(dstJpegReprocessingQ, pipeId_jpeg);

            /* 4. push the newFrame to pipe */
            m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_jpeg);

            /* 5. wait outputQ */
            ALOGI("INFO(%s[%d]):wait Jpeg output", __FUNCTION__, __LINE__);
            ret = dstJpegReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            }
            if (newFrame == NULL) {
                ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
                goto CLEAN;
            }

            /* When Front camera dose not setEntityState because Front camera share preview and capture frames */
            if (getCameraId() == CAMERA_ID_BACK) {
                ret = newFrame->setEntityState(pipeId_jpeg, ENTITY_STATE_COMPLETE);
                if (ret < 0) {
                    ALOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                    return ret;
                }
            }

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            int jpegOutputSize = newFrame->getJpegSize();
            ALOGI("INFO(%s[%d]):Jpeg output done, jpeg size(%d)", __FUNCTION__, __LINE__, jpegOutputSize);

            if (jpegOutputSize <= 0) {
                ALOGW("WRN(%s[%d]): jpegOutput size(%d) is invalid", __FUNCTION__, __LINE__, jpegOutputSize);
                jpegOutputSize = jpegReprocessingBuffer.size[0];
            }

            jpegReprocessingBuffer.size[0] = jpegOutputSize;

            /* push postProcess to call CAMERA_MSG_COMPRESSED_IMAGE */
            jpeg_callback_buffer_t jpegCallbackBuf;
            jpegCallbackBuf.buffer = jpegReprocessingBuffer;
retry:
            if ((m_exynosCameraParameters->getSeriesShotCount() > 0)
                ) {
                ;
            } else {
                jpegCallbackBuf.callbackNumber = 0;
                m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
            }

            m_jpegCounter.decCount();
        }
    } else {
        ALOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);

        /* put GSC buffer */
        ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        m_jpegCounter.decCount();
    }

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (newFrame->isComplete() == true) {
            ALOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            delete newFrame;
            newFrame = NULL;
        } else {
            ALOGW("WRN(%s[%d]): Picture frame(%d) is not completed", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        }
    }

    ALOGI("INFO(%s[%d]):postPicture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_jpegCounter.getCount());

    if (m_jpegCounter.getCount() <= 0) {
        if (m_hdrEnabled == true) {
            ALOGI("INFO(%s[%d]): End of HDR capture!", __FUNCTION__, __LINE__);
            m_hdrEnabled = false;
            m_pictureEnabled = false;
        }
        if (currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
            currentSeriesShotMode == SERIES_SHOT_MODE_SIS) {
            ALOGI("INFO(%s[%d]): End of LLS/SIS capture!", __FUNCTION__, __LINE__);
            m_pictureEnabled = false;
        }

        ALOGD("DEBUG(%s[%d]): free gsc buffers", __FUNCTION__, __LINE__);
        m_gscBufferMgr->deinit();

        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST) {
            ALOGD("DEBUG(%s[%d]): clearList postProcessList, series shot mode(%d)", __FUNCTION__, __LINE__, currentSeriesShotMode);
            if (m_clearList(&m_postProcessList) < 0) {
                ALOGE("ERR(%s):m_clearList fail", __FUNCTION__);
            }
        }
    }

    if (m_postPictureQ->getSizeOfProcessQ() > 0) {
        ALOGD("DEBUG(%s[%d]):postPicture thread will run again)", __func__, __LINE__);
         loop = true;
    } else {
#if 0
        if (m_clearList(&m_postProcessList) < 0) {
            ALOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        }
#endif
    }

    if (m_exynosCameraParameters->getScalableSensorMode()) {
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_CHANGING);
        ret = m_restartPreviewInternal();
        if (ret < 0)
            ALOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_NONE);
    }

    return loop;
CLEAN:

    if (m_postPictureQ->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;
}

bool ExynosCamera::m_jpegCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ALOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int retry = 0, maxRetry = 0;
    int loop = false;
    int seriesShotNumber = -1;

    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegCallbackBuffer;
    camera_memory_t *jpegCallbackHeap = NULL;

    jpegCallbackBuffer.index = -2;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

    do {
        ret = m_jpegCallbackQ->waitAndPopProcessQ(&jpegCallbackBuf);
        if (ret < 0) {
            retry++;
            ALOGW("WARN(%s[%d]):jpegCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
        }
    } while(ret < 0 && retry < maxRetry && m_jpegCounter.getCount() > 0);

    if (ret < 0) {
        ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        loop = true;
        goto CLEAN;
    }

    jpegCallbackBuffer = jpegCallbackBuf.buffer;
    seriesShotNumber = jpegCallbackBuf.callbackNumber;

    ALOGD("DEBUG(%s[%d]):jpeg calllback is start", __FUNCTION__, __LINE__);

    /* Make compressed image */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_exynosCameraParameters->getSeriesShotCount() > 0) {
            m_captureLock.lock();
            camera_memory_t *jpegCallbackHeap = m_getJpegCallbackHeap(jpegCallbackBuffer, seriesShotNumber);
            if (jpegCallbackHeap == NULL) {
                ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                android_printAssert(NULL, LOG_TAG, "Cannot recoverable, assert!!!!");
            }

            setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
            m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

            /* put JPEG callback buffer */
            if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                ALOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);

            jpegCallbackHeap->release(jpegCallbackHeap);
    } else {
        ALOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);
    }

CLEAN:
    ALOGI("INFO(%s[%d]):jpeg callback thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
    if (m_takePictureCounter.getCount() == 0) {
        m_pictureEnabled = false;
        m_clearJpegCallbackThread();
        m_captureLock.unlock();
    } else {
        m_captureLock.unlock();
    }

    return loop;
}

void ExynosCamera::m_clearJpegCallbackThread(void)
{
    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegCallbackBuffer;

    ALOGI("INFO(%s[%d]): takePicture disabled, takePicture callback done takePictureCounter(%d)",
            __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
    m_pictureEnabled = false;

    if (m_exynosCameraParameters->getUseDynamicScc() == true) {
        ALOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
        m_previewFrameFactory->setRequestSCC(false);
    }

    m_prePictureThread->requestExit();
    m_pictureThread->requestExit();
    m_postPictureThread->requestExit();
    m_jpegCallbackThread->requestExit();

    ALOGI("INFO(%s[%d]): wait m_prePictureThrad", __FUNCTION__, __LINE__);
    m_prePictureThread->requestExitAndWait();
    ALOGI("INFO(%s[%d]): wait m_pictureThrad", __FUNCTION__, __LINE__);
    m_pictureThread->requestExitAndWait();
    ALOGI("INFO(%s[%d]): wait m_postPictureThrad", __FUNCTION__, __LINE__);
    m_postPictureThread->requestExitAndWait();
    ALOGI("INFO(%s[%d]): wait m_jpegCallbackThrad", __FUNCTION__, __LINE__);
    m_jpegCallbackThread->requestExitAndWait();

    ALOGI("INFO(%s[%d]): All picture threads done", __FUNCTION__, __LINE__);

    while (m_jpegCallbackQ->getSizeOfProcessQ() > 0) {
        m_jpegCallbackQ->popProcessQ(&jpegCallbackBuf);
        jpegCallbackBuffer = jpegCallbackBuf.buffer;

        ALOGD("DEBUG(%s[%d]):put remaining jpeg buffer(index: %d)", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
            ALOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        }
    }

    m_burst[JPEG_SAVE_THREAD0] = false;
    m_burst[JPEG_SAVE_THREAD1] = false;
    m_burst[JPEG_SAVE_THREAD2] = false;

    ALOGD("DEBUG(%s[%d]): clear postProcessList", __FUNCTION__, __LINE__);
    if (m_clearList(&m_postProcessList) < 0) {
        ALOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }

#if 1
    ALOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    m_postPictureQ->release();

    ALOGD("DEBUG(%s[%d]): clear dstSccReprocessingQ", __FUNCTION__, __LINE__);
    dstSccReprocessingQ->release();
#else
    ExynosCameraFrame *frame = NULL;

    ALOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    while(m_postPictureQ->getSizeOfProcessQ()) {
        m_postPictureQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }

    ALOGD("DEBUG(%s[%d]): clear dstSccReprocessingQ", __FUNCTION__, __LINE__);
    while(dstSccReprocessingQ->getSizeOfProcessQ()) {
        dstSccReprocessingQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }
#endif

    ALOGD("DEBUG(%s[%d]): free gsc buffers", __FUNCTION__, __LINE__);
    m_gscBufferMgr->deinit();
    ALOGD("DEBUG(%s[%d]): free jpeg buffers", __FUNCTION__, __LINE__);
    m_jpegBufferMgr->deinit();
    ALOGD("DEBUG(%s[%d]): free sccReprocessing buffers", __FUNCTION__, __LINE__);
    m_sccReprocessingBufferMgr->resetBuffers();
}

bool ExynosCamera::m_highResolutionCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;

    ExynosCameraFrame *newFrame = NULL;
    camera2_stream *shot_stream = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBuffer highResolutionCbBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;

    int cbPreviewW = 0, cbPreviewH = 0;
    int previewFormat = 0;
    ExynosRect srcRect, dstRect;
    m_exynosCameraParameters->getPreviewSize(&cbPreviewW, &cbPreviewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    int pipeId_scc = 0;
    int pipeId_gsc = 0;

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount = getYuvPlaneCount(previewFormat);
    int minBufferCount = 1;
    int maxBufferCount = 1;

    sccReprocessingBuffer.index = -2;
    highResolutionCbBuffer.index = -2;

    pipeId_scc = PIPE_SCC_REPROCESSING;
    pipeId_gsc = PIPE_GSC_REPROCESSING;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == false &&
        m_highResolutionCallbackRunning == false) {
        ALOGD("DEBUG(%s[%d]): High Resolution Callback Stop", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == false &&
        m_highResolutionCallbackRunning == false) {
        ALOGD("DEBUG(%s[%d]): High Resolution Callback Stop", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, cbPreviewW, cbPreviewH);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): BAD value, format(%x), size(%dx%d)",
            __FUNCTION__, __LINE__, previewFormat, cbPreviewW, cbPreviewH);
        return ret;
    }

    /* wait SCC */
    ALOGV("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    ret = m_highResolutionCallbackQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        ALOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }
    ALOGV("INFO(%s[%d]):SCC output done", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        /* get GSC src buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[1]);

        /* alloc GSC buffer */
        if (m_highResolutionCallbackBufferMgr->isAllocated() == false) {
            ret = m_allocBuffers(m_highResolutionCallbackBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
            if (ret < 0) {
                ALOGE("ERR(%s[%d]):m_highResolutionCallbackBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
                return ret;
            }
        }

        /* get GSC dst buffer */
        int bufIndex = -2;
        m_highResolutionCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &highResolutionCbBuffer);

        /* get preview callback heap */
        camera_memory_t *previewCallbackHeap = NULL;
        previewCallbackHeap = m_getMemoryCb(highResolutionCbBuffer.fd[0], highResolutionCbBuffer.size[0], 1, m_callbackCookie);

        ret = m_setCallbackBufferInfo(&highResolutionCbBuffer, (char *)previewCallbackHeap->data);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }

        /* set src/dst rect */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];

        ret = m_calcHighResolutionPreviewGSCRect(&srcRect, &dstRect);
        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);

        ALOGV("DEBUG(%s[%d]):srcRect x : %d, y : %d, w : %d, h : %d", __FUNCTION__, __LINE__, srcRect.x, srcRect.y, srcRect.w, srcRect.h);
        ALOGV("DEBUG(%s[%d]):dstRect x : %d, y : %d, w : %d, h : %d", __FUNCTION__, __LINE__, dstRect.x, dstRect.y, dstRect.w, dstRect.h);

        ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, &highResolutionCbBuffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        /* push frame to GSC pipe */
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);
        m_pictureFrameFactory->setOutputFrameQToPipe(dstGscReprocessingQ, pipeId_gsc);

        /* wait GSC for high resolution preview callback */
        ALOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        ret = dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
        if (newFrame == NULL) {
            ALOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto CLEAN;
        }
        ALOGI("INFO(%s[%d]):GSC output done", __FUNCTION__, __LINE__);

        /* put SCC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);

        ALOGV("DEBUG(%s[%d]):high resolution preview callback", __FUNCTION__, __LINE__);
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }

        previewCallbackHeap->release(previewCallbackHeap);

        /* put high resolution callback buffer */
        ret = m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_putBuffers fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }
    }

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (newFrame->isComplete() == true) {
            ALOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            delete newFrame;
            newFrame = NULL;
        }
    }

    if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0) {
        ALOGD("DEBUG(%s[%d]):highResolutionCallbackQ size %d", __FUNCTION__, __LINE__, m_highResolutionCallbackQ->getSizeOfProcessQ());
        loop = true;
    }

    ALOGI("INFO(%s[%d]):high resolution callback thread complete", __FUNCTION__, __LINE__);

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2)
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
    if (highResolutionCbBuffer.index != -2)
        m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (newFrame->isComplete() == true) {
            ALOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            delete newFrame;
            newFrame = NULL;
        }
    }

    if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0)
        loop = true;

    ALOGI("INFO(%s[%d]):high resolution callback thread fail", __FUNCTION__, __LINE__);

    /* one shot */
    return loop;
}

status_t ExynosCamera::m_doPrviewToRecordingFunc(
        int32_t pipeId,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer recordingBuf)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    ALOGV("DEBUG(%s[%d]):--IN-- (previewBuf.index=%d, recordingBuf.index=%d)",
        __FUNCTION__, __LINE__, previewBuf.index, recordingBuf.index);

    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraFrame  *newFrame = NULL;

    newFrame = m_previewFrameFactory->createNewFrameVideoOnly();
    if (newFrame == NULL) {
        ALOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    /* csc and scaling */
    ret = m_calcRecordingGSCRect(&srcRect, &dstRect);
    ret = newFrame->setSrcRect(pipeId, srcRect);
    ret = newFrame->setDstRect(pipeId, dstRect);

    ret = m_setupEntity(pipeId, newFrame, &previewBuf, &recordingBuf);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, ret);
        ret = INVALID_OPERATION;
        if (newFrame != NULL) {
            delete newFrame;
            newFrame = NULL;
        }
        goto func_exit;
    }
    m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);

func_exit:

    ALOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);
    return ret;

}

bool ExynosCamera::m_recordingThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = false;
    int  pipeId = 0;
    nsecs_t timeStamp = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;

    if (getCameraId() == CAMERA_ID_BACK)
        pipeId = PIPE_GSC_VIDEO;
    else
        pipeId = PIPE_GSC_VIDEO_FRONT;

    ALOGV("INFO(%s[%d]):wait gsc done output", __FUNCTION__, __LINE__);
    ret = m_recordingQ->waitAndPopProcessQ(&frame);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    if (frame == NULL) {
        ALOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    ALOGV("INFO(%s[%d]):gsc done for recording callback", __FUNCTION__, __LINE__);

    ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    if (buffer.index < 0 || buffer.index >= (int)m_exynosconfig->current->bufInfo.num_recording_buffers) {
        ALOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, m_exynosconfig->current->bufInfo.num_recording_buffers, buffer.index);
        goto func_exit;
    }

    timeStamp = m_recordingTimeStamp[buffer.index];

    if (m_recordingStartTimeStamp == 0) {
        m_recordingStartTimeStamp = timeStamp;
        ALOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                __FUNCTION__, __LINE__, m_recordingStartTimeStamp);
    }

    if ((0L < timeStamp)
        && (m_lastRecordingTimeStamp < timeStamp)
        && (m_recordingStartTimeStamp <= timeStamp)) {
        if (m_recordingEnabled == true
            && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
            ALOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld",
                __FUNCTION__, __LINE__, buffer.index, timeStamp);
#endif
#ifdef DEBUG
            ALOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                __FUNCTION__, __LINE__,
                m_lastRecordingTimeStamp,
                systemTime(SYSTEM_TIME_MONOTONIC),
                m_recordingStartTimeStamp);
#endif
            struct addrs *recordAddrs = NULL;

            recordAddrs = (struct addrs *)m_recordingCallbackHeap->data;
            recordAddrs[buffer.index].type        = kMetadataBufferTypeCameraSource;
            recordAddrs[buffer.index].fdPlaneY    = (unsigned int)buffer.fd[0];
            recordAddrs[buffer.index].fdPlaneCbcr = (unsigned int)buffer.fd[1];
            recordAddrs[buffer.index].bufIndex    = buffer.index;

            m_dataCbTimestamp(
                    timeStamp,
                    CAMERA_MSG_VIDEO_FRAME,
                    m_recordingCallbackHeap,
                    buffer.index,
                    m_callbackCookie);
            m_lastRecordingTimeStamp = timeStamp;
        }
    } else {
        ALOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
            " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
            __FUNCTION__, __LINE__, buffer.index, timeStamp,
            m_lastRecordingTimeStamp,
            systemTime(SYSTEM_TIME_MONOTONIC),
            m_recordingStartTimeStamp);
        m_releaseRecordingBuffer(buffer.index);
    }

func_exit:

    if (frame != NULL) {
        delete frame;
        frame = NULL;
    }

    if (m_recordingQ->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;
}

status_t ExynosCamera::m_releaseRecordingBuffer(int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufIndex < 0 || bufIndex >= (int)m_exynosconfig->current->bufInfo.num_recording_buffers) {
        ALOGE("ERR(%s):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, m_exynosconfig->current->bufInfo.num_recording_buffers, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_recordingTimeStamp[bufIndex] = 0L;
    ret = m_putBuffers(m_recordingBufferMgr, bufIndex);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcHighResolutionPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcRecordingGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPictureRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPictureRect(originW, originH, srcRect, dstRect);
}

status_t ExynosCamera::m_searchFrameFromList(List<ExynosCameraFrame *> *list, uint32_t frameCount, ExynosCameraFrame **frame)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    if (list->empty()) {
        ALOGD("DEBUG(%s[%d]):list is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            ALOGE("ERR(%s):curFrame is empty", __FUNCTION__);
            return INVALID_OPERATION;
        }

        if (frameCount == curFrame->getFrameCount()) {
            ALOGV("DEBUG(%s):frame count match: expected(%d)", __FUNCTION__, frameCount);
            *frame = curFrame;
            return NO_ERROR;
        }
        r++;
    } while (r != list->end());

    ALOGV("DEBUG(%s[%d]):Cannot find match frame, frameCount(%d)", __FUNCTION__, __LINE__, frameCount);

    return NO_ERROR;
}

status_t ExynosCamera::m_removeFrameFromList(List<ExynosCameraFrame *> *list, ExynosCameraFrame *frame)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrame *>::iterator r;

    if (frame == NULL) {
        ALOGE("ERR(%s):frame is NULL", __FUNCTION__);
        return BAD_VALUE;
    }

    if (list->empty()) {
        ALOGE("ERR(%s):list is empty", __FUNCTION__);
        return INVALID_OPERATION;
    }

    frameCount = frame->getFrameCount();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            ALOGE("ERR(%s):curFrame is empty", __FUNCTION__);
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        if (frameCount == curFrameCount) {
            ALOGV("DEBUG(%s):frame count match: expected(%d), current(%d)", __FUNCTION__, frameCount, curFrameCount);
            list->erase(r);
            return NO_ERROR;
        }
        ALOGW("WARN(%s):frame count mismatch: expected(%d), current(%d)", __FUNCTION__, frameCount, curFrameCount);
        /* removed message */
        /* curFrame->printEntity(); */
        r++;
    } while (r != list->end());

    ALOGE("ERR(%s):Cannot find match frame!!!", __FUNCTION__);

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_clearList(List<ExynosCameraFrame *> *list)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    ALOGD("DEBUG(%s):remaining frame(%d), we remove them all", __FUNCTION__, list->size());

    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL && curFrame->getFrameLockState() == false) {
            ALOGV("DEBUG(%s):remove frame count %d", __FUNCTION__, curFrame->getFrameCount() );
            delete curFrame;
            curFrame = NULL;
        }
        list->erase(r);
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_printFrameList(List<ExynosCameraFrame *> *list)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    ALOGE("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    ALOGD("\t remaining frame count(%d)", list->size());

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame != NULL) {
            ALOGI("\t hal frame count %d", curFrame->getFrameCount() );
            curFrame->printEntity();
        }

        r++;
    } while (r != list->end());
    ALOGE("----------------------------------------------------------------------------");

    return NO_ERROR;
}

status_t ExynosCamera::m_createIonAllocator(ExynosCameraIonAllocator **allocator)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    do {
        retry++;
        ALOGI("INFO(%s[%d]):try(%d) to create IonAllocator", __FUNCTION__, __LINE__, retry);
        *allocator = new ExynosCameraIonAllocator();
        ret = (*allocator)->init(false);
        if (ret < 0)
            ALOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        else {
            ALOGD("DEBUG(%s[%d]):m_createIonAllocator success (allocator=%p)", __FUNCTION__, __LINE__, *allocator);
            break;
        }
    } while (ret < 0 && retry < 3);

    if (ret < 0 && retry >=3) {
        ALOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCamera::m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name)
{
    return m_createBufferManager(bufferManager, name, BUFFER_MANAGER_ION_TYPE);
}

status_t ExynosCamera::m_createBufferManager(
        ExynosCameraBufferManager **bufferManager,
        const char *name,
        buffer_manager_type type)
{
    status_t ret = NO_ERROR;

    if (m_ionAllocator == NULL) {
        ret = m_createIonAllocator(&m_ionAllocator);
        if (ret < 0)
            ALOGE("ERR(%s[%d]):m_createIonAllocator fail", __FUNCTION__, __LINE__);
        else
            ALOGD("DEBUG(%s[%d]):m_createIonAllocator success", __FUNCTION__, __LINE__);
    }

    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_ionAllocator);

    ALOGD("DEBUG(%s):BufferManager(%s) created", __FUNCTION__, name);

    return ret;
}

bool ExynosCamera::m_releasebuffersForRealloc()
{
    status_t ret = NO_ERROR;
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    ALOGE(" m_setBuffers free all buffers");
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }

    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }

#ifdef SAMSUNG_MAGICSHOT
    if (m_frontgscBufferMgr != NULL) {
        m_frontgscBufferMgr->deinit();
    }
#endif

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    m_exynosCameraParameters->setReallocBuffer(false);

    if (m_exynosCameraParameters->getRestartPreview() == true) {
        ret = setPreviewWindow(m_previewWindow);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        ALOGE("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);
        if (ret < 0) {
            ALOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

   return true;
}


status_t ExynosCamera::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);
    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW, hwPreviewH;
    int hwPictureW, hwPictureH;

    int previewMaxW, previewMaxH;
    int sensorMaxW, sensorMaxH;

    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

#if 1
    if( m_exynosCameraParameters->getReallocBuffer() ) {
        /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
        m_releasebuffersForRealloc();
    }
#endif

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    ALOGI("(%s):HW Preview width x height = %dx%d", __FUNCTION__, hwPreviewW, hwPreviewH);
    m_exynosCameraParameters->getHwPictureSize(&hwPictureW, &hwPictureH);
    ALOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
    if( m_exynosCameraParameters->getHighSpeedRecording() ) {
        m_exynosCameraParameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        ALOGI("(%s):HW Sensor(HighSpeed) MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
        m_exynosCameraParameters->getHwPreviewSize(&previewMaxW, &previewMaxH);
        ALOGI("(%s):HW Preview(HighSpeed) MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
    } else {
        m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        ALOGI("(%s):HW Sensor MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
        m_exynosCameraParameters->getMaxPreviewSize(&previewMaxW, &previewMaxH);
        ALOGI("(%s):HW Preview MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
    }

    /* FLITE */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = (sensorMaxW + 16) * 2;
        planeSize[0] = (sensorMaxW + 16) * (sensorMaxH + 10) * 2;
    } else
#endif /* DEBUG_RAWDUMP */
    {
        bytesPerLine[0] = ROUND_UP((sensorMaxW + 16), 10) * 8 / 5;
        planeSize[0]    = bytesPerLine[0] * (sensorMaxH + 10);
    }
#else
    planeSize[0] = (sensorMaxW + 16) * (sensorMaxH + 10) * 2;
#endif
    planeCount  = 2;

    /* TO DO : make num of buffers samely */
    if (getCameraId() == CAMERA_ID_BACK) {
        maxBufferCount =  m_exynosconfig->current->bufInfo.num_bayer_buffers;
#ifdef RESERVED_MEMORY_ENABLE
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#endif
    } else {
        maxBufferCount = m_exynosconfig->current->bufInfo.front_num_bayer_buffers;
    }

    ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif

    /* for preview */
    planeSize[0] = 32 * 64 * 2;
    planeCount  = 2;
    /* TO DO : make num of buffers samely */
    if (getCameraId())
        maxBufferCount = m_exynosconfig->current->bufInfo.front_num_bayer_buffers;
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_exynosCameraParameters->checkBayerDumpEnable()) {
            bytesPerLine[0] = previewMaxW * 2;
            planeSize[0] = previewMaxW * previewMaxH * 2;
        } else
#endif /* DEBUG_RAWDUMP */
        {
            bytesPerLine[0] = ROUND_UP((sensorMaxW * 3 / 2), 16);
            planeSize[0]    = bytesPerLine[0] * sensorMaxH;
        }
#else
        /* planeSize[0] = width * height * 2; */
        planeSize[0] = previewMaxW * previewMaxH * 2;
#endif
    } else {
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_exynosCameraParameters->checkBayerDumpEnable()) {
            bytesPerLine[0] = sensorMaxW * 2;
            planeSize[0] = sensorMaxW * sensorMaxH * 2;
        } else
#endif /* DEBUG_RAWDUMP */
        {
            bytesPerLine[0] = ROUND_UP((sensorMaxW * 3 / 2), 16);
            planeSize[0]    = bytesPerLine[0] * sensorMaxH;
        }
#else
        /* planeSize[0] = width * height * 2; */
        planeSize[0] = sensorMaxW * sensorMaxH * 2;
#endif
    }
    planeCount  = 2;
    /* TO DO : make num of buffers samely */
    if (getCameraId())
        maxBufferCount = m_exynosconfig->current->bufInfo.front_num_bayer_buffers;
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

    ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    planeSize[0] = hwPreviewW * hwPreviewH;
    planeSize[1] = hwPreviewW * hwPreviewH / 2;
    planeCount  = 3;
    if( m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE ) {
        maxBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
    } else {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
    }


    bool needMmap = false;
    if (m_previewWindow == NULL)
        needMmap = true;

    ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

#ifdef USE_BUFFER_WITH_STRIDE
    int stride = m_scpBufferMgr->getBufStride();
    if (stride != hwPreviewW) {
        ALOGI("INFO(%s[%d]):hwPreviewW(%d), stride(%d)", __FUNCTION__, __LINE__, hwPreviewW, stride);
        if (stride == 0) {
            /* If the SCP buffer manager is not instance of GrallocExynosCameraBufferManager
               (In case of setPreviewWindow(null) is called), return value of setHwPreviewStride()
               will be zero. If this value is passed as SCP width to firmware, firmware will
               generate PABORT error. */
            ALOGW("WARN(%s[%d]):HACK: Invalid stride(%d). It will be replaced as hwPreviewW(%d) value.",
                __FUNCTION__, __LINE__, stride, hwPreviewW);
            stride = hwPreviewW;
        }
    }
    m_exynosCameraParameters->setHwPreviewStride(stride);
#endif

    if (getCameraId() == CAMERA_ID_FRONT) {
        m_exynosCameraParameters->getHwPictureSize(&hwPictureW, &hwPictureH);
        ALOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 2;
        planeCount  = 2;
        /* TO DO : make same num of buffers */
        if (getCameraId())
            maxBufferCount = m_exynosconfig->current->bufInfo.front_num_bayer_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

        ret = m_allocBuffers(m_sccBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_sccBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

    ALOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setReprocessingBuffer(void)
{
    int ret = 0;
    int pictureMaxW, pictureMaxH;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount  = 0;
    int bufferCount = 0;
    int minBufferCount = NUM_REPROCESSING_BUFFERS;
    int maxBufferCount = NUM_PICTURE_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_exynosCameraParameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    ALOGI("(%s):HW Picture MAX width x height = %dx%d", __FUNCTION__, pictureMaxW, pictureMaxH);

    /* for reprocessing */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = pictureMaxW * 2;
        planeSize[0] = pictureMaxW * pictureMaxH * 2;
    } else
#endif /* DEBUG_RAWDUMP */
    {
        bytesPerLine[0] = ROUND_UP((pictureMaxW * 3 / 2), 16);
        planeSize[0] = bytesPerLine[0] * pictureMaxH;
    }
#else
    planeSize[0] = pictureMaxW * pictureMaxH * 2;
#endif
    planeCount  = 2;
    bufferCount = NUM_REPROCESSING_BUFFERS;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        /* ISP Reprocessing Buffer realloc for high resolution callback */
        minBufferCount = 2;
    }

    ret = m_allocBuffers(m_ispReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
            __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setPreviewCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int planeCount  = getYuvPlaneCount(previewFormat);
    int bufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    if (m_previewCallbackBufferMgr == NULL) {
        ALOGE("ERR(%s[%d]): m_previewCallbackBufferMgr is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_previewCallbackBufferMgr->isAllocated() == true) {
        if (m_exynosCameraParameters->getRestartPreview() == true) {
            ALOGD("DEBUG(%s[%d]): preview size is changed, realloc buffer", __FUNCTION__, __LINE__);
            m_previewCallbackBufferMgr->deinit();
        } else {
            return NO_ERROR;
        }
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, previewW, previewH);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): BAD value, format(%x), size(%dx%d)",
            __FUNCTION__, __LINE__, previewFormat, previewW, previewH);
        return ret;
    }

    ret = m_allocBuffers(m_previewCallbackBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, bufferCount, type, false, false);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setPictureBuffer(void)
{
    int ret = 0;
    unsigned int planeSize[3] = {0};
    unsigned int bytesPerLine[3] = {0};
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int planeCount = 0;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
    pictureFormat = m_exynosCameraParameters->getPictureFormat();

    if ((needGSCForCapture(getCameraId()) == true)
        ) {
        {
            planeSize[0] = pictureW * pictureH * 2;
        }
        planeCount = 1;
        minBufferCount = 1;
        /* TO DO : make num of buffers samely */
        if (getCameraId())
            maxBufferCount = m_exynosconfig->current->bufInfo.front_num_picture_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_exynosCameraParameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;

        ret = m_allocBuffers(m_gscBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }
    }

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) == true
        && m_hdrEnabled == false) {
        planeSize[0] = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
        planeCount = 1;
        minBufferCount = 1;
        /* TO DO : make num of buffers samely */
        if (getCameraId())
            maxBufferCount = m_exynosconfig->current->bufInfo.front_num_picture_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;


        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        ALOGD("DEBUG(%s[%d]): jpegBuffer picture(%dx%d) size(%d)", __FUNCTION__, __LINE__, pictureW, pictureH, planeSize[0]);

        // Same with above GSC buffers
        if (m_exynosCameraParameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;

        ret = m_allocBuffers(m_jpegBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
        if (ret < 0)
            ALOGE("ERR(%s:%d):jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, NUM_REPROCESSING_BUFFERS);
    }

    return ret;
}

status_t ExynosCamera::m_releaseBuffers(void)
{
    ALOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
    }
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->deinit();
    }

    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    ALOGI("INFO(%s[%d]):free buffer done", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex)
{
    if (bufManager != NULL)
        bufManager->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);

    return NO_ERROR;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  reqBufCount,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = m_allocBuffers(
                bufManager,
                planeCount,
                planeSize,
                bytePerLine,
                reqBufCount,
                reqBufCount,
                EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE,
                BUFFER_MANAGER_ALLOCATION_ATONCE,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_allocBuffers(reqBufCount=%d) fail",
            __FUNCTION__, __LINE__, reqBufCount);
    }

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        exynos_camera_buffer_type_t type,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = m_allocBuffers(
                bufManager,
                planeCount,
                planeSize,
                bytePerLine,
                minBufCount,
                maxBufCount,
                type,
                BUFFER_MANAGER_ALLOCATION_ONDEMAND,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):m_allocBuffers(minBufCount=%d, maxBufCount=%d, type=%d) fail",
            __FUNCTION__, __LINE__, minBufCount, maxBufCount, type);
    }

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        exynos_camera_buffer_type_t type,
        buffer_manager_allocation_mode_t allocMode,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ALOGI("INFO(%s[%d]):setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, type=%d, allocMode=%d)",
        __FUNCTION__, __LINE__, planeCount, minBufCount, maxBufCount, (int)type, (int)allocMode);

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        minBufCount,
                        maxBufCount,
                        type,
                        allocMode,
                        createMetaPlane,
                        needMmap);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setInfo fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):alloc fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_checkThreadState(int *threadState, int *countRenew)
{
    int ret = NO_ERROR;

    if ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT)) {
        ALOGW("WRN(%s[%d]:SCP DQ Timeout! State:[%d], Duration:%d msec", __FUNCTION__, __LINE__, *threadState, (*countRenew)*(MONITOR_THREAD_INTERVAL/1000));
        ret = false;
    } else {
        ALOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
        ret = NO_ERROR;
    }

    return ret;
}

status_t ExynosCamera::m_checkThreadInterval(uint32_t pipeId, uint32_t pipeInterval, int *threadState)
{
    uint64_t *threadInterval;
    int ret = NO_ERROR;

    m_previewFrameFactory->getThreadInterval(&threadInterval, pipeId);
    if (*threadInterval > pipeInterval) {
        ALOGW("WRN(%s[%d]:Pipe(%d) Thread Interval [%lld msec], State:[%d]", __FUNCTION__, __LINE__, pipeId, (*threadInterval)/1000, *threadState);
        ret = false;
    } else {
        ALOGV("Thread IntervalTime [%lld]", *threadInterval);
        ALOGV("Thread Renew Count [%d]", *countRenew);
        ret = NO_ERROR;
    }

    return ret;
}

bool ExynosCamera::m_monitorThreadFunc(void)
{
    ALOGV("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int *threadState;
    int *countRenew;
    int camId = getCameraId();
    int ret = NO_ERROR;


#if 0
    /* check PIPE_SCP thread state & interval */
    m_previewFrameFactory->getThreadState(&threadState, PIPE_SCP);
    m_previewFrameFactory->getThreadRenew(&countRenew, PIPE_SCP);
    m_checkThreadState(threadState, countRenew)?:ret = false;

    if (ret == false) {
        dump();

        /* in GED */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */
        /* specifically defined */
        m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie);
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
    }
    m_checkThreadInterval(PIPE_SCP, WARNING_SCP_THREAD_INTERVAL, threadState)?:ret = false;

    /* check PIPE_3AA thread state & interval */
    if(camId == CAMERA_ID_BACK) {
        m_previewFrameFactory->getThreadRenew(&countRenew, PIPE_3AA_ISP);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            dump();

            /* in GED */
            m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
            /* specifically defined */
            /* m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie); */
            /* or */
            android_printAssert(NULL, LOG_TAG, "killed by itself");
        }

        m_checkThreadInterval(PIPE_3AA_ISP, WARNING_3AA_THREAD_INTERVAL, threadState)?:ret = false;
    }
    else if(camId == CAMERA_ID_FRONT) {
        m_previewFrameFactory->getThreadRenew(&countRenew, PIPE_3AA_FRONT);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            dump();

            /* in GED */
            m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
            /* specifically defined */
            /* m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie); */
            /* or */
            android_printAssert(NULL, LOG_TAG, "killed by itself");
        }

        m_checkThreadInterval(PIPE_3AA_FRONT, WARNING_3AA_THREAD_INTERVAL, threadState)?:ret = false;
    }
    else {
        ALOGE("ERR(%s[%d]):Invalid Camera ID [%d]", __FUNCTION__, __LINE__, camId);
    }

    if (m_callbackState == 0) {
        m_callbackStateOld = 0;
        m_callbackState = 0;
        m_callbackMonitorCount = 0;
    } else {
        if (m_callbackStateOld != m_callbackState) {
            m_callbackStateOld = m_callbackState;
            ALOGD("INFO(%s[%d]):callback state is updated (0x%x)", __FUNCTION__, __LINE__, m_callbackStateOld);
        } else {
            if ((m_callbackStateOld & m_callbackState) != 0)
                ALOGE("ERR(%s[%d]):callback is blocked (0x%x), Duration:%d msec", __FUNCTION__, __LINE__, m_callbackState, m_callbackMonitorCount*(MONITOR_THREAD_INTERVAL/1000));
        }
    }
#endif

    m_previewFrameFactory->incThreadRenew(PIPE_SCP);

    usleep(MONITOR_THREAD_INTERVAL);

    return true;
}

bool ExynosCamera::m_autoFocusResetNotify(int focusMode)
{
    /* show restart */
    ALOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, 4, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, 4, 0, m_callbackCookie);

    /* show focusing */
    ALOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, 3, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, 3, 0, m_callbackCookie);

    return true;
}

bool ExynosCamera::m_autoFocusThreadFunc(void)
{
    ALOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    bool afResult = false;
    int focusMode = 0;

    /* block until we're told to start.  we don't want to use
     * a restartable thread and requestExitAndWait() in cancelAutoFocus()
     * because it would cause deadlock between our callbacks and the
     * caller of cancelAutoFocus() which both want to grab the same lock
     * in CameraServices layer.
     */

    if (m_autoFocusType == AUTO_FOCUS_SERVICE) {
        focusMode = m_exynosCameraParameters->getFocusMode();
    } else if (m_autoFocusType == AUTO_FOCUS_HAL) {
        focusMode = FOCUS_MODE_AUTO;

        m_autoFocusResetNotify(focusMode);

        m_autoFocusLock.lock();
    }

    /* check early exit request */
    if (m_exitAutoFocusThread == true) {
        ALOGD("DEBUG(%s):exiting on request", __FUNCTION__);
        goto done;
    }

    m_autoFocusRunning = true;

    if (m_autoFocusRunning == true) {
        afResult = m_exynosCameraActivityControl->autoFocus(focusMode, m_autoFocusType);
        if (afResult == true)
            ALOGV("DEBUG(%s):autoFocus Success!!", __FUNCTION__);
        else
            ALOGV("DEBUG(%s):autoFocus Fail !!", __FUNCTION__);
    } else {
        ALOGV("DEBUG(%s):autoFocus canceled !!", __FUNCTION__);
    }

    /*
     * CAMERA_MSG_FOCUS only takes a bool.  true for
     * finished and false for failure.
     * If cancelAutofocus() called, no callback.
     */
    if ((m_autoFocusRunning == true) &&
        m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {

        if (m_notifyCb != NULL) {
            int afFinalResult = (int)afResult;

            /* if inactive detected, tell it */
            if (focusMode == FOCUS_MODE_CONTINUOUS_PICTURE) {
                if (m_exynosCameraActivityControl->getCAFResult() == 2) {
                    afFinalResult = 2;
                }
            }

            ALOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __FUNCTION__, afFinalResult, focusMode);
            m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
        }  else {
            ALOGD("DEBUG(%s):m_notifyCb is NULL mode(%d)", __FUNCTION__, focusMode);
        }
    } else {
        ALOGV("DEBUG(%s):autoFocus canceled, no callback !!", __FUNCTION__);
    }

    m_autoFocusRunning = false;

    ALOGV("DEBUG(%s):exiting with no error", __FUNCTION__);

done:
    m_autoFocusLock.unlock();

    ALOGI("DEBUG(%s):end", __FUNCTION__);

    return false;
}

status_t ExynosCamera::dump(int fd) const
{
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCamera::dump()
{
    ALOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->dump();

    if (m_bayerBufferMgr != NULL)
        m_bayerBufferMgr->dump();
    if (m_3aaBufferMgr != NULL)
        m_3aaBufferMgr->dump();
    if (m_ispBufferMgr != NULL)
        m_ispBufferMgr->dump();
    if (m_scpBufferMgr != NULL)
        m_scpBufferMgr->dump();

    if (m_ispReprocessingBufferMgr != NULL)
        m_ispReprocessingBufferMgr->dump();
    if (m_sccReprocessingBufferMgr != NULL)
        m_sccReprocessingBufferMgr->dump();
    if (m_sccBufferMgr != NULL)
        m_sccBufferMgr->dump();
    if (m_gscBufferMgr != NULL)
        m_gscBufferMgr->dump();

    return;
}

status_t ExynosCamera::m_getBufferManager(uint32_t pipeId, ExynosCameraBufferManager **bufMgr, uint32_t direction)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager **bufMgrList[2] = {NULL};
    *bufMgr = NULL;

    if (getCameraId() == CAMERA_ID_BACK) {
        switch (pipeId) {
        case PIPE_FLITE:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_bayerBufferMgr;
            break;
        case PIPE_3AA_ISP:
            bufMgrList[0] = &m_3aaBufferMgr;
            bufMgrList[1] = &m_ispBufferMgr;
            break;
        case PIPE_3AC:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_bayerBufferMgr;
            break;
        case PIPE_SCP:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_scpBufferMgr;
            break;
        case PIPE_3AA_REPROCESSING:
            bufMgrList[0] = &m_bayerBufferMgr;
            bufMgrList[1] = &m_ispReprocessingBufferMgr;
            break;
        case PIPE_ISP_REPROCESSING:
            bufMgrList[0] = &m_ispReprocessingBufferMgr;
            bufMgrList[1] = NULL;
            break;
#if 0 // TODO: need to commonize
        case PIPE_SCC_REPROCESSING:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_sccBufferMgr;
            break;
        case PIPE_GSC_REPROCESSING:
            bufMgrList[0] = &m_sccBufferMgr;
            bufMgrList[1] = &m_gscBufferMgr;
            break;
        case PIPE_ISP_REPROCESSING:
            bufMgrList[0] = &m_bayerBufferMgr;
            bufMgrList[1] = NULL;
            break;
#endif
        case PIPE_SCC_REPROCESSING:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_sccReprocessingBufferMgr;
            break;
        case PIPE_GSC_REPROCESSING:
            bufMgrList[0] = &m_sccReprocessingBufferMgr;
            bufMgrList[1] = &m_gscBufferMgr;
            break;
        default:
            ALOGE("ERR(%s[%d]): Unknown pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            bufMgrList[0] = NULL;
            bufMgrList[1] = NULL;
            ret = BAD_VALUE;
            break;
        }
    } else {
        switch (pipeId) {
        case PIPE_FLITE_FRONT:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_bayerBufferMgr;
            break;
        case PIPE_3AA_FRONT:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_ispBufferMgr;
            break;
        case PIPE_ISP_FRONT:
            bufMgrList[0] = NULL;
            bufMgrList[1] = NULL;
            break;
        case PIPE_SCC_FRONT:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_sccBufferMgr;
            break;
        case PIPE_SCP_FRONT:
            bufMgrList[0] = NULL;
            bufMgrList[1] = &m_scpBufferMgr;
            break;
        case PIPE_GSC_PICTURE_FRONT:
            bufMgrList[0] = &m_sccBufferMgr;
            bufMgrList[1] = &m_gscBufferMgr;
            break;
        default:
            ALOGE("ERR(%s[%d]): Unknown pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            bufMgrList[0] = NULL;
            bufMgrList[1] = NULL;
            ret = BAD_VALUE;
            break;
        }
    }

    if (bufMgrList[direction] != NULL)
        *bufMgr = *bufMgrList[direction];

    return ret;
}

uint32_t ExynosCamera::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true)
        pipeId = PIPE_FLITE;
    else
        pipeId = PIPE_3AC;

    return pipeId;
}

void ExynosCamera::m_debugFpsCheck(uint32_t pipeId)
{
#ifdef FPS_CHECK
    uint32_t id = pipeId % DEBUG_MAX_PIPE_NUM;

    m_debugFpsCount[id]++;
    if (m_debugFpsCount[id] == 1) {
        m_debugFpsTimer[id].start();
    }
    if (m_debugFpsCount[id] == 30) {
        m_debugFpsTimer[id].stop();
        long long durationTime = m_debugFpsTimer[id].durationMsecs();
        ALOGI("DEBUG: FPS_CHECK(id:%d), duration %lld / 30 = %lld ms", pipeId, durationTime, durationTime / 30);
        m_debugFpsCount[id] = 0;
    }
#endif
}

status_t ExynosCamera::m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo)
{
/* TODO: HACK: Will be removed, this is driver's job */
    status_t ret = NO_ERROR;
    int bayerFrameCount = 0;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;

    shot_stream = (struct camera2_stream *)buffer->addr[1];
    bayerFrameCount = shot_stream->fcount;
    outputInfo->cropRegion[0] = shot_stream->output_crop_region[0];
    outputInfo->cropRegion[1] = shot_stream->output_crop_region[1];
    outputInfo->cropRegion[2] = shot_stream->output_crop_region[2];
    outputInfo->cropRegion[3] = shot_stream->output_crop_region[3];

    memset(buffer->addr[1], 0x0, sizeof(struct camera2_shot_ext));

    shot_ext = (struct camera2_shot_ext *)buffer->addr[1];
    shot_ext->shot.dm.request.frameCount = bayerFrameCount;

    return ret;
}

status_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    bool isSrc = false;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;

    m_captureSelector->setWaitTime(200000000);
    ExynosCameraFrame *bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
    if (bayerFrame == NULL) {
        ALOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto ERR;
    }

    ret = bayerFrame->getDstBuffer(pipeId, buffer);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto ERR;
    }

    shot_ext = (struct camera2_shot_ext *)buffer->addr[1];

    ALOGD("DEBUG(%s[%d]): Selected frame count(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
                                                                         bayerFrame->getFrameCount(),
                                                                         shot_ext->shot.dm.request.frameCount);
    return ret;

ERR:
    if (bayerFrame != NULL && bayerFrame->isComplete() == true) {
        if (bayerFrame->getFrameLockState() == false) {
            ALOGD("DEBUG(%s[%d]): Selected frame(%d) complete, Delete", __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
            delete bayerFrame;
        }
        bayerFrame = NULL;
    }

    return ret;
}

status_t ExynosCamera::m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr)
{
    status_t ret = TIMED_OUT;
    int retry = 0;

    do {
        ret = -1;
        retry++;
        if (bufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = OK;
        } else {
            /* wait available ISP buffer */
            usleep(WAITING_TIME);
        }
        if (retry % 10 == 0)
            ALOGW("WRAN(%s[%d]):retry(%d) setupEntity for pipeId(%d)", __FUNCTION__, __LINE__, retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME));

    return ret;
}

status_t ExynosCamera::m_boostDynamicCapture(void)
{
    status_t ret = NO_ERROR;
#if 0 /* TODO: need to implementation for bayer */
    uint32_t pipeId = PIPE_SCC;
    uint32_t size = m_processList.size();

    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;
    camera2_node_group node_group_info_isp;

    if (m_processList.empty()) {
        ALOGD("DEBUG(%s[%d]):m_processList is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }
    ALOGD("DEBUG(%s[%d]):m_processList size(%d)", __FUNCTION__, __LINE__, m_processList.size());
    r = m_processList.end();

    for (unsigned int i = 0; i < 3; i++) {
        r--;
        if (r == m_processList.begin())
            break;

    }

    curFrame = *r;
    if (curFrame == NULL) {
        ALOGE("ERR(%s):curFrame is empty", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (curFrame->getRequest(PIPE_SCC) == true) {
        ALOGD("DEBUG(%s[%d]): Boosting dynamic capture is not need", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    ALOGI("INFO(%s[%d]): boosting dynamic capture (frameCount: %d)", __FUNCTION__, __LINE__, curFrame->getFrameCount());
    /* For ISP */
    curFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    m_updateBoostDynamicCaptureSize(&node_group_info_isp);
    curFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

    curFrame->setRequest(pipeId, true);
    curFrame->setNumRequestPipe(curFrame->getNumRequestPipe() + 1);

    ret = curFrame->setEntityState(pipeId, ENTITY_STATE_REWORK);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ENTITY_STATE_REWORK, ret);
        return ret;
    }

    m_previewFrameFactory->pushFrameToPipe(&curFrame, pipeId);
    m_dynamicSccCount++;
    ALOGV("DEBUG(%s[%d]): dynamicSccCount inc(%d) frameCount(%d)", __FUNCTION__, __LINE__, m_dynamicSccCount, curFrame->getFrameCount());
#endif

    return ret;
}

void ExynosCamera::m_updateBoostDynamicCaptureSize(camera2_node_group *node_group_info)
{
#if 0 /* TODO: need to implementation for bayer */
    ExynosRect sensorSize;
    ExynosRect bayerCropSize;

    node_group_info->capture[PERFRAME_BACK_SCC_POS].request = 1;

    m_exynosCameraParameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);

    node_group_info->leader.input.cropRegion[0] = bayerCropSize.x;
    node_group_info->leader.input.cropRegion[1] = bayerCropSize.y;
    node_group_info->leader.input.cropRegion[2] = bayerCropSize.w;
    node_group_info->leader.input.cropRegion[3] = bayerCropSize.h;
    node_group_info->leader.output.cropRegion[0] = 0;
    node_group_info->leader.output.cropRegion[1] = 0;
    node_group_info->leader.output.cropRegion[2] = node_group_info->leader.input.cropRegion[2];
    node_group_info->leader.output.cropRegion[3] = node_group_info->leader.input.cropRegion[3];

    /* Capture 0 : SCC - [scaling] */
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[0] = node_group_info->leader.output.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[1] = node_group_info->leader.output.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[0] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[1] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[2] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[3] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[3];

    /* Capture 1 : SCP - [scaling] */
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[0] = node_group_info->leader.output.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[1] = node_group_info->leader.output.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

#endif
    return;
}

void ExynosCamera::m_checkFpsAndUpdatePipeWaitTime(void)
{
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    frame_queue_t *inputFrameQ = NULL;

    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_curMinFps != curMinFps) {
        ALOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);
        m_previewFrameFactory->getInputFrameQToPipe(&inputFrameQ, PIPE_SCC);

        /* 100ms * (30 / 15 fps) = 200ms */
        /* 100ms * (30 / 30 fps) = 100ms */
        /* 100ms * (30 / 10 fps) = 300ms */
        if (inputFrameQ != NULL && curMinFps != 0)
            inputFrameQ->setWaitTime((100000000 * (30 / curMinFps)));
    }

    m_curMinFps = curMinFps;

    return;
}

}; /* namespace android */
