/*
 * Copyright 2013, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ExynosCameraNode.h
 * \brief     hearder file for ExynosCameraNode
 * \author    Pilsun Jang(pilsun.jang@samsung.com)
 * \date      2013/6/27
 *
 */

#ifndef EXYNOS_CAMERA_NODE_H__
#define EXYNOS_CAMERA_NODE_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utils/threads.h>

#include <videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/vt.h>

#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/List.h>
#include <utils/Log.h>
#include "cutils/properties.h"

#include "ion.h"
#include "exynos_format.h"
#include "ExynosCameraBuffer.h"
#include "ExynosRect.h"
/* #include "ExynosFrame.h" */

#include "exynos_v4l2.h"
#include "fimc-is-metadata.h"

/* #include "ExynosCameraState.h" */
#include "ExynosCameraConfig.h"

using namespace android;

/* #define EXYNOS_CAMERA_NODE_TRACE */

#ifdef EXYNOS_CAMERA_NODE_TRACE
#define EXYNOS_CAMERA_NODE_IN()   CLOGD("DEBUG(%s[%d]):IN...m_nodeState[%d]", __FUNCTION__, __LINE__, m_nodeState)
#define EXYNOS_CAMERA_NODE_OUT()  CLOGD("DEBUG(%s[%d]):OUT..m_nodeState[%d]", __FUNCTION__, __LINE__, m_nodeState)
#else
#define EXYNOS_CAMERA_NODE_IN()   ((void *)0)
#define EXYNOS_CAMERA_NODE_OUT()  ((void *)0)
#endif

/* ExynosCameraNode
 *
 * ingroup Exynos
 */

/*
 * Mapping table
 *
 * |------------------------------------------------------------------
 * | ExynosCamera        | ExynosCameraNode             | Driver IOCTL
 * |------------------------------------------------------------------
 * | setSize()           | setSize() - NONE             | S_FMT
 * | setColorFormat()    | setColorFormat() - NONE      | S_FMT
 * | setBufferType()     | setBufferTyoe() - NONE       | S_FMT
 * | prepare()           | prepare() - m_setInput()     | S_INPUT
 * |                     | prepare() - m_setFmt()       | S_FMT
 * | reqBuffer()         | reqBuffer() - m_reqBuf()     | REQ_BUF
 * | queryBuffer()       | queryBuffer() - m_queryBuf() | QUERY_BUF
 * | setBuffer()         | setBuffer() - m_qBuf()       | Q_BUF
 * | getBuffer()         | getBuffer() - m_qBuf()       | Q_BUF
 * | putBuffer()         | putBuffer() - m_dqBuf()      | DQ_BUF
 * | start()             | start() - m_streamOn()       | STREAM_ON
 * | polling()           | polling() - m_poll()         | POLL
 * |------------------------------------------------------------------
 * | setBufferRef()      |                              |
 * | getSize()           |                              |
 * | getColorFormat()    |                              |
 * | getBufferType()     |                              |
 * |------------------------------------------------------------------
 *
 */

enum node_request_state {
    NODE_REQUEST_STATE_BASE = 0,
    NODE_REQUEST_STATE_READY,
    NODE_REQUEST_STATE_QBUF_BLOCK,
    NODE_REQUEST_STATE_QBUF_DONE,
    NODE_REQUEST_STATE_DQBUF_BLOCK,
    NODE_REQUEST_STATE_DQBUF_DONE,
    NODE_REQUEST_STATE_STOPPED,
    NODE_REQUEST_STATE_INVALID
};

class ExynosCameraNodeRequest {
public:
    /* Constructor */
    ExynosCameraNodeRequest();
    /* Destructor */
    virtual ~ExynosCameraNodeRequest();

    void setState(enum node_request_state state);
    enum node_request_state getState(void);

private:
    unsigned int m_requestCount;
    enum node_request_state m_requestState;
};

#define NODE_PREPARE_SIZE (1)
#define NODE_PREPARE_FORMAT (2)
#define NODE_PREPARE_BUFFER_TYPE (3)
#define NODE_PREPARE_BUFFER_REF (4)
#define NODE_PREPARE_COMPLETE (NODE_PREPARE_SIZE | \
        NODE_PREPARE_FORMAT | \
        NODE_PREPARE_BUFFER_TYPE | \
        NODE_PREPARE_BUFFER_REF)

#define NODE_INIT_NEGATIVE_VALUE -1
#define NODE_INIT_ZERO_VALUE 0

class ExynosCameraNode {
public:
    enum EXYNOS_CAMERA_NODE_DUMMY {
        NODE_NONE,
    };

    enum EXYNOS_CAMERA_NODE_STATE {
        NODE_STATE_BASE = 0,
        NODE_STATE_NONE,
        NODE_STATE_CREATED,
        NODE_STATE_OPENED,
        NODE_STATE_IN_PREPARE,
        NODE_STATE_RUNNING,
        NODE_STATE_DESTROYED,
        NODE_STATE_MAX
    };

public:
    /* Constructor */
    ExynosCameraNode();
    /* Destructor */
    virtual ~ExynosCameraNode();

