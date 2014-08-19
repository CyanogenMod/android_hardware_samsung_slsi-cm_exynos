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

#ifndef EXYNOS_CAMERA_PIPE_3AA_ISP_H
#define EXYNOS_CAMERA_PIPE_3AA_ISP_H

#include "ExynosCameraPipe.h"

namespace android {

typedef ExynosCameraList<ExynosCameraBuffer> isp_buffer_queue_t;

class ExynosCameraPipe3AA_ISP : protected virtual ExynosCameraPipe {
public:
    ExynosCameraPipe3AA_ISP(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums);

    virtual ~ExynosCameraPipe3AA_ISP();

    virtual status_t        create(int32_t *sensorIds);
    virtual status_t        destroy(void);

    virtual status_t        setupPipe(camera_pipe_info_t *pipeInfos);
    virtual status_t        prepare(void);

    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        startThread(void);
    virtual status_t        stopThread(void);

    virtual status_t        setControl(int cid, int value);
    virtual status_t        instantOn(int32_t numFrames);
    virtual status_t        instantOff(void);

    virtual void            dump(void);

protected:
    status_t                m_getBuffer(void);
    status_t                m_putBuffer(void);
    status_t                m_getIspBuffer(void);

    virtual status_t        m_checkShotDone(struct camera2_shot_ext *shot_ext);

protected:
    virtual bool            m_mainThreadFunc(void);
    virtual bool            m_ispThreadFunc(void);

    /* Node for 3AA output Interface : m_mainNode is defined in Super class */
    /* Node for 3AA capture Interface */
    ExynosCameraNode           *m_subNode;

    /* Node for ISP output Interface */
    ExynosCameraNode           *m_ispNode;
    int32_t                     m_ispNodeNum;
    camera_pipe_perframe_node_group_info_t m_perframeSubNodeGroupInfo;
    camera_pipe_perframe_node_group_info_t m_perframeIspNodeGroupInfo;
private:
    sp<Thread>                  m_ispThread;
    isp_buffer_queue_t          *m_ispBufferQ;
    int                         m_setfile;
};

}; /* namespace android */

#endif
