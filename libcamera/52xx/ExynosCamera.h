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

#ifndef EXYNOS_CAMERA_HW_IMPLEMENTATION_H
#define EXYNOS_CAMERA_HW_IMPLEMENTATION_H

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <hardware/gralloc.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <media/hardware/MetadataBufferType.h>

#include <fcntl.h>
#include <sys/mman.h>
#include "csc.h"

#include "ExynosCameraParameters.h"
#include "ExynosCameraFrameFactory.h"
#include "ExynosCameraFrameReprocessingFactory.h"
#include "ExynosCameraFrameFactoryFront.h"
#include "ExynosCameraFrameFactoryVision.h"
#include "ExynosCameraMemory.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraBufferLocker.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraScalableSensor.h"
#include "ExynosCameraFrameSelector.h"
#include "vendor/SecCameraConfig.h"
#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraParameters.h"
#endif

namespace android {

typedef struct ExynosCameraJpegCallbackBuffer {
    ExynosCameraBuffer buffer;
    int callbackNumber;
} jpeg_callback_buffer_t;

typedef ExynosCameraList<ExynosCameraFrame *> frame_queue_t;
typedef ExynosCameraList<jpeg_callback_buffer_t> jpeg_callback_queue_t;

typedef enum buffer_direction_type {
    SRC_BUFFER_DIRECTION        = 0,
    DST_BUFFER_DIRECTION        = 1,
    INVALID_BUFFER_DIRECTION,
} buffer_direction_type_t;

enum jpeg_save_thread {
    JPEG_SAVE_THREAD0       = 0,
    JPEG_SAVE_THREAD1       = 1,
    JPEG_SAVE_THREAD2,
};

class ExynosCamera {
public:
    ExynosCamera() {};
    ExynosCamera(int cameraId, camera_device_t *dev);
    virtual             ~ExynosCamera();

    status_t    setPreviewWindow(preview_stream_ops *w);
    void        setCallbacks(
                    camera_notify_callback notify_cb,
                    camera_data_callback data_cb,
                    camera_data_timestamp_callback data_cb_timestamp,
                    camera_request_memory get_memory,
                    void *user);

    void        enableMsgType(int32_t msgType);
    void        disableMsgType(int32_t msgType);
    bool        msgTypeEnabled(int32_t msgType);

    status_t    startPreview();
    void        stopPreview();
    bool        previewEnabled();

    status_t    storeMetaDataInBuffers(bool enable);

    status_t    startRecording();
    void        stopRecording();
    bool        recordingEnabled();
    void        releaseRecordingFrame(const void *opaque);

    status_t    autoFocus();
    status_t    cancelAutoFocus();

    status_t    takePicture();
    status_t    cancelPicture();

    status_t    setParameters(const CameraParameters& params);
    CameraParameters  getParameters() const;
    status_t    sendCommand(int32_t command, int32_t arg1, int32_t arg2);

    int         getMaxNumDetectedFaces(void);
    bool        startFaceDetection(void);
    bool        stopFaceDetection(void);
    bool        m_startFaceDetection(bool toggle);
    int         m_calibratePosition(int w, int new_w, int pos);
    status_t    m_doFdCallbackFunc(ExynosCameraFrame *frame);

    void        release();

    status_t    dump(int fd) const;
    void        dump(void);

    int         getCameraId() const;
    bool        isReprocessing(void) const;
    bool        isSccCapture(void) const;

    status_t    generateFrame(int32_t frameCount, ExynosCameraFrame **newFrame);

    status_t    generateFrameReprocessing(ExynosCameraFrame **newFrame);

    /* vision */
    status_t    generateFrameVision(int32_t frameCount, ExynosCameraFrame **newFrame);

private:
    void        m_createThreads(void);

    status_t    m_startPreviewInternal(void);
    status_t    m_stopPreviewInternal(void);

    status_t    m_restartPreviewInternal(void);

    status_t    m_startPictureInternal(void);
    status_t    m_stopPictureInternal(void);

    status_t    m_startRecordingInternal(void);
    status_t    m_stopRecordingInternal(void);

