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
 * \file      ExynosCameraBufferManager.h
 * \brief     header file for ExynosCameraBufferManager
 * \author    Sunmi Lee(carrotsm.lee@samsung.com)
 * \date      2013/07/17
 *
 */

#ifndef EXYNOS_CAMERA_BUFFER_MANAGER_H__
#define EXYNOS_CAMERA_BUFFER_MANAGER_H__


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <utils/List.h>
#include <utils/threads.h>
#include <cutils/properties.h>

#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <videodev2.h>
#include <videodev2_exynos_camera.h>

#include "gralloc_priv.h"
#include "ion.h"

#include "fimc-is-metadata.h"
#include "ExynosCameraConfig.h"
#include "ExynosCameraAutoTimer.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraMemory.h"
#include "ExynosCameraThread.h"

namespace android {

/* #define DUMP_2_FILE */
/* #define EXYNOS_CAMERA_BUFFER_TRACE */

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
#define EXYNOS_CAMERA_BUFFER_IN()   CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__)
#define EXYNOS_CAMERA_BUFFER_OUT()  CLOGD("DEBUG(%s[%d]):OUT..", __FUNCTION__, __LINE__)
#else
#define EXYNOS_CAMERA_BUFFER_IN()   ((void *)0)
#define EXYNOS_CAMERA_BUFFER_OUT()  ((void *)0)
#endif

typedef enum buffer_manager_type {
    BUFFER_MANAGER_ION_TYPE         = 0,
    BUFFER_MANAGER_HEAP_BASE_TYPE   = 1,
    BUFFER_MANAGER_GRALLOC_TYPE     = 2,
    BUFFER_MANAGER_INVALID_TYPE,
} buffer_manager_type_t;

typedef enum buffer_manager_allocation_mode {
    BUFFER_MANAGER_ALLOCATION_ATONCE   = 0,   /* alloc() : allocation all buffers */
    BUFFER_MANAGER_ALLOCATION_ONDEMAND = 1,   /* alloc() : allocation the number of reqCount buffers, getBuffer() : increase buffers within limits */
    BUFFER_MANAGER_ALLOCATION_SILENT   = 2,   /* alloc() : same as ONDEMAND, increase buffers in background */
    BUFFER_MANAGER_ALLOCATION_INVALID_MODE,
} buffer_manager_allocation_mode_t;

class ExynosCameraBufferManager {
protected:
    ExynosCameraBufferManager();

public:
    static ExynosCameraBufferManager *createBufferManager(buffer_manager_type_t type);
    virtual ~ExynosCameraBufferManager();

    status_t create(const char *name, void *defaultAllocator);

    void     init(void);
    void     deinit(void);
    status_t resetBuffers(void);

    status_t setAllocator(void *allocator);

    status_t alloc(void);

    status_t setInfo(
                int planeCount,
                unsigned int size[],
                unsigned int bytePerLine[],
                int reqBufCount,
                bool createMetaPlane,
                bool needMmap = false);
    status_t setInfo(
                int planeCount,
                unsigned int size[],
                unsigned int bytePerLine[],
                int reqBufCount,
                int allowedMaxBufCount,
                exynos_camera_buffer_type_t type,
                bool createMetaPlane,
                bool needMmap = false);
    status_t setInfo(
                int planeCount,
                unsigned int size[],
                unsigned int bytePerLine[],
                int reqBufCount,
                int allowedMaxBufCount,
                exynos_camera_buffer_type_t type,
                buffer_manager_allocation_mode_t allocMode,
                bool createMetaPlane,
                bool needMmap = false);

    status_t putBuffer(
                int bufIndex,
                enum EXYNOS_CAMERA_BUFFER_POSITION position);
    status_t getBuffer(
                int    *reqBufIndex,
                enum   EXYNOS_CAMERA_BUFFER_POSITION position,
                struct ExynosCameraBuffer *buffer);

    status_t updateStatus(
                int bufIndex,
                int driverValue,
                enum EXYNOS_CAMERA_BUFFER_POSITION   position,
                enum EXYNOS_CAMERA_BUFFER_PERMISSION permission);
    status_t getStatus(
                int bufIndex,
                struct ExynosCameraBufferStatus *bufStatus);

    bool     isAllocated(void);
    bool     isAvaliable(int bufIndex);

    void     dump(void);
    void     dumpBufferInfo(void);
    int      getNumOfAvailableBuffer(void);
    int      getNumOfAvailableAndNoneBuffer(void);
    void     printBufferInfo(
                const char *funcName,
                const int lineNum,
                int bufIndex,
                int planeIndex);
    void     printBufferQState(void);
    virtual void printBufferState(void);
    virtual void printBufferState(int bufIndex, int planeIndex);

