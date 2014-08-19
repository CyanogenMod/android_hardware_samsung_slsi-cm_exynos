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

#ifndef EXYNOS_CAMERA_FRAME_H
#define EXYNOS_CAMERA_FRAME_H

#include <utils/List.h>

#include "ExynosCameraParameters.h"
#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraBuffer.h"

namespace android {

typedef enum entity_type {
    ENTITY_TYPE_INPUT_ONLY              = 0, /* Need input buffer only */
    ENTITY_TYPE_OUTPUT_ONLY             = 1, /* Need output buffer only */
    ENTITY_TYPE_INPUT_OUTPUT            = 2, /* Need input/output both */
    ENTITY_TYPE_INVALID,
} entity_type_t;

/* Entity state define */
typedef enum entity_state {
    ENTITY_STATE_NOP                    = 0, /* Do not need operation */
    ENTITY_STATE_READY                  = 1, /* Ready to operation */
    ENTITY_STATE_PROCESSING             = 2, /* Processing stage */
    ENTITY_STATE_FRAME_DONE             = 3, /* Pipe has been done HW operation. */
    ENTITY_STATE_COMPLETE               = 4, /* Complete frame, done for all SW work. */
    ENTITY_STATE_REWORK                 = 5, /* After COMPLETE, but need re-work entity */
} entity_state_t;

typedef enum entity_buffer_type {
    ENTITY_BUFFER_FIXED                 = 0, /* Buffer is never change */
    ENTITY_BUFFER_DELIVERY              = 1, /* Buffer change is possible */
    ENTITY_BUFFER_INVALID
} entity_buffer_type_t;

typedef enum entity_buffer_state {
    ENTITY_BUFFER_STATE_NOREQ           = 0, /* This buffer is not used */
    ENTITY_BUFFER_STATE_REQUESTED       = 1, /* This buffer is not used */
    ENTITY_BUFFER_STATE_READY           = 2, /* Buffer is ready */
    ENTITY_BUFFER_STATE_PROCESSING      = 3, /* Buffer is being prossed */
    ENTITY_BUFFER_STATE_COMPLETE        = 4, /* Buffer is complete */
    ENTITY_BUFFER_STATE_INVALID,
} entity_buffer_state_t;

class ExynosCameraFrameEntity {
public:
    ExynosCameraFrameEntity(
        uint32_t pipeId,
        entity_type_t type,
        entity_buffer_type_t bufType);
    uint32_t getPipeId(void);

    status_t setSrcBuf(ExynosCameraBuffer buf);
    status_t setDstBuf(ExynosCameraBuffer buf);

    status_t getSrcBuf(ExynosCameraBuffer *buf);
    status_t getDstBuf(ExynosCameraBuffer *buf);

    status_t setSrcRect(ExynosRect rect);
    status_t setDstRect(ExynosRect rect);

    status_t getSrcRect(ExynosRect *rect);
    status_t getDstRect(ExynosRect *rect);

    status_t setSrcBufState(entity_buffer_state_t state);
    status_t setDstBufState(entity_buffer_state_t state);

    entity_buffer_state_t getSrcBufState(void);
    entity_buffer_state_t getDstBufState(void);

    entity_buffer_type_t getBufType(void);

    status_t        setEntityState(entity_state_t state);
    entity_state_t  getEntityState(void);

    ExynosCameraFrameEntity *getPrevEntity(void);
    ExynosCameraFrameEntity *getNextEntity(void);

    status_t setPrevEntity(ExynosCameraFrameEntity *entity);
    status_t setNextEntity(ExynosCameraFrameEntity *entity);

private:
    status_t m_setEntityType(entity_type_t type);

private:
    uint32_t                m_pipeId;
    ExynosCameraBuffer      m_srcBuf;
    ExynosCameraBuffer      m_dstBuf;

    ExynosRect              m_srcRect;
    ExynosRect              m_dstRect;

    entity_type_t           m_EntityType;
    entity_buffer_type_t    m_bufferType;

    entity_buffer_state_t   m_srcBufState;
    entity_buffer_state_t   m_dstBufState;
    entity_state_t          m_entityState;

    ExynosCameraFrameEntity *m_prevEntity;
    ExynosCameraFrameEntity *m_nextEntity;
};

/* Frame state define */
typedef enum frame_status {
    FRAME_STATE_READY      = 0,    /* Ready to operation */
    FRAME_STATE_RUNNING    = 1,    /* Frame is running */
    FRAME_STATE_COMPLETE   = 2,    /* Complete frame. */
    FRAME_STATE_SKIPPED    = 3,    /* This Frame has been skipped. */
    FRAME_STATE_INVALID    = 4,    /* Invalid state */
} frame_status_t;

typedef struct ExynosCameraPerFrameInfo {
    bool perFrameControlNode;
    int perFrameNodeIndex;
    int perFrameNodeVideID;
} camera_per_fream_into_t;

class ExynosCameraFrame {
public:
    ExynosCameraFrame(
            ExynosCameraParameters *obj_param,
            uint32_t frameCount);
    ~ExynosCameraFrame();