    status_t    m_searchFrameFromList(List<ExynosCameraFrame *> *list, uint32_t frameCount, ExynosCameraFrame **frame);
    status_t    m_removeFrameFromList(List<ExynosCameraFrame *> *list, ExynosCameraFrame *frame);

    status_t    m_clearList(List<ExynosCameraFrame *> *list);

    status_t    m_printFrameList(List<ExynosCameraFrame *> *list);

    status_t    m_createIonAllocator(ExynosCameraIonAllocator **allocator);
    status_t    m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name);
    status_t    m_createBufferManager(
                    ExynosCameraBufferManager **bufferManager,
                    const char *name,
                    buffer_manager_type type = BUFFER_MANAGER_ION_TYPE);

    status_t    m_setConfigInform();
    status_t    m_setBuffers(void);
    status_t    m_setReprocessingBuffer(void);
    status_t    m_setPreviewCallbackBuffer(void);
    status_t    m_setPictureBuffer(void);
    status_t    m_releaseBuffers(void);

    status_t    m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  reqBufCount,
                    bool createMetaPlane,
                    bool needMmap = false);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  minBufCount,
                    int  maxBufCount,
                    exynos_camera_buffer_type_t type,
                    bool createMetaPlane,
                    bool needMmap = false);
    status_t    m_allocBuffers(
                    ExynosCameraBufferManager *bufManager,
                    int  planeCount,
                    unsigned int *planeSize,
                    unsigned int *bytePerLine,
                    int  minBufCount,
                    int  maxBufCount,
                    exynos_camera_buffer_type_t type,
                    buffer_manager_allocation_mode_t allocMode,
                    bool createMetaPlane,
                    bool needMmap = false);

    status_t    m_handlePreviewFrame(ExynosCameraFrame *frame);
    status_t    m_handlePreviewFrameFront(ExynosCameraFrame *frame);

    status_t    m_setupEntity(
                    uint32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer *srcBuf = NULL,
                    ExynosCameraBuffer *dstBuf = NULL);
    status_t    m_setSrcBuffer(
                    uint32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer *buffer);
    status_t    m_setDstBuffer(
                    uint32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer *buffer);

    status_t    m_getBufferManager(uint32_t pipeId, ExynosCameraBufferManager **bufMgr, uint32_t direction);

    status_t    m_calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t    m_calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect);

    status_t    m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr);

    status_t    m_doPreviewToCallbackFunc(
                    int32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer previewBuf,
                    ExynosCameraBuffer callbackBuf);
    status_t    m_doCallbackToPreviewFunc(
                    int32_t pipeId,
                    ExynosCameraFrame *newFrame,
                    ExynosCameraBuffer callbackBuf,
                    ExynosCameraBuffer previewBuf);
    status_t    m_doPrviewToRecordingFunc(
                    int32_t pipeId,
                    ExynosCameraBuffer previewBuf,
                    ExynosCameraBuffer recordingBuf);
    status_t    m_releaseRecordingBuffer(int bufIndex);

    status_t    m_fastenAeStable(void);
    camera_memory_t *m_getJpegCallbackHeap(ExynosCameraBuffer callbackBuf, int seriesShotNumber);

    void        m_debugFpsCheck(uint32_t pipeId);

    uint32_t    m_getBayerPipeId(void);

    status_t    m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo);

    status_t    m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer);
    status_t    m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr);

    status_t    m_boostDynamicCapture(void);
    void        m_updateBoostDynamicCaptureSize(camera2_node_group *node_group_info);
    void        m_checkFpsAndUpdatePipeWaitTime(void);

public:
    ExynosCameraParameters          *m_exynosCameraParameters;
    ExynosCameraFrameFactory        *m_previewFrameFactory;
    ExynosCameraGrallocAllocator    *m_grAllocator;
    ExynosCameraIonAllocator        *m_ionAllocator;
    ExynosCameraMHBAllocator        *m_mhbAllocator;

    ExynosCameraFrameFactory        *m_pictureFrameFactory;

    ExynosCameraFrameFactory        *m_reprocessingFrameFactory;
    mutable Mutex                   m_frameLock;

    preview_stream_ops              *m_previewWindow;
    bool                            m_previewEnabled;
    bool                            m_pictureEnabled;
    bool                            m_recordingEnabled;
    bool                            m_zslPictureEnabled;

    ExynosCameraActivityControl     *m_exynosCameraActivityControl;

    /* vision */
    ExynosCameraFrameFactory        *m_visionFrameFactory;