    /* Create the instance */
    status_t create();
    /* Create the instance */
    status_t create(const char *nodeName);
    /* Create the instance */
    status_t create(const char *nodeName, const char *nodeAlias);
    /* Create the instance */
    status_t create(const char *nodeName, int fd);
    /* Destroy the instance */
    status_t destroy(void);

    /* open Node */
    status_t open(int videoNodeNum);
    /* close Node */
    status_t close(void);
    /* get file descriptor */
    status_t getFd(int *fd);

    /* set v4l2 color format */
    status_t setColorFormat(int v4l2Colorformat, int planesCount);
    /* get v4l2 color format */
    status_t getColorFormat(int *v4l2Colorformat, int *planesCount);

    /* set size */
    status_t setSize(int w, int h);
    /* get size */
    status_t getSize(int *w, int *h);

    /* set id */
    status_t setId(int id);

    /* set memory info */
    status_t setBufferType(
                int bufferCount,
                enum v4l2_buf_type type,
                enum v4l2_memory bufferMemoryType);
    /* get memory info */
    status_t getBufferType(
                int *bufferCount,
                enum v4l2_buf_type *type,
                enum v4l2_memory *bufferMemoryType);

    /* query buffer */
    status_t queryBuf(void);
    /* request buffers */
    status_t reqBuffers(void);
    /* clear buffers */
    status_t clrBuffers(void);
    /* set id */
    status_t setControl(unsigned int id, int value);
    status_t getControl(unsigned int id, int *value);

    /* polling */
    status_t polling(void);

    /* setInput */
    status_t setInput(int sensorId);

    /* setCrop */
    status_t setCrop(enum v4l2_buf_type type, int x, int y, int w, int h);

    /* setFormat */
    status_t setFormat(void);
    status_t setFormat(unsigned int bytesPerPlane[]);

    /* startNode */
    status_t start(void);
    /* stopNode */
    status_t stop(void);

    /* Check if the instance was created */
    bool isCreated(void);
    /* Check if it start */
    bool isStarted(void);

    /* putBuffer */
    status_t putBuffer(ExynosCameraBuffer *buf);

    /* getBuffer */
    status_t getBuffer(ExynosCameraBuffer *buf, int *dqIndex);

    /* dump the object info */
    void dump(void);
    /* dump state info */
    void dumpState(void);
    /* dump queue info */
    void dumpQueue(void);

    /* set param */
    int setParam(struct v4l2_streamparm *stream_parm);
    void removeItemBufferQ();	

private:
    /* get pixel format */
    int  m_pixelDepth(void);
    /* check whether queued on index */
    bool m_getFlagQ(int index);
    /* set queue flag on index */
    bool m_setFlagQ(int index, bool toggle);
    /* polling */
    int m_polling(void);

    /* stream on */
    int m_streamOn(void);
    /* stream off */
    int m_streamOff(void);

    /* set input */
    int m_setInput(int id);
    /* set format */
    int m_setFmt(void);
    /* req buf */
    int m_reqBuffers(int *reqCount);
    /* clear buf */
    int m_clrBuffers(int *reqCount);
    /* set crop */
    int m_setCrop(int v4l2BufType, ExynosRect *rect);
    /* set contorl */
    int m_setControl(unsigned int id, int value);
    /* get contorl */
    int m_getControl(unsigned int id, int *value);

    /* qbuf from src, with metaBuf */
    int m_qBuf(ExynosCameraBuffer *buf);

    /* dqbuf */
    int m_dqBuf(ExynosCameraBuffer *buf, int *dqIndex);

    /* Buffer trace */
    status_t m_putBufferQ(ExynosCameraBuffer *buf, int *qindex);
    status_t m_getBufferQ(ExynosCameraBuffer *buf, int *dqindex);
    bool m_isExistBufferQ(ExynosCameraBuffer *buf);
    void m_printBufferQ();
    void m_removeItemBufferQ();

    /*
     * thoes member value should be declare in private
     * but we declare in publuc to support backward compatibility
     */
public:

private:
    ExynosCameraNodeRequest m_nodeRequest;

    bool               m_flagStart;
    bool               m_flagCreate;

    char               m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    char               m_alias[EXYNOS_CAMERA_NAME_STR_SIZE];

    int                m_fd;
    struct v4l2_format m_v4l2Format;
    struct v4l2_requestbuffers m_v4l2ReqBufs;
    struct v4l2_crop m_crop;

    bool               m_flagQ[VIDEO_MAX_FRAME];
    Mutex              m_qLock;

    bool               m_flagStreamOn;
    /* for reprocessing */
    bool               m_flagDup;

    int m_paramSate;
    mutable Mutex m_nodeStateLock;
    mutable Mutex m_nodeActionLock;

    Mutex              m_queueBufferListLock;
    ExynosCameraBuffer  m_queueBufferList[MAX_BUFFERS];

/*    ExynosCameraState m_nodeStateMgr; */
    int m_nodeState;
};

#endif /* EXYNOS_CAMERA_NODE_H__ */