    virtual status_t cancelBuffer(int bufIndex);
    virtual status_t setBufferCount(int bufferCount);
    virtual int      getBufferCount(void);
    virtual int      getBufStride(void);

protected:
    status_t m_free(void);

    status_t m_setDefaultAllocator(void *allocator);
    status_t m_defaultAlloc(int bIndex, int eIndex, bool isMetaPlane);
    status_t m_defaultFree(int bIndex, int eIndex, bool isMetaPlane);

    bool     m_checkInfoForAlloc(void);
    status_t m_createDefaultAllocator(bool isCached = false);

    void     m_resetSequenceQ(void);

    virtual status_t m_setAllocator(void *allocator) = 0;
    virtual status_t m_alloc(int bIndex, int eIndex) = 0;
    virtual status_t m_free(int bIndex, int eIndex)  = 0;

    virtual status_t m_increase(int increaseCount) = 0;
    virtual status_t m_decrease(void) = 0;

    virtual status_t m_putBuffer(int bufIndex) = 0;
    virtual status_t m_getBuffer(int *bufIndex) = 0;

protected:
    bool                        m_flagAllocated;
    int                         m_reqBufCount;
    int                         m_allocatedBufCount;
    int                         m_allowedMaxBufCount;
    bool                        m_flagSkipAllocation;
    bool                        m_isDestructor;
    mutable Mutex               m_lock;
    bool                        m_flagNeedMmap;

    bool                        m_hasMetaPlane;
    /* using internal allocator (ION) for MetaData plane */
    ExynosCameraIonAllocator    *m_defaultAllocator;
    struct ExynosCameraBuffer   m_buffer[VIDEO_MAX_FRAME];
    char                        m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    List<int>                   m_availableBufferIndexQ;
    mutable Mutex               m_availableBufferIndexQLock;

    buffer_manager_allocation_mode_t m_allocMode;

private:
    typedef ExynosCameraThread<ExynosCameraBufferManager> allocThread;

    sp<allocThread>             m_allocationThread;
    bool                        m_allocationThreadFunc(void);
};

class InternalExynosCameraBufferManager : public ExynosCameraBufferManager {
public:
    InternalExynosCameraBufferManager();
    virtual ~InternalExynosCameraBufferManager();

protected:
    status_t m_setAllocator(void *allocator);

    status_t m_alloc(int bIndex, int eIndex);
    status_t m_free(int bIndex, int eIndex);

    status_t m_increase(int increaseCount);
    status_t m_decrease(void);

    status_t m_putBuffer(int bufIndex);
    status_t m_getBuffer(int *bufIndex);
};

class MHBExynosCameraBufferManager : public ExynosCameraBufferManager {
public:
    MHBExynosCameraBufferManager();
    virtual ~MHBExynosCameraBufferManager();

    status_t allocMulti();
    status_t getHeapMemory(
                int bufIndex,
                int planeIndex,
                camera_memory_t **heap);

protected:
    status_t m_setAllocator(void *allocator);

    status_t m_alloc(int bIndex, int eIndex);
    status_t m_free(int bIndex, int eIndex);

    status_t m_increase(int increaseCount);
    status_t m_decrease(void);

    status_t m_putBuffer(int bufIndex);
    status_t m_getBuffer(int *bufIndex);

private:
    ExynosCameraMHBAllocator *m_allocator;
    camera_memory_t          *m_heap[VIDEO_MAX_FRAME][EXYNOS_CAMERA_BUFFER_MAX_PLANES];
    int                      m_numBufsHeap;
};

class GrallocExynosCameraBufferManager : public ExynosCameraBufferManager {
public:
    GrallocExynosCameraBufferManager();
    virtual ~GrallocExynosCameraBufferManager();

    status_t cancelBuffer(int bufIndex);
    status_t setBufferCount(int bufferCount);
    int      getBufferCount(void);
    int      getBufStride(void);
    void     printBufferState(void);
    void     printBufferState(int bufIndex, int planeIndex);

protected:
    status_t m_setAllocator(void *allocator);

    status_t m_alloc(int bIndex, int eIndex);
    status_t m_free(int bIndex, int eIndex);

    status_t m_increase(int increaseCount);
    status_t m_decrease(void);

    status_t m_putBuffer(int bufIndex);
    status_t m_getBuffer(int *bufIndex);

private:
    ExynosCameraGrallocAllocator *m_allocator;
    buffer_handle_t              *m_handle[VIDEO_MAX_FRAME];
    bool                         m_handleIsLocked[VIDEO_MAX_FRAME];
    int                          m_dequeuedBufCount;
    int                          m_minUndequeuedBufCount;
    int                          m_bufferCount;
    int                          m_bufStride;
};
}
#endif
