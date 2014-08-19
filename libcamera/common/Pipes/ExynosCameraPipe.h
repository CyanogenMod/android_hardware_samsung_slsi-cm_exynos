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

#ifndef EXYNOS_CAMERA_PIPE_H
#define EXYNOS_CAMERA_PIPE_H

#include "ExynosCameraThread.h"
#include "ExynosCameraThreadFactory.h"

#include "ExynosCameraNode.h"
#include "ExynosCameraFrame.h"
#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraConfig.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraList.h"

namespace android {

typedef ExynosCameraList<ExynosCameraFrame *> frame_queue_t;

enum PIPE_POSITION {
    SRC_PIPE            = 0,
    DST_PIPE
};

enum NODE_TYPE {
    OUTPUT_NODE         = 0,    /* Node for capture device */
    CAPTURE_NODE,               /* Node for output device */
    SUB_NODE,                   /* Node for extra device */
    MAX_NODE
};

typedef enum perframe_node_type {
    PERFRAME_NODE_TYPE_NONE        = 0,
    PERFRAME_NODE_TYPE_LEADER      = 1,
    PERFRAME_NODE_TYPE_CAPTURE     = 2,
} perframe_node_type_t;

typedef struct ExynosCameraPerframeNodeInfo {
    perframe_node_type_t perFrameNodeType;
    int perframeInfoIndex;
    int perFrameVideoID;
} camera_pipe_perframe_node_info_t;

typedef struct ExynosCameraPerframeNodeGroupInfo {
    int perframeSupportNodeNum;
    camera_pipe_perframe_node_info_t perFrameLeaderInfo;
    camera_pipe_perframe_node_info_t perFrameCaptureInfo[CAPTURE_NODE_MAX];
} camera_pipe_perframe_node_group_info_t;

typedef struct ExynosCameraPipeInfo {
    struct ExynosRect rectInfo;
    struct v4l2_requestbuffers bufInfo;
    camera_pipe_perframe_node_group_info_t perFrameNodeGroupInfo;
    unsigned int bytesPerPlane[EXYNOS_CAMERA_BUFFER_MAX_PLANES];
} camera_pipe_info_t;

namespace BUFFER_POS {
    enum POS {
        SRC = 0x00,
        DST = 0x01
    };
}

namespace BUFFERQ_TYPE {
    enum TYPE {
        INPUT = 0x00,
        OUTPUT = 0x01
    };
};

class ExynosCameraPipe {
public:
    ExynosCameraPipe();
    virtual ~ExynosCameraPipe();

    virtual status_t        create(int32_t *sensorIds);
    virtual status_t        destroy(void);

    virtual status_t        setupPipe(camera_pipe_info_t *pipeInfos);
    virtual status_t        prepare(void);

    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        startThread(void);
    virtual status_t        stopThread(void);

    virtual status_t        sensorStream(bool on);
    virtual status_t        setControl(int cid, int value);
    virtual status_t        getControl(int cid, int *value);
    virtual status_t        setParam(struct v4l2_streamparm streamParam);

    virtual status_t        instantOn(int32_t numFrames);
    virtual status_t        instantOnQbuf(ExynosCameraFrame **frame, BUFFER_POS::POS pos);
    virtual status_t        instantOnDQbuf(ExynosCameraFrame **frame, BUFFER_POS::POS pos);
    virtual status_t        instantOff(void);
    virtual status_t        instantOnPushFrameQ(BUFFERQ_TYPE::TYPE type, ExynosCameraFrame **frame);

    virtual status_t        pushFrame(ExynosCameraFrame **newFrame);

    virtual int             getCameraId(void);
    virtual uint32_t        getPipeId(void);
    virtual status_t        setPipeId(uint32_t id);
    virtual status_t        setPipeName(const char *pipeName);

    virtual status_t        getInputFrameQ(frame_queue_t **inputQ);
    virtual status_t        getOutputFrameQ(frame_queue_t **outputQ);
    virtual status_t        setOutputFrameQ(frame_queue_t *outputQ);

    virtual status_t        getPipeInfo(int *fullW, int *fullH, int *colorFormat, int pipePosition);

    virtual status_t        getThreadState(int **threadState);
    virtual status_t        getThreadInterval(uint64_t **timeInterval);
    virtual status_t        getThreadRenew(int **timeRenew);
    virtual status_t        incThreadRenew();
    virtual status_t        setStopFlag(void);
    virtual void            dump(void);

protected:
    virtual bool            m_mainThreadFunc(void);

    virtual status_t        m_putBuffer(void);
    virtual status_t        m_getBuffer(void);

    virtual status_t        m_completeFrame(
                                ExynosCameraFrame **frame,
                                ExynosCameraBuffer buffer,
                                bool isValid = true);

    virtual status_t        m_getFrameByIndex(ExynosCameraFrame **frame, int index);
    virtual status_t        m_updateMetadataToFrame(void *metadata, int index);

    virtual bool            m_isReprocessing(void);
    virtual bool            m_checkThreadLoop(void);

    virtual void            m_dumpRunningFrameList(void);

    virtual void            m_dumpPerframeNodeGroupInfo(const char *name, camera_pipe_perframe_node_group_info_t nodeInfo);

    void                    m_configDvfs(void);

protected:
    ExynosCameraNode           *m_mainNode;
    int32_t                     m_mainNodeNum;

    sp<Thread>                  m_mainThread;
    struct ExynosConfigInfo     *m_exynosconfig;

protected:
    uint32_t                    m_pipeId;
    int32_t                     m_cameraId;
    uint32_t                    m_reprocessing;

    ExynosCameraParameters     *m_parameters;
    uint32_t                    m_prepareBufferCount;
    uint32_t                    m_numBuffers;
    
    ExynosCameraActivityControl *m_activityControl;

    frame_queue_t              *m_inputFrameQ;
    frame_queue_t              *m_outputFrameQ;

    ExynosCameraFrame          *m_runningFrameList[MAX_BUFFERS];
    uint32_t                    m_numOfRunningFrame;

    bool                        m_flagStartPipe;
    bool                        m_flagTryStop;

    ExynosCameraDurationTimer   m_timer;
    int                         m_threadCommand;
    uint64_t                    m_timeInterval;
    int                         m_threadState;
    int                         m_threadRenew;
    char                        m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    bool                        m_metadataTypeShot;
    camera_pipe_perframe_node_group_info_t m_perframeMainNodeGroupInfo;


    bool                        m_dvfsLocked;
};

}; /* namespace android */

#endif
