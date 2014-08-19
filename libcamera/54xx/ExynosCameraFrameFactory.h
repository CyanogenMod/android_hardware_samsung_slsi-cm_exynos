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

#ifndef EXYNOS_CAMERA_FRAME_FACTORY_H
#define EXYNOS_CAMERA_FRAME_FACTORY_H

#include "ExynosCameraFrame.h"

#include "ExynosCameraPipe.h"
#include "ExynosCameraPipeFlite.h"
#include "ExynosCameraPipe3AA.h"
#include "ExynosCameraPipe3AC.h"
#include "ExynosCameraPipeISP.h"
#include "ExynosCameraPipe3AA_ISP.h"
#include "ExynosCameraPipeSCC.h"
#include "ExynosCameraPipeSCP.h"
#include "ExynosCameraPipeGSC.h"
#include "ExynosCameraPipeJpeg.h"

#define MAX_NUM_PIPES       (MAX_PIPE_NUM)
#define INDEX(x)            (x % MAX_NUM_PIPES)

namespace android {

class ExynosCameraFrameFactory {
protected:
    ExynosCameraFrameFactory();
    ExynosCameraFrameFactory(int cameraId, ExynosCameraParameters *param);
public:
    static ExynosCameraFrameFactory *createFrameFactory(int cameraId, ExynosCameraParameters *param);
    virtual ~ExynosCameraFrameFactory();

    virtual status_t        create(void);
    virtual status_t        destroy(void);

    virtual status_t        fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers);

    virtual ExynosCameraFrame *createNewFrameOnlyOnePipe(int pipeId);
    virtual ExynosCameraFrame *createNewFrameVideoOnly(void);
    virtual ExynosCameraFrame *createNewFrame(void);

    virtual status_t        initPipes(void);
    virtual status_t        preparePipes(void);

    virtual status_t        pushFrameToPipe(ExynosCameraFrame **newFrame, uint32_t pipeId);
    virtual status_t        setOutputFrameQToPipe(frame_queue_t *outputQ, uint32_t pipeId);
    virtual status_t        getOutputFrameQToPipe(frame_queue_t **outputQ, uint32_t pipeId);
    virtual status_t        getInputFrameQToPipe(frame_queue_t **inputFrameQ, uint32_t pipeId);
    virtual status_t        startPipes(void);
    virtual status_t        startInitialThreads(void);
    virtual status_t        startThread(uint32_t pipeId);
    virtual status_t        stopThread(uint32_t pipeId);
    virtual status_t        setStopFlag(void);
    virtual status_t        stopPipes(void);

    virtual status_t        getThreadState(int **threadState, uint32_t pipeId);
    virtual status_t        getThreadInterval(uint64_t **threadInterval, uint32_t pipeId);
    virtual status_t        getThreadRenew(int **threadRenew, uint32_t pipeId);
    virtual status_t        incThreadRenew(uint32_t pipeId);
    virtual void            dump(void);

    virtual void            setRequestFLITE(bool enable);
    virtual void            setRequest3AC(bool enable);
    virtual void            setRequestSCC(bool enable);


    virtual status_t        setParam(struct v4l2_streamparm *streamParam, uint32_t pipeId);
    virtual status_t        setControl(int cid, int value, uint32_t pipeId);

protected:
    virtual status_t        m_initPipelines(ExynosCameraFrame *frame);
    virtual status_t        m_initFrameMetadata(ExynosCameraFrame *frame);
    virtual status_t        m_fillNodeGroupInfo(ExynosCameraFrame *frame);
    virtual status_t        m_checkPipeInfo(uint32_t srcPipeId, uint32_t dstPipeId);

protected:
    int                         m_cameraId;
    ExynosCameraPipe           *m_pipes[MAX_NUM_PIPES];
    ExynosCameraParameters     *m_parameters;
    uint32_t                    m_frameCount;

    ExynosCameraActivityControl *m_activityControl;

    uint32_t                    m_requestFLITE;
    uint32_t                    m_request3AP;
    uint32_t                    m_request3AC;
    uint32_t                    m_requestISP;
    uint32_t                    m_requestSCC;
    uint32_t                    m_requestDIS;
    uint32_t                    m_requestSCP;

    bool                        m_bypassDRC;
    bool                        m_bypassDIS;
    bool                        m_bypassDNR;
    bool                        m_bypassFD;
};

}; /* namespace android */

#endif
