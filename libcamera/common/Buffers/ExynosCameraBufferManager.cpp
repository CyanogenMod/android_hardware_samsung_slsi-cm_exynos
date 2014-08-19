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

#define LOG_TAG "ExynosCameraBufferManager"
#include "ExynosCameraBufferManager.h"

namespace android {

ExynosCameraBufferManager::ExynosCameraBufferManager()
{
    m_isDestructor = false;
    init();

    m_allocationThread = new allocThread(this, &ExynosCameraBufferManager::m_allocationThreadFunc, "allocationThreadFunc");
}

ExynosCameraBufferManager::~ExynosCameraBufferManager()
{
    m_isDestructor = true;
    deinit();
}

ExynosCameraBufferManager *ExynosCameraBufferManager::createBufferManager(buffer_manager_type_t type)
{
    switch (type) {
    case BUFFER_MANAGER_ION_TYPE:
        return (ExynosCameraBufferManager *)new InternalExynosCameraBufferManager();
        break;
    case BUFFER_MANAGER_HEAP_BASE_TYPE:
        return (ExynosCameraBufferManager *)new MHBExynosCameraBufferManager();
        break;
    case BUFFER_MANAGER_GRALLOC_TYPE:
        return (ExynosCameraBufferManager *)new GrallocExynosCameraBufferManager();
        break;
    case BUFFER_MANAGER_INVALID_TYPE:
        ALOGE("ERR(%s[%d]):Unknown bufferManager type(%d)", __FUNCTION__, __LINE__, (int)type);
    default:
        break;
    }

    return NULL;
}

status_t ExynosCameraBufferManager::create(const char *name, void *defaultAllocator)
{
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;

    strncpy(m_name, name, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    if (defaultAllocator == NULL) {
        if (m_createDefaultAllocator(false) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_createDefaultAllocator failed", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    } else {
        if (m_setDefaultAllocator(defaultAllocator) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_setDefaultAllocator failed", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    return ret;
}

void ExynosCameraBufferManager::init(void)
{
    EXYNOS_CAMERA_BUFFER_IN();

    m_flagAllocated = false;
    m_reqBufCount  = 0;
    m_allocatedBufCount  = 0;
    m_allowedMaxBufCount = 0;
    m_defaultAllocator = NULL;
    memset((void *)m_buffer, 0, (VIDEO_MAX_FRAME) * sizeof(struct ExynosCameraBuffer));
    m_hasMetaPlane = false;
    memset(m_name, 0x00, sizeof(m_name));
    strncpy(m_name, "none", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_flagSkipAllocation = false;
    m_flagNeedMmap = false;
    m_allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    EXYNOS_CAMERA_BUFFER_OUT();
}

void ExynosCameraBufferManager::deinit(void)
{
    CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__);

    if (m_flagAllocated == false) {
        CLOGI("INFO(%s[%d]:OUT.. Buffer is not allocated", __FUNCTION__, __LINE__);
        return;
    }

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_SILENT) {
        m_allocationThread->join();
        CLOGI("INFO(%s[%d]):allocationThread is finished", __FUNCTION__, __LINE__);
    }

    for (int bufIndex = 0; bufIndex < m_allocatedBufCount; bufIndex++)
        cancelBuffer(bufIndex);

    if (m_free() != NO_ERROR)
        CLOGE("ERR(%s[%d])::free failed", __FUNCTION__, __LINE__);

    m_flagSkipAllocation = false;
    CLOGD("DEBUG(%s[%d]):OUT..", __FUNCTION__, __LINE__);
}

status_t ExynosCameraBufferManager::resetBuffers(void)
{
    /* same as deinit */
    /* clear buffers except releasing the memory */
    CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__);
    status_t ret = NO_ERROR;

    if (m_flagAllocated == false) {
        CLOGI("INFO(%s[%d]:OUT.. Buffer is not allocated", __FUNCTION__, __LINE__);
        return ret;
    }

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_SILENT) {
        m_allocationThread->join();
        CLOGI("INFO(%s[%d]):allocationThread is finished", __FUNCTION__, __LINE__);
    }

    for (int bufIndex = 0; bufIndex < m_allocatedBufCount; bufIndex++)
        cancelBuffer(bufIndex);

    m_resetSequenceQ();
    m_flagSkipAllocation = true;

    return ret;
}

status_t ExynosCameraBufferManager::setAllocator(void *allocator)
{
    Mutex::Autolock lock(m_lock);

    if (allocator == NULL) {
        CLOGE("ERR(%s[%d]):m_allocator equals NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    return m_setAllocator(allocator);
}

status_t ExynosCameraBufferManager::alloc(void)
{
    EXYNOS_CAMERA_BUFFER_IN();
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;

    if (m_flagSkipAllocation == true) {
        CLOGI("INFO(%s[%d]):skip to allocate memory (m_flagSkipAllocation=%d)",
            __FUNCTION__, __LINE__, (int)m_flagSkipAllocation);
        goto func_exit;
    }

    if (m_checkInfoForAlloc() == false) {
        CLOGE("ERR(%s[%d]):m_checkInfoForAlloc failed", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* allocate image buffer */
    if (m_alloc(0, m_reqBufCount) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_alloc failed", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_hasMetaPlane == true) {
        if (m_defaultAlloc(0, m_reqBufCount, m_hasMetaPlane) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_defaultAlloc failed", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    m_allocatedBufCount = m_reqBufCount;
    m_resetSequenceQ();
    m_flagAllocated = true;

    CLOGD("DEBUG(%s[%d]):Allocate the buffer succeeded "
          "(m_allocatedBufCount=%d, m_reqBufCount=%d, m_allowedMaxBufCount=%d) --- dumpBufferInfo ---",
        __FUNCTION__, __LINE__,
        m_allocatedBufCount, m_reqBufCount, m_allowedMaxBufCount);
    dumpBufferInfo();
    CLOGD("DEBUG(%s[%d]):------------------------------------------------------------------",
        __FUNCTION__, __LINE__);

    if (m_allocMode == BUFFER_MANAGER_ALLOCATION_SILENT) {
        /* run the allocationThread */
        m_allocationThread->run(PRIORITY_DEFAULT);
        CLOGI("INFO(%s[%d]):allocationThread is started", __FUNCTION__, __LINE__);
    }

func_exit:

    m_flagSkipAllocation = false;
    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::m_free(void)
{
    EXYNOS_CAMERA_BUFFER_IN();
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    Mutex::Autolock lock(m_lock);

    CLOGD("DEBUG(%s[%d]):Free the buffer (m_allocatedBufCount=%d) --- dumpBufferInfo ---",
        __FUNCTION__, __LINE__, m_allocatedBufCount);
    dumpBufferInfo();
    CLOGD("DEBUG(%s[%d]):------------------------------------------------------",
        __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    if (m_flagAllocated != false) {
        if (m_free(0, m_allocatedBufCount) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_free failed", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            goto func_exit;
        }

        if (m_hasMetaPlane == true) {
            if (m_defaultFree(0, m_allocatedBufCount, m_hasMetaPlane) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_defaultFree failed", __FUNCTION__, __LINE__);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
        }
        m_availableBufferIndexQLock.lock();
        m_availableBufferIndexQ.clear();
        m_availableBufferIndexQLock.unlock();
        m_allocatedBufCount  = 0;
        m_allowedMaxBufCount = 0;
        m_flagAllocated = false;
    }

    CLOGD("DEBUG(%s[%d]):Free the buffer succeeded (m_allocatedBufCount=%d)",
        __FUNCTION__, __LINE__, m_allocatedBufCount);

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

void ExynosCameraBufferManager::m_resetSequenceQ()
{
    Mutex::Autolock lock(m_availableBufferIndexQLock);
    m_availableBufferIndexQ.clear();

    for (int bufIndex = 0; bufIndex < m_allocatedBufCount; bufIndex++)
        m_availableBufferIndexQ.push_back(m_buffer[bufIndex].index);

    return;
}

/*  If Image buffer color format equals YV12, and buffer has MetaDataPlane..

    planeCount = 4      (set by user)
    size[0] : Image buffer plane Y size
    size[1] : Image buffer plane Cr size
    size[2] : Image buffer plane Cb size

    if (createMetaPlane == true)
        size[3] = EXYNOS_CAMERA_META_PLANE_SIZE;    (set by BufferManager, internally)
*/
status_t ExynosCameraBufferManager::setInfo(
        int planeCount,
        unsigned int size[],
        unsigned int bytePerLine[],
        int reqBufCount,
        bool createMetaPlane,
        bool needMmap)
{
    status_t ret = NO_ERROR;

    ret = setInfo(
            planeCount,
            size,
            bytePerLine,
            reqBufCount,
            reqBufCount,
            EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE,
            BUFFER_MANAGER_ALLOCATION_ATONCE,
            createMetaPlane,
            needMmap);
    if (ret < 0)
        ALOGE("ERR(%s[%d]):setInfo fail", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCameraBufferManager::setInfo(
        int planeCount,
        unsigned int size[],
        unsigned int bytePerLine[],
        int reqBufCount,
        int allowedMaxBufCount,
        exynos_camera_buffer_type_t type,
        bool createMetaPlane,
        bool needMmap)
{
    status_t ret = NO_ERROR;

    ret = setInfo(
            planeCount,
            size,
            bytePerLine,
            reqBufCount,
            allowedMaxBufCount,
            type,
            BUFFER_MANAGER_ALLOCATION_ONDEMAND,
            createMetaPlane,
            needMmap);
    if (ret < 0)
        ALOGE("ERR(%s[%d]):setInfo fail", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCameraBufferManager::setInfo(
        int planeCount,
        unsigned int size[],
        unsigned int bytePerLine[],
        int reqBufCount,
        int allowedMaxBufCount,
        exynos_camera_buffer_type_t type,
        buffer_manager_allocation_mode_t allocMode,
        bool createMetaPlane,
        bool needMmap)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;

    if (createMetaPlane == true) {
        size[planeCount-1] = EXYNOS_CAMERA_META_PLANE_SIZE;
        m_hasMetaPlane = true;
    }

    if (allowedMaxBufCount < reqBufCount) {
        CLOGW("WARN(%s[%d]):abnormal value [reqBufCount=%d, allowedMaxBufCount=%d]",
            __FUNCTION__, __LINE__, reqBufCount, allowedMaxBufCount);
        allowedMaxBufCount = reqBufCount;
    }

    if (reqBufCount < 0 || VIDEO_MAX_FRAME <= reqBufCount) {
        CLOGE("ERR(%s[%d]):abnormal value [reqBufCount=%d]",
            __FUNCTION__, __LINE__, reqBufCount);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (planeCount < 0 || EXYNOS_CAMERA_BUFFER_MAX_PLANES <= planeCount) {
        CLOGE("ERR(%s[%d]):abnormal value [planeCount=%d]",
            __FUNCTION__, __LINE__, planeCount);
        ret = BAD_VALUE;
        goto func_exit;
    }

    for (int bufIndex = 0; bufIndex < reqBufCount; bufIndex++) {
        for (int planeIndex = 0; planeIndex < planeCount; planeIndex++) {
            if (size[planeIndex] == 0) {
                CLOGE("ERR(%s[%d]):abnormal value [size=%d]",
                    __FUNCTION__, __LINE__, size[planeIndex]);
                ret = BAD_VALUE;
                goto func_exit;
            }
            m_buffer[bufIndex].size[planeIndex]         = size[planeIndex];
            m_buffer[bufIndex].bytesPerLine[planeIndex] = bytePerLine[planeIndex];
        }
        m_buffer[bufIndex].planeCount = planeCount;
        m_buffer[bufIndex].type       = type;
    }
    m_allowedMaxBufCount = allowedMaxBufCount;
    m_reqBufCount  = reqBufCount;
    m_flagNeedMmap = needMmap;
    m_allocMode    = allocMode;

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

bool ExynosCameraBufferManager::m_allocationThreadFunc(void)
{
    status_t ret = NO_ERROR;
    int increaseCount = 1;

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]:increase buffer silently - start - "
          "(m_allowedMaxBufCount=%d, m_allocatedBufCount=%d, m_reqBufCount=%d)",
        __FUNCTION__, __LINE__,
        m_allowedMaxBufCount, m_allocatedBufCount, m_reqBufCount);

    increaseCount = m_allowedMaxBufCount - m_reqBufCount;

    /* increase buffer*/
    for (int count = 0; count < increaseCount; count++) {
        ret = m_increase(1);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):increase the buffer failed", __FUNCTION__, __LINE__);
        } else {
            m_lock.lock();
            m_availableBufferIndexQ.push_back(m_buffer[m_allocatedBufCount].index);
            m_allocatedBufCount++;
            m_lock.unlock();
        }

    }
    dumpBufferInfo();
    CLOGI("INFO(%s[%d]:increase buffer silently - end - (increaseCount=%d)"
          "(m_allowedMaxBufCount=%d, m_allocatedBufCount=%d, m_reqBufCount=%d)",
        __FUNCTION__, __LINE__, increaseCount,
        m_allowedMaxBufCount, m_allocatedBufCount, m_reqBufCount);

    /* false : Thread run once */
    return false;
}

status_t ExynosCameraBufferManager::putBuffer(
        int bufIndex,
        enum EXYNOS_CAMERA_BUFFER_POSITION position)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    List<int>::iterator r;
    bool found = false;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;

    permission = EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE;

    if (bufIndex < 0 || m_allocatedBufCount <= bufIndex) {
        CLOGE("ERR(%s[%d]):buffer Index in out of bound [bufIndex=%d], allocatedBufCount(%d)",
            __FUNCTION__, __LINE__, bufIndex, m_allocatedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        if (bufIndex == *r) {
            found = true;
            break;
        }
    }
    m_availableBufferIndexQLock.unlock();

    if (found == true) {
        CLOGI("INFO(%s[%d]):bufIndex=%d is already in (available state)",
            __FUNCTION__, __LINE__, bufIndex);
        goto func_exit;
    }

    if (m_putBuffer(bufIndex) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_putBuffer failed [bufIndex=%d, position=%d, permission=%d]",
            __FUNCTION__, __LINE__, bufIndex, (int)position, (int)permission);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (updateStatus(bufIndex, 0, position, permission) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setStatus failed [bufIndex=%d, position=%d, permission=%d]",
            __FUNCTION__, __LINE__, bufIndex, (int)position, (int)permission);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    m_availableBufferIndexQ.push_back(m_buffer[bufIndex].index);
    m_availableBufferIndexQLock.unlock();

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

/* User Process need to check the index of buffer returned from "getBuffer()" */
status_t ExynosCameraBufferManager::getBuffer(
        int  *reqBufIndex,
        enum EXYNOS_CAMERA_BUFFER_POSITION position,
        struct ExynosCameraBuffer *buffer)
{
    EXYNOS_CAMERA_BUFFER_IN();
    Mutex::Autolock lock(m_lock);

    status_t ret = NO_ERROR;
    List<int>::iterator r;

    int  bufferIndex;
    enum EXYNOS_CAMERA_BUFFER_PERMISSION permission;

    bufferIndex = *reqBufIndex;
    permission = EXYNOS_CAMERA_BUFFER_PERMISSION_NONE;

    if (m_allocatedBufCount == 0) {
        CLOGE("ERR(%s[%d]):m_allocatedBufCount equals zero", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_getBuffer(&bufferIndex) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_getBuffer failed [bufferIndex=%d, position=%d, permission=%d]",
            __FUNCTION__, __LINE__, bufferIndex, (int)position, (int)permission);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

reDo:

    if (bufferIndex < 0 || m_allocatedBufCount <= bufferIndex) {
        /* find availableBuffer */
        m_availableBufferIndexQLock.lock();
        if (m_availableBufferIndexQ.empty() == false) {
            r = m_availableBufferIndexQ.begin();
            bufferIndex = *r;
            m_availableBufferIndexQ.erase(r);
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGI("INFO(%s[%d]):available buffer [index=%d]...",
                __FUNCTION__, __LINE__, bufferIndex);
#endif
        }
        m_availableBufferIndexQLock.unlock();
    } else {
        m_availableBufferIndexQLock.lock();
        /* get the Buffer of requested */
        for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
            if (bufferIndex == *r) {
                m_availableBufferIndexQ.erase(r);
                break;
            }
        }
        m_availableBufferIndexQLock.unlock();
    }

    if (0 <= bufferIndex && bufferIndex < m_allocatedBufCount) {
        /* found buffer */
        if (isAvaliable(bufferIndex) == false) {
            CLOGE("ERR(%s[%d]):isAvaliable failed [bufferIndex=%d]",
                __FUNCTION__, __LINE__, bufferIndex);
            ret = BAD_VALUE;
            goto func_exit;
        }

        permission = EXYNOS_CAMERA_BUFFER_PERMISSION_IN_PROCESS;

        if (updateStatus(bufferIndex, 0, position, permission) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=%d, permission=%d]",
                __FUNCTION__, __LINE__, bufferIndex, (int)position, (int)permission);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    } else {
        /* do not find buffer */
        if (m_allocMode == BUFFER_MANAGER_ALLOCATION_ONDEMAND) {
            /* increase buffer*/
            ret = m_increase(1);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):increase the buffer failed, m_allocatedBufCount %d, bufferIndex %d",
                    __FUNCTION__, __LINE__,  m_allocatedBufCount, bufferIndex);
            } else {
                m_availableBufferIndexQLock.lock();
                m_availableBufferIndexQ.push_back(m_allocatedBufCount);
                m_availableBufferIndexQLock.unlock();
                bufferIndex = m_allocatedBufCount;
                m_allocatedBufCount++;

                dumpBufferInfo();
                CLOGI("INFO(%s[%d]):increase the buffer succeeded (bufferIndex=%d)",
                    __FUNCTION__, __LINE__, bufferIndex);
                goto reDo;
            }
        } else {
            ret = INVALID_OPERATION;
        }

        if (ret < 0) {
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGD("DEBUG(%s[%d]):find free buffer... failed --- dump ---",
                __FUNCTION__, __LINE__);
            dump();
            CLOGD("DEBUG(%s[%d]):----------------------------------------",
                __FUNCTION__, __LINE__);
            CLOGD("DEBUG(%s[%d]):buffer Index in out of bound [bufferIndex=%d]",
                __FUNCTION__, __LINE__, bufferIndex);
#endif
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    m_buffer[bufferIndex].index = bufferIndex;
    *reqBufIndex = bufferIndex;
    *buffer      = m_buffer[bufferIndex];

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::cancelBuffer(int bufIndex)
{
    int ret = putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
    return ret;
}

int ExynosCameraBufferManager::getBufStride(void)
{
    return 0;
}

status_t ExynosCameraBufferManager::updateStatus(
        int bufIndex,
        int driverValue,
        enum EXYNOS_CAMERA_BUFFER_POSITION   position,
        enum EXYNOS_CAMERA_BUFFER_PERMISSION permission)
{
    m_buffer[bufIndex].index = bufIndex;
    m_buffer[bufIndex].status.driverReturnValue = driverValue;
    m_buffer[bufIndex].status.position          = position;
    m_buffer[bufIndex].status.permission        = permission;

    return NO_ERROR;
}

status_t ExynosCameraBufferManager::getStatus(
        int bufIndex,
        struct ExynosCameraBufferStatus *bufStatus)
{
    *bufStatus = m_buffer[bufIndex].status;

    return NO_ERROR;
}

bool ExynosCameraBufferManager::isAllocated(void)
{
    return m_flagAllocated;
}

bool ExynosCameraBufferManager::isAvaliable(int bufIndex)
{
    bool ret = false;

    switch (m_buffer[bufIndex].status.permission) {
    case EXYNOS_CAMERA_BUFFER_PERMISSION_NONE:
    case EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE:
        ret = true;
        break;

    case EXYNOS_CAMERA_BUFFER_PERMISSION_IN_PROCESS:
    default:
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
        CLOGD("DEBUG(%s[%d]):buffer is not available", __FUNCTION__, __LINE__);
        dump();
#endif
        ret = false;
        break;
    }

    return ret;
}

status_t ExynosCameraBufferManager::m_setDefaultAllocator(void *allocator)
{
    m_defaultAllocator = (ExynosCameraIonAllocator *)allocator;

    return NO_ERROR;
}

status_t ExynosCameraBufferManager::m_defaultAlloc(int bIndex, int eIndex, bool isMetaPlane)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    int planeIndexStart = 0;
    int planeIndexEnd   = 0;
    bool mapNeeded      = false;
#ifdef DEBUG_RAWDUMP
    char enableRawDump[PROP_VALUE_MAX];
#endif /* DEBUG_RAWDUMP */

    int mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
    int flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;

    ExynosCameraDurationTimer m_timer;
    long long    durationTime = 0;
    long long    durationTimeSum = 0;
    unsigned int estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
    unsigned int estimatedTime = 0;
    unsigned int bufferSize = 0;
    int          reservedMaxCount = 0;

    if (m_defaultAllocator == NULL) {
        CLOGE("ERR(%s[%d]):m_defaultAllocator equals NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (isMetaPlane == true) {
        mapNeeded = true;
    } else {
#ifdef DEBUG_RAWDUMP
        /* to make file dump, we should make user vritual table */
        property_get("ro.debug.rawdump", enableRawDump, "0");
        if (strcmp(enableRawDump, "1") == 0)
            mapNeeded = true;
        else
#endif
        {
            mapNeeded = m_flagNeedMmap;
        }
    }

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (isMetaPlane == true) {
            planeIndexStart = m_buffer[bufIndex].planeCount-1;
            planeIndexEnd   = m_buffer[bufIndex].planeCount;
            mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
            flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
            estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
        } else {
            planeIndexStart = 0;
            planeIndexEnd   = (m_hasMetaPlane ?
                m_buffer[bufIndex].planeCount-1 : m_buffer[bufIndex].planeCount);
            switch (m_buffer[bufIndex].type) {
            case EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE:
                mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
                estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
                break;
            case EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE:
                mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_CACHED;
                flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_CACHED;
                estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_CACHED;
                break;
            case EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE:
#ifdef RESERVED_MEMORY_ENABLE
                reservedMaxCount = RESERVED_BUFFER_COUNT_MAX;
#else
                reservedMaxCount = 0;
#endif
                if (bufIndex < reservedMaxCount) {
                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_RESERVED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_RESERVED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_RESERVED;
                } else {
                    mask  = EXYNOS_CAMERA_BUFFER_ION_MASK_NONCACHED;
                    flags = EXYNOS_CAMERA_BUFFER_ION_FLAG_NONCACHED;
                    estimatedBase = EXYNOS_CAMERA_BUFFER_ION_WARNING_TIME_NONCACHED;
                }
                break;
            case EXYNOS_CAMERA_BUFFER_INVALID_TYPE:
            default:
                CLOGE("ERR(%s[%d]):buffer type is invaild (%d)", __FUNCTION__, __LINE__, (int)m_buffer[bufIndex].type);
                break;
            }
        }

        if (isMetaPlane == false) {
            m_timer.start();
            bufferSize = 0;
        }

        for (int planeIndex = planeIndexStart; planeIndex < planeIndexEnd; planeIndex++) {
            if (m_buffer[bufIndex].addr[planeIndex] != NULL) {
                CLOGE("ERR(%s[%d]):buffer[%d].addr[%d] already allocated",
                        __FUNCTION__, __LINE__, bufIndex, planeIndex);
                continue;
            }

            if (m_defaultAllocator->alloc(
                    m_buffer[bufIndex].size[planeIndex],
                    &(m_buffer[bufIndex].fd[planeIndex]),
                    &(m_buffer[bufIndex].addr[planeIndex]),
                    mask,
                    flags,
                    mapNeeded) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_defaultAllocator->alloc(bufIndex=%d, planeIndex=%d, planeIndex=%d) failed",
                    __FUNCTION__, __LINE__, bufIndex, planeIndex, m_buffer[bufIndex].size[planeIndex]);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            printBufferInfo(__FUNCTION__, __LINE__, bufIndex, planeIndex);
#endif
            if (isMetaPlane == false)
                bufferSize = bufferSize + m_buffer[bufIndex].size[planeIndex];
        }
        if (isMetaPlane == false) {
            m_timer.stop();
            durationTime = m_timer.durationMsecs();
            durationTimeSum += durationTime;
            CLOGD("DEBUG(%s[%d]):duration time(%5d msec):(type=%d, bufIndex=%d, size=%d)",
                __FUNCTION__, __LINE__, (int)durationTime, m_buffer[bufIndex].type, bufIndex, (int)bufferSize);

            estimatedTime = estimatedBase * bufferSize / EXYNOS_CAMERA_BUFFER_1MB;
            if (estimatedTime < durationTime) {
                CLOGW("WARN(%s[%d]):estimated time(%5d msec):(type=%d, bufIndex=%d, size=%d)",
                    __FUNCTION__, __LINE__, (int)estimatedTime, m_buffer[bufIndex].type, bufIndex, (int)bufferSize);
            }
        }

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }
    CLOGD("DEBUG(%s[%d]):Duration time of buffer allocation(%5d msec)", __FUNCTION__, __LINE__, (int)durationTimeSum);

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::m_defaultFree(int bIndex, int eIndex, bool isMetaPlane)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    int planeIndexStart = 0;
    int planeIndexEnd   = 0;
    bool mapNeeded      = false;
#ifdef DEBUG_RAWDUMP
    char enableRawDump[PROP_VALUE_MAX];
#endif /* DEBUG_RAWDUMP */

    if (isMetaPlane == true) {
        mapNeeded = true;
    } else {
#ifdef DEBUG_RAWDUMP
        /* to make file dump, we should make user vritual table */
        property_get("ro.debug.rawdump", enableRawDump, "0");
        if (strcmp(enableRawDump, "1") == 0)
            mapNeeded = true;
        else
#endif
        {
            mapNeeded = m_flagNeedMmap;
        }
    }

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (isAvaliable(bufIndex) == false) {
            CLOGE("ERR(%s[%d]):buffer [bufIndex=%d] in InProcess state",
                __FUNCTION__, __LINE__, bufIndex);
            if (m_isDestructor == false) {
                ret = BAD_VALUE;
                continue;
            } else {
                CLOGE("ERR(%s[%d]):buffer [bufIndex=%d] in InProcess state, but try to forcedly free",
                    __FUNCTION__, __LINE__, bufIndex);
            }
        }

        if (isMetaPlane == true) {
            planeIndexStart = m_buffer[bufIndex].planeCount-1;
            planeIndexEnd   = m_buffer[bufIndex].planeCount;
        } else {
            planeIndexStart = 0;
            planeIndexEnd   = (m_hasMetaPlane ?
                m_buffer[bufIndex].planeCount-1 : m_buffer[bufIndex].planeCount);
        }

        for (int planeIndex = planeIndexStart; planeIndex < planeIndexEnd; planeIndex++) {
            if (m_defaultAllocator->free(
                    m_buffer[bufIndex].size[planeIndex],
                    &(m_buffer[bufIndex].fd[planeIndex]),
                    &(m_buffer[bufIndex].addr[planeIndex]),
                    mapNeeded) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_defaultAllocator->free for Imagedata Plane failed",
                    __FUNCTION__, __LINE__);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
        }

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

bool ExynosCameraBufferManager::m_checkInfoForAlloc(void)
{
    EXYNOS_CAMERA_BUFFER_IN();

    bool ret = true;

    if (m_reqBufCount < 0 || VIDEO_MAX_FRAME <= m_reqBufCount) {
        CLOGE("ERR(%s[%d]):buffer Count in out of bound [m_reqBufCount=%d]",
            __FUNCTION__, __LINE__, m_reqBufCount);
        ret = false;
        goto func_exit;
    }

    for (int bufIndex = 0; bufIndex < m_reqBufCount; bufIndex++) {
        if (m_buffer[bufIndex].planeCount < 0
         || VIDEO_MAX_PLANES <= m_buffer[bufIndex].planeCount) {
            CLOGE("ERR(%s[%d]):plane Count in out of bound [m_buffer[bIndex].planeCount=%d]",
                __FUNCTION__, __LINE__, m_buffer[bufIndex].planeCount);
            ret = false;
            goto func_exit;
        }

        for (int planeIndex = 0; planeIndex < m_buffer[bufIndex].planeCount; planeIndex++) {
            if (m_buffer[bufIndex].size[planeIndex] == 0) {
                CLOGE("ERR(%s[%d]):size is empty [m_buffer[%d].size[%d]=%d]",
                    __FUNCTION__, __LINE__, bufIndex, planeIndex, m_buffer[bufIndex].size[planeIndex]);
                ret = false;
                goto func_exit;
            }
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t ExynosCameraBufferManager::m_createDefaultAllocator(bool isCached)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;

    m_defaultAllocator = new ExynosCameraIonAllocator();
    if (m_defaultAllocator->init(isCached) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_defaultAllocator->init failed", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

int ExynosCameraBufferManager::getNumOfAvailableBuffer(void)
{
    int numAvailable = 0;

    for (int i = 0; i < m_allocatedBufCount; i++) {
        if (m_buffer[i].status.permission == EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE)
            numAvailable++;
    }

    return numAvailable;
}

int ExynosCameraBufferManager::getNumOfAvailableAndNoneBuffer(void)
{
    int numAvailable = 0;

    for (int i = 0; i < m_allocatedBufCount; i++) {
        if (m_buffer[i].status.permission == EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE ||
            m_buffer[i].status.permission == EXYNOS_CAMERA_BUFFER_PERMISSION_NONE)
            numAvailable++;
    }

    return numAvailable;
}

void ExynosCameraBufferManager::printBufferState(void)
{
    for (int i = 0; i < m_allocatedBufCount; i++) {
        CLOGI("INFO(%s[%d]):m_buffer[%d].fd[0]=%d, position=%d, permission=%d]",
            __FUNCTION__, __LINE__, i, m_buffer[i].fd[0],
            m_buffer[i].status.position, m_buffer[i].status.permission);
    }

    return;
}

void ExynosCameraBufferManager::printBufferState(int bufIndex, int planeIndex)
{
    CLOGI("INFO(%s[%d]):m_buffer[%d].fd[%d]=%d, .status.permission=%d]",
        __FUNCTION__, __LINE__, bufIndex, planeIndex, m_buffer[bufIndex].fd[planeIndex],
        m_buffer[bufIndex].status.permission);

    return;
}

void ExynosCameraBufferManager::printBufferQState()
{
    List<int>::iterator r;
    int  bufferIndex;

    Mutex::Autolock lock(m_availableBufferIndexQLock);

    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        bufferIndex = *r;
        CLOGD("DEBUG(%s[%d]):bufferIndex=%d", __FUNCTION__, __LINE__, bufferIndex);
    }

    return;
}

void ExynosCameraBufferManager::printBufferInfo(
        const char *funcName,
        const int lineNum,
        int bufIndex,
        int planeIndex)
{
    CLOGI("INFO(%s[%d]):[m_buffer[%d].fd[%d]=%d] .addr=%p .size=%d]",
        funcName, lineNum, bufIndex, planeIndex,
        m_buffer[bufIndex].fd[planeIndex],
        m_buffer[bufIndex].addr[planeIndex],
        m_buffer[bufIndex].size[planeIndex]);

    return;
}

void ExynosCameraBufferManager::dump(void)
{
    printBufferState();
    printBufferQState();

    return;
}

void ExynosCameraBufferManager::dumpBufferInfo(void)
{
    for (int bufIndex = 0; bufIndex < m_allocatedBufCount; bufIndex++)
        for (int planeIndex = 0; planeIndex < m_buffer[bufIndex].planeCount; planeIndex++) {
            CLOGI("INFO(%s[%d]):[m_buffer[%d].fd[%d]=%d] .addr=%p .size=%d .position=%d .permission=%d]",
                __FUNCTION__, __LINE__, m_buffer[bufIndex].index, planeIndex,
                m_buffer[bufIndex].fd[planeIndex],
                m_buffer[bufIndex].addr[planeIndex],
                m_buffer[bufIndex].size[planeIndex],
                m_buffer[bufIndex].status.position,
                m_buffer[bufIndex].status.permission);
    }
    printBufferQState();

    return;
}

status_t ExynosCameraBufferManager::setBufferCount(int bufferCount)
{
    CLOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

int ExynosCameraBufferManager::getBufferCount(void)
{
    CLOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return 0;
}

InternalExynosCameraBufferManager::InternalExynosCameraBufferManager()
{
    ExynosCameraBufferManager::init();
}

InternalExynosCameraBufferManager::~InternalExynosCameraBufferManager()
{
    ExynosCameraBufferManager::deinit();
}

status_t InternalExynosCameraBufferManager::m_setAllocator(void *allocator)
{
    return m_setDefaultAllocator(allocator);
}

status_t InternalExynosCameraBufferManager::m_alloc(int bIndex, int eIndex)
{
    return m_defaultAlloc(bIndex, eIndex, false);
}

status_t InternalExynosCameraBufferManager::m_free(int bIndex, int eIndex)
{
    return m_defaultFree(bIndex, eIndex, false);
}

status_t InternalExynosCameraBufferManager::m_increase(int increaseCount)
{
    CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__);
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_allowedMaxBufCount <= m_allocatedBufCount) {
        CLOGD("DEBUG(%s[%d]):BufferManager can't increase the buffer "
              "(m_reqBufCount=%d, m_allowedMaxBufCount=%d <= m_allocatedBufCount=%d)",
            __FUNCTION__, __LINE__,
            m_reqBufCount, m_allowedMaxBufCount, m_allocatedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_allowedMaxBufCount < m_allocatedBufCount + increaseCount) {
        CLOGI("INFO(%s[%d]):change the increaseCount (%d->%d) --- "
              "(m_reqBufCount=%d, m_allowedMaxBufCount=%d <= m_allocatedBufCount=%d + increaseCount=%d)",
            __FUNCTION__, __LINE__, increaseCount, m_allowedMaxBufCount - m_allocatedBufCount,
            m_reqBufCount, m_allowedMaxBufCount, m_allocatedBufCount, increaseCount);
        increaseCount = m_allowedMaxBufCount - m_allocatedBufCount;
    }

    /* set the buffer information */
    for (int bufIndex = m_allocatedBufCount; bufIndex < m_allocatedBufCount + increaseCount; bufIndex++) {
        for (int planeIndex = 0; planeIndex < m_buffer[0].planeCount; planeIndex++) {
            if (m_buffer[0].size[planeIndex] == 0) {
                CLOGE("ERR(%s[%d]):abnormal value [size=%d]",
                    __FUNCTION__, __LINE__, m_buffer[0].size[planeIndex]);
                ret = BAD_VALUE;
                goto func_exit;
            }
            m_buffer[bufIndex].size[planeIndex]         = m_buffer[0].size[planeIndex];
            m_buffer[bufIndex].bytesPerLine[planeIndex] = m_buffer[0].bytesPerLine[planeIndex];
        }
        m_buffer[bufIndex].planeCount = m_buffer[0].planeCount;
        m_buffer[bufIndex].type       = m_buffer[0].type;
    }

    if (m_alloc(m_allocatedBufCount, m_allocatedBufCount + increaseCount) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_alloc failed", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_hasMetaPlane == true) {
        if (m_defaultAlloc(m_allocatedBufCount, m_allocatedBufCount + increaseCount, m_hasMetaPlane) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_defaultAlloc failed", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    CLOGD("DEBUG(%s[%d]):Increase the buffer succeeded (m_allocatedBufCount=%d, increaseCount=%d)",
        __FUNCTION__, __LINE__, m_allocatedBufCount, increaseCount);

func_exit:

    CLOGD("DEBUG(%s[%d]):OUT.." , __FUNCTION__, __LINE__);

    return ret;
}

status_t InternalExynosCameraBufferManager::m_decrease(void)
{
    CLOGD("DEBUG(%s[%d]):IN.." , __FUNCTION__, __LINE__);
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = true;
    List<int>::iterator r;

    int  bufferIndex = -1;

    if (m_allocatedBufCount <= m_reqBufCount) {
        CLOGD("DEBUG(%s[%d]):BufferManager can't decrease the buffer "
              "(m_allowedMaxBufCount=%d, m_allocatedBufCount=%d <= m_reqBufCount=%d)",
            __FUNCTION__, __LINE__,
            m_allowedMaxBufCount, m_allocatedBufCount, m_reqBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    bufferIndex = m_allocatedBufCount;

    if (m_free(bufferIndex-1, bufferIndex) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_free failed", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_hasMetaPlane == true) {
        if (m_defaultFree(bufferIndex-1, bufferIndex, m_hasMetaPlane) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_defaultFree failed", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    m_availableBufferIndexQLock.lock();
    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        if (bufferIndex == *r) {
            m_availableBufferIndexQ.erase(r);
            break;
        }
    }
    m_availableBufferIndexQLock.unlock();
    m_allocatedBufCount--;

    CLOGD("DEBUG(%s[%d]):Decrease the buffer succeeded (m_allocatedBufCount=%d)" ,
        __FUNCTION__, __LINE__, m_allocatedBufCount);

func_exit:

    CLOGD("DEBUG(%s[%d]):OUT.." , __FUNCTION__, __LINE__);

    return ret;
}

status_t InternalExynosCameraBufferManager::m_putBuffer(int bufIndex)
{
    return NO_ERROR;
}

status_t InternalExynosCameraBufferManager::m_getBuffer(int *bufIndex)
{
    return NO_ERROR;
}

MHBExynosCameraBufferManager::MHBExynosCameraBufferManager()
{
    ExynosCameraBufferManager::init();

    m_allocator   = NULL;
    m_numBufsHeap = 1;

    for (int bufIndex = 0; bufIndex < VIDEO_MAX_FRAME; bufIndex++) {
        for (int planeIndex = 0; planeIndex < EXYNOS_CAMERA_BUFFER_MAX_PLANES; planeIndex++) {
            m_heap[bufIndex][planeIndex] = NULL;
        }
    }
}

MHBExynosCameraBufferManager::~MHBExynosCameraBufferManager()
{
    ExynosCameraBufferManager::deinit();
}

status_t MHBExynosCameraBufferManager::m_setAllocator(void *allocator)
{
    m_allocator = (ExynosCameraMHBAllocator *)allocator;

    return NO_ERROR;
}

status_t MHBExynosCameraBufferManager::m_alloc(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    int planeCount = 0;

    if (m_allocator == NULL) {
        CLOGE("ERR(%s[%d]):m_allocator equals NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        planeCount = (m_hasMetaPlane ?
            m_buffer[bufIndex].planeCount-1 : m_buffer[bufIndex].planeCount);

        for (int planeIndex = 0; planeIndex < planeCount; planeIndex++) {
            if (m_allocator->alloc(
                    m_buffer[bufIndex].size[planeIndex],
                    &(m_buffer[bufIndex].fd[planeIndex]),
                    &(m_buffer[bufIndex].addr[planeIndex]),
                    m_numBufsHeap,
                    &(m_heap[bufIndex][planeIndex])) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_allocator->alloc(bufIndex=%d, planeIndex=%d, planeIndex=%d) failed",
                    __FUNCTION__, __LINE__, bufIndex, planeIndex, m_buffer[bufIndex].size[planeIndex]);
                return INVALID_OPERATION;
            }
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            printBufferInfo(__FUNCTION__, __LINE__, bufIndex, planeIndex);
            CLOGI("INFO(%s[%d]):[m_buffer[%d][%d].heap=%p]",
                __FUNCTION__, __LINE__, bufIndex, planeIndex, m_heap[bufIndex][planeIndex]);
#endif
        }

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            return INVALID_OPERATION;
        }
    }

    EXYNOS_CAMERA_BUFFER_OUT();

    return NO_ERROR;
}

status_t MHBExynosCameraBufferManager::m_free(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    int planeCount = 0;

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (isAvaliable(bufIndex) == false) {
            CLOGE("ERR(%s[%d]):buffer [bufIndex=%d] in InProcess state",
                __FUNCTION__, __LINE__, bufIndex);
            if (m_isDestructor == false) {
                ret = BAD_VALUE;
                continue;
            } else {
                CLOGE("ERR(%s[%d]):buffer [bufIndex=%d] in InProcess state, but try to forcedly free",
                    __FUNCTION__, __LINE__, bufIndex);
            }
        }

        planeCount = (m_hasMetaPlane ?
            m_buffer[bufIndex].planeCount-1 : m_buffer[bufIndex].planeCount);

        for (int planeIndex = 0; planeIndex < planeCount; planeIndex++) {
            if (m_allocator->free(
                    m_buffer[bufIndex].size[planeIndex],
                    &(m_buffer[bufIndex].fd[planeIndex]),
                    &(m_buffer[bufIndex].addr[planeIndex]),
                    &(m_heap[bufIndex][planeIndex])) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_defaultAllocator->free for Imagedata Plane failed",
                    __FUNCTION__, __LINE__);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
            m_heap[bufIndex][planeIndex] = 0;
        }

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t MHBExynosCameraBufferManager::m_increase(int increaseCount)
{
    CLOGD("DEBUG(%s[%d]):allocMode(%d) is invalid. Do nothing", __FUNCTION__, __LINE__, m_allocMode);
    return INVALID_OPERATION;
}

status_t MHBExynosCameraBufferManager::m_decrease(void)
{
    return INVALID_OPERATION;
}

status_t MHBExynosCameraBufferManager::m_putBuffer(int bufIndex)
{
    return NO_ERROR;
}

status_t MHBExynosCameraBufferManager::m_getBuffer(int *bufIndex)
{
    return NO_ERROR;
}

status_t MHBExynosCameraBufferManager::allocMulti()
{
    m_numBufsHeap = m_reqBufCount;
    m_reqBufCount = 1;

    return alloc();
}

status_t MHBExynosCameraBufferManager::getHeapMemory(
        int bufIndex,
        int planeIndex,
        camera_memory_t **heap)
{
    EXYNOS_CAMERA_BUFFER_IN();

    if (m_buffer[bufIndex].status.position != EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL) {
        CLOGE("ERR(%s[%d]):buffer position not in IN_HAL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (m_heap[bufIndex][planeIndex] == NULL) {
        CLOGE("ERR(%s[%d]):m_heap equals NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    *heap = m_heap[bufIndex][planeIndex];
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGI("INFO(%s[%d]):heap=%p", __FUNCTION__, __LINE__, *heap);
#endif

    EXYNOS_CAMERA_BUFFER_OUT();

    return NO_ERROR;
}

GrallocExynosCameraBufferManager::GrallocExynosCameraBufferManager()
{
    ExynosCameraBufferManager::init();

    m_allocator             = NULL;
    m_dequeuedBufCount      = 0;
    m_minUndequeuedBufCount = 0;
    m_bufStride             = 0;
    m_bufferCount = 0;

    for (int bufIndex = 0; bufIndex < VIDEO_MAX_FRAME; bufIndex++) {
        m_handle[bufIndex]         = NULL;
        m_handleIsLocked[bufIndex] = false;
    }
}

GrallocExynosCameraBufferManager::~GrallocExynosCameraBufferManager()
{
    ExynosCameraBufferManager::deinit();
}

status_t GrallocExynosCameraBufferManager::m_setAllocator(void *allocator)
{
    m_allocator = (ExynosCameraGrallocAllocator *)allocator;

    return NO_ERROR;
}

status_t GrallocExynosCameraBufferManager::m_alloc(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;

    ExynosCameraDurationTimer m_timer;
    long long    durationTime = 0;
    long long    durationTimeSum = 0;
    unsigned int estimatedBase = EXYNOS_CAMERA_BUFFER_GRALLOC_WARNING_TIME;
    unsigned int estimatedTime = 0;
    unsigned int bufferSize = 0;
    int planeIndexEnd   = 0;

    if (m_allocator == NULL) {
        CLOGE("ERR(%s[%d]):m_allocator equals NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (getBufferCount() == 0) {
        CLOGE("ERR(%s[%d]):m_reqBufCount(%d)", __FUNCTION__, __LINE__, m_reqBufCount);
        setBufferCount(m_reqBufCount);
    }

    m_minUndequeuedBufCount = m_allocator->getMinUndequeueBuffer();
    if (m_minUndequeuedBufCount < 0 ) {
        CLOGE("ERR(%s[%d]):m_minUndequeuedBufCount=%d..",
            __FUNCTION__, __LINE__, m_minUndequeuedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGI("INFO(%s[%d]):before dequeueBuffer m_reqBufCount=%d, m_minUndequeuedBufCount=%d",
        __FUNCTION__, __LINE__, m_reqBufCount, m_minUndequeuedBufCount);
#endif
    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        m_timer.start();
        if (m_allocator->alloc(
                &m_handle[bufIndex],
                m_buffer[bufIndex].fd,
                m_buffer[bufIndex].addr,
                &m_bufStride,
                &m_handleIsLocked[bufIndex]) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):alloc failed [bufIndex=%d]", __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }

        planeIndexEnd = m_buffer[bufIndex].planeCount;

        if (m_hasMetaPlane == true)
            planeIndexEnd--;

        bufferSize = 0;
        for (int planeIndex = 0; planeIndex < planeIndexEnd; planeIndex++)
            bufferSize = bufferSize + m_buffer[bufIndex].size[planeIndex];

        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        durationTimeSum += durationTime;
        CLOGD("DEBUG(%s[%d]):duration time(%5d msec):(type=%d, bufIndex=%d, size=%d)",
            __FUNCTION__, __LINE__, (int)durationTime, m_buffer[bufIndex].type, bufIndex, (int)bufferSize);

        estimatedTime = estimatedBase * bufferSize / EXYNOS_CAMERA_BUFFER_1MB;
        if (estimatedTime < durationTime) {
            CLOGW("WARN(%s[%d]):estimated time(%5d msec):(type=%d, bufIndex=%d, size=%d)",
                __FUNCTION__, __LINE__, (int)estimatedTime, m_buffer[bufIndex].type, bufIndex, (int)bufferSize);
        }

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
        CLOGD("DEBUG(%s[%d]):-- dump buffer status --", __FUNCTION__, __LINE__);
        dump();
#endif
        m_dequeuedBufCount++;

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bufIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }
    CLOGD("DEBUG(%s[%d]):Duration time of buffer allocation(%5d msec)", __FUNCTION__, __LINE__, (int)durationTimeSum);

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
        CLOGD("DEBUG(%s[%d]):-- dump buffer status --", __FUNCTION__, __LINE__);
        dump();
#endif
        if (m_allocator->cancelBuffer(m_handle[bufIndex]) != 0) {
            CLOGE("ERR(%s[%d]):could not free [bufIndex=%d]", __FUNCTION__, __LINE__, bufIndex);
            goto func_exit;
        }
        m_dequeuedBufCount--;
        m_handleIsLocked[bufIndex] = false;

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bufIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGI("INFO(%s[%d]):before exit m_alloc m_dequeuedBufCount=%d, m_minUndequeuedBufCount=%d",
        __FUNCTION__, __LINE__, m_dequeuedBufCount, m_minUndequeuedBufCount);
#endif

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t GrallocExynosCameraBufferManager::m_free(int bIndex, int eIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;

    CLOGD("DEBUG(%s[%d]):IN -- dump buffer status --", __FUNCTION__, __LINE__);
    dump();

    for (int bufIndex = bIndex; bufIndex < eIndex; bufIndex++) {
        if (m_handleIsLocked[bufIndex] == false) {
            CLOGD("DEBUG(%s[%d]):buffer [bufIndex=%d] already free", __FUNCTION__, __LINE__, bufIndex);
            continue;
        }

        if (m_allocator->free(m_handle[bufIndex], m_handleIsLocked[bufIndex]) != 0) {
            CLOGE("ERR(%s[%d]):could not free [bufIndex=%d]", __FUNCTION__, __LINE__, bufIndex);
            goto func_exit;
        }
        m_dequeuedBufCount--;
        m_handle[bufIndex] = NULL;
        m_handleIsLocked[bufIndex] = false;

        if (updateStatus(
                bufIndex,
                0,
                EXYNOS_CAMERA_BUFFER_POSITION_NONE,
                EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
                __FUNCTION__, __LINE__, bufIndex);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t GrallocExynosCameraBufferManager::m_increase(int increaseCount)
{
    CLOGD("DEBUG(%s[%d]):allocMode(%d) is invalid. Do nothing", __FUNCTION__, __LINE__, m_allocMode);
    return INVALID_OPERATION;
}

status_t GrallocExynosCameraBufferManager::m_decrease(void)
{
    return INVALID_OPERATION;
}

status_t GrallocExynosCameraBufferManager::m_putBuffer(int bufIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;

    if (m_handle[bufIndex] != NULL &&
        m_buffer[bufIndex].status.position == EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL) {
        if (m_allocator->enqueueBuffer(m_handle[bufIndex]) != 0) {
            CLOGE("ERR(%s[%d]):could not enqueue_buffer [bufIndex=%d]",
                __FUNCTION__, __LINE__, bufIndex);
            CLOGD("DEBUG(%s[%d]):dump buffer status", __FUNCTION__, __LINE__);
            dump();
            goto func_exit;
        }
        m_dequeuedBufCount--;
        m_handleIsLocked[bufIndex] = false;
    }
    m_buffer[bufIndex].status.position = EXYNOS_CAMERA_BUFFER_POSITION_IN_SERVICE;

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGD("DEBUG(%s[%d]):dump buffer status", __FUNCTION__, __LINE__);
    dump();
#endif

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t GrallocExynosCameraBufferManager::m_getBuffer(int *bufIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    buffer_handle_t *bufHandle = NULL;
    int  bufferFd[3] = {0};
    void *bufferAddr[3] = {NULL};

    int   stride = 0;
    int   bufferIndex = -1;

    const private_handle_t *priv_handle;
    bool  isExistedBuffer = false;
    bool  isLocked = false;

    m_minUndequeuedBufCount = m_allocator->getMinUndequeueBuffer();

    if (m_minUndequeuedBufCount < 0 ) {
        CLOGE("ERR(%s[%d]):m_minUndequeuedBufCount=%d..",
            __FUNCTION__, __LINE__, m_minUndequeuedBufCount);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

   if (m_dequeuedBufCount == m_reqBufCount - m_minUndequeuedBufCount) {
        CLOGI("INFO(%s[%d]):skip allocation... ", __FUNCTION__, __LINE__);
        CLOGI("INFO(%s[%d]):m_dequeuedBufCount(%d) == m_reqBufCount(%d) - m_minUndequeuedBufCount(%d)",
                __FUNCTION__, __LINE__, m_dequeuedBufCount, m_reqBufCount, m_minUndequeuedBufCount);
        CLOGD("DEBUG(%s[%d]):-- dump buffer status --", __FUNCTION__, __LINE__);
        dump();
        ret = INVALID_OPERATION;

        goto func_exit;
    }
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGD("DEBUG(%s[%d]):before dequeueBuffer() "
          "m_reqBufCount=%d, m_dequeuedBufCount=%d, m_minUndequeuedBufCount=%d",
        __FUNCTION__, __LINE__,
        m_reqBufCount, m_dequeuedBufCount, m_minUndequeuedBufCount);
#endif
    if (m_allocator->dequeueBuffer(
            &bufHandle,
            bufferFd,
            (char **)bufferAddr,
            &isLocked) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):dequeueBuffer failed", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    for (int index = 0; index < m_reqBufCount; index++) {
        if (m_buffer[index].addr[0] != bufferAddr[0]) {
            continue;
        } else {
            bufferIndex = index;
            isExistedBuffer = true;
#ifdef EXYNOS_CAMERA_BUFFER_TRACE
            CLOGI("INFO(%s[%d]):bufferIndex(%d) found!", __FUNCTION__, __LINE__, bufferIndex);
#endif
            break;
        }
    }

    if (isExistedBuffer == false) {
        CLOGI("INFO(%s[%d]):not existedBuffer!", __FUNCTION__, __LINE__);
        if (m_allocator->cancelBuffer(bufHandle) != 0) {
            CLOGE("ERR(%s[%d]):could not cancelBuffer [bufferIndex=%d]",
                __FUNCTION__, __LINE__, bufferIndex);
        }
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (bufferIndex < 0 || VIDEO_MAX_FRAME <= bufferIndex) {
        CLOGE("ERR(%s[%d]):abnormal value [bufferIndex=%d]",
            __FUNCTION__, __LINE__, bufferIndex);
        ret = BAD_VALUE;
        goto func_exit;
    }

    priv_handle = private_handle_t::dynamicCast(*bufHandle);

    m_buffer[bufferIndex].fd[0]   = priv_handle->fd;
    m_buffer[bufferIndex].fd[1]   = priv_handle->fd1;
    m_buffer[bufferIndex].addr[0] = (char *)bufferAddr[0];
    m_buffer[bufferIndex].addr[1] = (char *)bufferAddr[1];
    m_handleIsLocked[bufferIndex] = isLocked;

    *bufIndex = bufferIndex;
    m_handle[bufferIndex] = bufHandle;
    m_dequeuedBufCount++;
    m_buffer[bufferIndex].status.position   = EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL;
    m_buffer[bufferIndex].status.permission = EXYNOS_CAMERA_BUFFER_PERMISSION_AVAILABLE;

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGD("DEBUG(%s[%d]):-- dump buffer status --", __FUNCTION__, __LINE__);
    dump();
    CLOGI("INFO(%s[%d]):-- OUT -- m_dequeuedBufCount=%d, m_minUndequeuedBufCount=%d",
        __FUNCTION__, __LINE__, m_dequeuedBufCount, m_minUndequeuedBufCount);
#endif

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t GrallocExynosCameraBufferManager::cancelBuffer(int bufIndex)
{
    EXYNOS_CAMERA_BUFFER_IN();

    status_t ret = NO_ERROR;
    Mutex::Autolock lock(m_lock);

    List<int>::iterator r;
    bool found = false;

    if (bufIndex < 0 || m_reqBufCount <= bufIndex) {
        CLOGE("ERR(%s[%d]):buffer Index in out of bound [bufIndex=%d]",
            __FUNCTION__, __LINE__, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_handleIsLocked[bufIndex] == false) {
        CLOGD("DEBUG(%s[%d]):buffer [bufIndex=%d] already free", __FUNCTION__, __LINE__, bufIndex);
        return ret;
    }

    if (m_allocator->cancelBuffer(m_handle[bufIndex]) != 0) {
        CLOGE("ERR(%s[%d]):could not cancel buffer [bufIndex=%d]", __FUNCTION__, __LINE__, bufIndex);
        goto func_exit;
    }
    m_dequeuedBufCount--;
    m_handle[bufIndex] = NULL;
    m_handleIsLocked[bufIndex] = false;

    if (updateStatus(
            bufIndex,
            0,
            EXYNOS_CAMERA_BUFFER_POSITION_NONE,
            EXYNOS_CAMERA_BUFFER_PERMISSION_NONE) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setStatus failed [bIndex=%d, position=NONE, permission=NONE]",
            __FUNCTION__, __LINE__, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_availableBufferIndexQLock.lock();
    for (r = m_availableBufferIndexQ.begin(); r != m_availableBufferIndexQ.end(); r++) {
        if (bufIndex == *r) {
            found = true;
            break;
        }
    }

    if (found == true) {
        CLOGI("INFO(%s[%d]):bufIndex=%d is already in (available state)",
            __FUNCTION__, __LINE__, bufIndex);
        m_availableBufferIndexQLock.unlock();
        goto func_exit;
    }
    m_availableBufferIndexQ.push_back(m_buffer[bufIndex].index);
    m_availableBufferIndexQLock.unlock();

#ifdef EXYNOS_CAMERA_BUFFER_TRACE
    CLOGD("DEBUG(%s[%d]):-- dump buffer status --", __FUNCTION__, __LINE__);
    dump();
#endif

func_exit:

    EXYNOS_CAMERA_BUFFER_OUT();

    return ret;
}

status_t GrallocExynosCameraBufferManager::setBufferCount(int bufferCount)
{
    CLOGE("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    if (m_allocator->setBufferCount(bufferCount) != 0) {
        CLOGE("ERR(%s[%d]):", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    m_bufferCount = bufferCount;

    return NO_ERROR;

func_exit:
    return -1;
}

int GrallocExynosCameraBufferManager::getBufferCount(void)
{
    CLOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return m_bufferCount;
}

int GrallocExynosCameraBufferManager::getBufStride(void)
{
    ALOGI("INFO(%s):bufStride=%d", __FUNCTION__, m_bufStride);
    return m_bufStride;
}

void GrallocExynosCameraBufferManager::printBufferState(void)
{
    for (int i = 0; i < m_allocatedBufCount; i++) {
        CLOGI("INFO(%s[%d]):m_buffer[%d].fd[0]=%d, position=%d, permission=%d, lock=%d]",
            __FUNCTION__, __LINE__, i, m_buffer[i].fd[0],
            m_buffer[i].status.position, m_buffer[i].status.permission, m_handleIsLocked[i]);
    }

    return;
}

void GrallocExynosCameraBufferManager::printBufferState(int bufIndex, int planeIndex)
{
    CLOGI("INFO(%s[%d]):m_buffer[%d].fd[%d]=%d, .status.permission=%d, lock=%d]",
        __FUNCTION__, __LINE__, bufIndex, planeIndex, m_buffer[bufIndex].fd[planeIndex],
        m_buffer[bufIndex].status.permission, m_handleIsLocked[bufIndex]);

    return;
}
}