private:
    typedef ExynosCameraThread<ExynosCamera> mainCameraThread;

    uint32_t                        m_cameraId;

    camera_notify_callback          m_notifyCb;
    camera_data_callback            m_dataCb;
    camera_data_timestamp_callback  m_dataCbTimestamp;
    camera_request_memory           m_getMemoryCb;
    void                            *m_callbackCookie;

    List<ExynosCameraFrame *>       m_processList;
    List<ExynosCameraFrame *>       m_postProcessList;
    frame_queue_t                   *m_pipeFrameDoneQ;
    /* vision */
    frame_queue_t                   *m_pipeFrameVisionDoneQ;

    sp<mainCameraThread>            m_mainThread;
    bool                            m_mainThreadFunc(void);

    frame_queue_t                   *m_previewQ;
    frame_queue_t                   *m_previewReturnQ;
    sp<mainCameraThread>            m_previewThread;
    bool                            m_previewThreadFunc(void);

    sp<mainCameraThread>            m_setBuffersThread;
    bool                            m_setBuffersThreadFunc(void);

    sp<mainCameraThread>            m_startPictureInternalThread;
    bool                            m_startPictureInternalThreadFunc(void);

    sp<mainCameraThread>            m_autoFocusThread;
    bool                            m_autoFocusThreadFunc(void);
    bool                            m_autoFocusResetNotify(int focusMode);
    mutable Mutex                   m_autoFocusLock;
    bool                            m_exitAutoFocusThread;
    bool                            m_autoFocusRunning;
    int                             m_autoFocusType;

    ExynosCameraBufferManager       *m_bayerBufferMgr;
    ExynosCameraBufferManager       *m_3aaBufferMgr;
    ExynosCameraBufferManager       *m_ispBufferMgr;
    ExynosCameraBufferManager       *m_sccBufferMgr;
    ExynosCameraBufferManager       *m_scpBufferMgr;

    uint32_t                        m_fliteFrameCount;
    uint32_t                        m_3aa_ispFrameCount;
    uint32_t                        m_sccFrameCount;
    uint32_t                        m_scpFrameCount;

    /* for Recording */
    frame_queue_t                   *m_recordingQ;
    mutable Mutex                   m_recordingStateLock;
    nsecs_t                         m_lastRecordingTimeStamp;
    nsecs_t                         m_recordingStartTimeStamp;

    ExynosCameraBufferManager       *m_recordingBufferMgr;
    camera_memory_t                 *m_recordingCallbackHeap;

    nsecs_t                         m_recordingTimeStamp[NUM_RECORDING_BUFFERS];
    sp<mainCameraThread>            m_recordingThread;
    bool                            m_recordingThreadFunc(void);

    ExynosCameraBufferManager       *m_previewCallbackBufferMgr;

    /* Pre picture Thread */
    sp<mainCameraThread>            m_prePictureThread;
    bool                            m_reprocessingPrePictureInternal(void);
    bool                            m_prePictureInternal(bool* pIsProcessed);
    bool                            m_prePictureThreadFunc(void);

    sp<mainCameraThread>            m_pictureThread;
    bool                            m_pictureThreadFunc(void);

    sp<mainCameraThread>            m_postPictureThread;
    bool                            m_postPictureThreadFunc(void);

    sp<mainCameraThread>            m_jpegCallbackThread;
    bool                            m_jpegCallbackThreadFunc(void);
    void                            m_clearJpegCallbackThread(void);

    bool                            m_releasebuffersForRealloc(void);

    /* Reprocessing Buffer Managers */
    ExynosCameraBufferManager       *m_ispReprocessingBufferMgr;

    ExynosCameraBufferManager       *m_sccReprocessingBufferMgr;

    /* TODO: will be removed when SCC scaling for picture size */
    ExynosCameraBufferManager       *m_gscBufferMgr;

    ExynosCameraBufferManager       *m_jpegBufferMgr;

    ExynosCameraCounter             m_takePictureCounter;
    ExynosCameraCounter             m_reprocessingCounter;
    ExynosCameraCounter             m_pictureCounter;
    ExynosCameraCounter             m_jpegCounter;

    /* Reprocessing Q */
    frame_queue_t                   *dstIspReprocessingQ;
    frame_queue_t                   *dstSccReprocessingQ;
    frame_queue_t                   *dstGscReprocessingQ;
    frame_queue_t                   *dstJpegReprocessingQ;

    frame_queue_t                   *m_postPictureQ;
    jpeg_callback_queue_t           *m_jpegCallbackQ;

    bool                            m_flagStartFaceDetection;
    camera_face_t                   m_faces[NUM_OF_DETECTED_FACES];
    camera_frame_metadata_t         m_frameMetadata;
    camera_memory_t                 *m_fdCallbackHeap;

    bool                            m_faceDetected;
    int                             m_fdThreshold;

    ExynosCameraScalableSensor      m_scalableSensorMgr;

    /* Watch Dog Thread */
    sp<mainCameraThread>            m_monitorThread;
    bool                            m_monitorThreadFunc(void);
    status_t                        m_checkThreadState(int *threadState, int *countRenew);
    status_t                        m_checkThreadInterval(uint32_t pipeId, uint32_t pipeInterval, int *threadState);
    unsigned int                    m_callbackState;
    unsigned int                    m_callbackStateOld;
    int                             m_callbackMonitorCount;
    bool                            m_isNeedAllocPictureBuffer;