    /* If curEntity is NULL, newEntity is added to m_linkageList */
    status_t        addSiblingEntity(
                        ExynosCameraFrameEntity *curEntity,
                        ExynosCameraFrameEntity *newEntity);
    status_t        addChildEntity(
                        ExynosCameraFrameEntity *parentEntity,
                        ExynosCameraFrameEntity *newEntity);

    ExynosCameraFrameEntity *getFirstEntity(void);
    ExynosCameraFrameEntity *getNextEntity(void);
    ExynosCameraFrameEntity *getChildEntity(ExynosCameraFrameEntity *parentEntity);

    ExynosCameraFrameEntity *searchEntityByPipeId(uint32_t pipeId);

    status_t        setSrcBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer srcBuf);
    status_t        setDstBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer dstBuf);

    status_t        getSrcBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer *srcBuf);
    status_t        getDstBuffer(
                        uint32_t pipeId,
                        ExynosCameraBuffer *dstBuf);

    status_t        setSrcRect(
                        uint32_t pipeId,
                        ExynosRect srcRect);
    status_t        setDstRect(
                        uint32_t pipeId,
                        ExynosRect dstRect);

    status_t        getSrcRect(
                        uint32_t pipeId,
                        ExynosRect *srcRect);
    status_t        getDstRect(
                        uint32_t pipeId,
                        ExynosRect *dstRect);

    status_t        getSrcBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t *state);
    status_t        getDstBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t *state);

    status_t        setSrcBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state);
    status_t        setDstBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state);

    status_t        ensureSrcBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state);

    status_t        ensureDstBufferState(
                        uint32_t pipeId,
                        entity_buffer_state_t state);

    status_t        setEntityState(
                        uint32_t pipeId,
                        entity_state_t state);
    status_t        getEntityState(
                        uint32_t pipeId,
                        entity_state_t *state);

    status_t        getEntityBufferType(
                        uint32_t pipeId,
                        entity_buffer_type_t *type);

    void            setRequest(bool tap,
                               bool tac,
                               bool isp,
                               bool scc,
                               bool dis,
                               bool scp);

    void            setRequest(uint32_t pipeId, bool val);
    bool            getRequest(uint32_t pipeId);

    uint32_t        getFrameCount(void);
    status_t        setNumRequestPipe(uint32_t num);
    uint32_t        getNumRequestPipe(void);

    bool            isComplete(void);
    ExynosCameraFrameEntity *getFrameDoneEntity(void);

    status_t        skipFrame(void);

    void            setFrameState(frame_status_t state);
    frame_status_t  getFrameState(void);
    bool            checkFrameState(frame_status_t state);

    void            printEntity(void);
    void            dump(void);

    void            frameLock(void);
    void            frameUnlock(void);
    bool            getFrameLockState(void);

    status_t        initMetaData(struct camera2_shot_ext *shot);
    status_t        getMetaData(struct camera2_shot_ext *shot);

    status_t        storeDynamicMeta(struct camera2_shot_ext *shot);
    status_t        storeUserDynamicMeta(struct camera2_shot_ext *shot);

    status_t        getDynamicMeta(struct camera2_shot_ext *shot);
    status_t        getUserDynamicMeta(struct camera2_shot_ext *shot);

    status_t        setMetaDataEnable(bool flag);
    bool            getMetaDataEnable();

    status_t        getNodeGroupInfo(struct camera2_node_group *node_group, int index);
    status_t        storeNodeGroupInfo(struct camera2_node_group *node_group, int index);
    status_t        getNodeGroupInfo(struct camera2_node_group *node_group, int index, int *zoom);
    status_t        storeNodeGroupInfo(struct camera2_node_group *node_group, int index, int zoom);
    void            dumpNodeGroupInfo(const char *name);

    void            setJpegSize(int size);
    int             getJpegSize(void);

    int64_t         getTimeStamp(void);
    void            getFpsRange(uint32_t *min, uint32_t *max);

private:
    List<ExynosCameraFrameEntity *>      m_linkageList;
    List<ExynosCameraFrameEntity *>::iterator m_currentEntity;

    ExynosCameraParameters     *m_parameters;
    uint32_t                    m_frameCount;

    frame_status_t              m_frameState;
    mutable Mutex               m_frameStateLock;

    uint32_t                    m_numRequestPipe;
    uint32_t                    m_numCompletePipe;

    bool                        m_frameLocked;

    bool                        m_metaDataEnable;
    struct camera2_shot_ext     m_metaData;
    struct camera2_node_group   m_node_gorup[PERFRAME_NODE_GROUP_MAX];
    int                         m_zoom;

    int                         m_jpegSize;

    bool                        m_request3AP;
    bool                        m_request3AC;
    bool                        m_requestISP;
    bool                        m_requestSCC;
    bool                        m_requestDIS;
    bool                        m_requestSCP;
};

}; /* namespace android */

#endif