#ifdef FPS_CHECK
    /* TODO: */
#define DEBUG_MAX_PIPE_NUM 10
    int32_t                         m_debugFpsCount[DEBUG_MAX_PIPE_NUM];
    ExynosCameraDurationTimer       m_debugFpsTimer[DEBUG_MAX_PIPE_NUM];
#endif

    ExynosCameraFrameSelector       *m_captureSelector;
    ExynosCameraFrameSelector       *m_sccCaptureSelector;

    sp<mainCameraThread>            m_jpegSaveThread0;
    sp<mainCameraThread>            m_jpegSaveThread1;
    sp<mainCameraThread>            m_jpegSaveThread2;
    bool                            m_jpegSaveThreadFunc(void);

    jpeg_callback_queue_t          *m_jpegSaveQ0;
    jpeg_callback_queue_t          *m_jpegSaveQ1;
    jpeg_callback_queue_t          *m_jpegSaveQ2;

#if (BURST_CAPTURE)
    bool                            m_isCancelBurstCapture;
    int                             m_burstCaptureCallbackCount;
    mutable Mutex                   m_burstCaptureCallbackCountLock;
    ExynosCameraDurationTimer                   m_burstSaveTimer;
    long long                       m_burstSaveTimerTime;
    int                             m_burstDuration;
    char                            m_burstFilePath[50];
#endif

    bool                            m_stopBurstShot;
    bool                            m_burst[3];
    bool                            m_running[3];

    /* high resolution preview callback */
    sp<mainCameraThread>            m_highResolutionCallbackThread;
    bool                            m_highResolutionCallbackThreadFunc(void);

    bool                            m_highResolutionCallbackRunning;
    frame_queue_t                   *m_highResolutionCallbackQ;
    bool                            m_resetPreview;

    uint32_t                        m_displayPreviewToggle;

    bool                            m_hdrEnabled;
    unsigned int                    m_hdrSkipedFcount;
    bool                            m_isFirstStart;
    uint32_t                        m_dynamicSccCount;
    bool                            m_isTryStopFlash;
    uint32_t                        m_curMinFps;

    /* vision */
    status_t                        m_startVisionInternal(void);
    status_t                        m_stopVisionInternal(void);

    status_t                        m_setVisionBuffers(void);
    status_t                        m_setVisionCallbackBuffer(void);

    sp<mainCameraThread>            m_visionThread;
    bool                            m_visionThreadFunc(void);
    int                             m_visionFps;
    int                             m_visionAe;
    struct ExynosConfigInfo         *m_exynosconfig;
    uint32_t                        m_recordingFrameSkipCount;
};

}; /* namespace android */
#endif
