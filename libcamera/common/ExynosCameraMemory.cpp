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

#define LOG_TAG "ExynosCameraMemoryAllocator"
#include "ExynosCameraMemory.h"

namespace android {


gralloc_module_t const *ExynosCameraGrallocAllocator::m_grallocHal;

ExynosCameraIonAllocator::ExynosCameraIonAllocator()
{
    m_ionClient   = 0;
    m_ionAlign    = 0;
    m_ionHeapMask = 0;
    m_ionFlags    = 0;
}

ExynosCameraIonAllocator::~ExynosCameraIonAllocator()
{
    ion_client_destroy(m_ionClient);
}

status_t ExynosCameraIonAllocator::init(bool isCached)
{
    status_t ret = NO_ERROR;

    if (m_ionClient == 0) {
        m_ionClient = ion_client_create();

        if (m_ionClient < 0) {
            ALOGE("ERR(%s):ion_client_create failed", __FUNCTION__);
            ret = BAD_VALUE;
            goto func_exit;
        }
    }

    m_ionAlign    = 0;
    m_ionHeapMask = ION_HEAP_SYSTEM_MASK;
    m_ionFlags    = (isCached == true ?
        (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC | ION_FLAG_PRESERVE_KMAP) : 0);

func_exit:

    return ret;
}

status_t ExynosCameraIonAllocator::alloc(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient == 0) {
        ALOGE("ERR(%s):allocator is not yet created", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        ALOGE("ERR(%s):size equals zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionFd = ion_alloc(m_ionClient, size, m_ionAlign, m_ionHeapMask, m_ionFlags);

    if (ionFd <= 0) {
        ALOGE("ERR(%s):ion_alloc(fd=%d) failed", __FUNCTION__, ionFd);
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (map(size, ionFd, &ionAddr) != NO_ERROR) {
            ALOGE("ERR(%s):map failed", __FUNCTION__);
        }
    }

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraIonAllocator::alloc(
        int size,
        int *fd,
        char **addr,
        int  mask,
        int  flags,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = 0;
    char *ionAddr = NULL;

    if (m_ionClient == 0) {
        ALOGE("ERR(%s):allocator is not yet created", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (size == 0) {
        ALOGE("ERR(%s):size equals zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionFd = ion_alloc(m_ionClient, size, m_ionAlign, mask, flags);

    if (ionFd <= 0) {
        ALOGE("ERR(%s):ion_alloc(fd=%d) failed", __FUNCTION__, ionFd);
        ionFd = -1;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (map(size, ionFd, &ionAddr) != NO_ERROR) {
            ALOGE("ERR(%s):map failed", __FUNCTION__);
        }
    }

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraIonAllocator::free(
        int size,
        int *fd,
        char **addr,
        bool mapNeeded)
{
    status_t ret = NO_ERROR;
    int ionFd = *fd;
    char *ionAddr = *addr;

    if (ionFd < 0) {
        ALOGE("ERR(%s):ion_fd is lower than zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (mapNeeded == true) {
        if (ionAddr == NULL) {
            ALOGE("ERR(%s):ion_addr equals NULL", __FUNCTION__);
            ret = BAD_VALUE;
            goto func_exit;
        }

        if (ion_unmap(ionAddr, size) < 0) {
            ALOGE("ERR(%s):ion_unmap failed", __FUNCTION__);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    ion_free(ionFd);

    ionFd   = -1;
    ionAddr = NULL;

func_exit:

    *fd   = ionFd;
    *addr = ionAddr;

    return ret;
}

status_t ExynosCameraIonAllocator::map(int size, int fd, char **addr)
{
    status_t ret = NO_ERROR;
    char *ionAddr = NULL;

    if (size == 0) {
        ALOGE("ERR(%s):size equals zero", __FUNCTION__);
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (fd <= 0) {
        ALOGE("ERR(%s):fd=%d failed", __FUNCTION__, size);
        ret = BAD_VALUE;
        goto func_exit;
    }

    ionAddr = (char *)ion_map(fd, size, 0);

    if (ionAddr == (char *)MAP_FAILED || ionAddr == NULL) {
        ALOGE("ERR(%s):ion_map(size=%d) failed", __FUNCTION__, size);
        ion_free(fd);
        ionAddr = NULL;
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_exit:

    *addr = ionAddr;

    return ret;
}

void ExynosCameraIonAllocator::setIonHeapMask(int mask)
{
    m_ionHeapMask |= mask;
}

void ExynosCameraIonAllocator::setIonFlags(int flags)
{
    m_ionFlags |= flags;
}

ExynosCameraMHBAllocator::ExynosCameraMHBAllocator()
{
    m_allocator = NULL;
}

ExynosCameraMHBAllocator::~ExynosCameraMHBAllocator()
{
}

status_t ExynosCameraMHBAllocator::init(camera_request_memory allocator)
{
    m_allocator = allocator;

    return NO_ERROR;
}

status_t ExynosCameraMHBAllocator::alloc(
        int size,
        int *fd,
        char **addr,
        int numBufs,
        camera_memory_t **heap)
{
    status_t ret = NO_ERROR;
    camera_memory_t *heap_ptr = NULL;
    int  heapFd    = 0;
    char *heapAddr = NULL;

    if (m_allocator == NULL) {
        ALOGE("ERR(%s):m_allocator equals NULL", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    heap_ptr = m_allocator(-1, size, numBufs, &heapFd);

    if (heap_ptr == NULL || heapFd < 0) {
        ALOGE("ERR(%s):heap_alloc(size=%d) failed", __FUNCTION__, size);
        heap_ptr = NULL;
        heapFd = -1;
        ret = BAD_VALUE;
        goto func_exit;
    }

    heapAddr = (char *)heap_ptr->data;

func_exit:

    *fd   = heapFd;
    *addr = heapAddr;
    *heap = heap_ptr;

#ifdef EXYNOS_CAMERA_MEMORY_TRACE
    ALOGI("INFO(%s[%d]):[heap.fd=%d] .addr=%p .heap=%p]",
        __FUNCTION__, __LINE__, heapFd, heapAddr, heap_ptr);
#endif

    return ret;
}

status_t ExynosCameraMHBAllocator::free(
        int size,
        int *fd,
        char **addr,
        camera_memory_t **heap)
{
    status_t ret = NO_ERROR;
    camera_memory_t *heap_ptr = *heap;
    int heapFd     = *fd;
    char *heapAddr = *addr;

    if (heap_ptr == NULL) {
        ALOGE("ERR(%s):heap_ptr equals NULL", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    heap_ptr->release(heap_ptr);
    heapAddr = NULL;
    heapFd   = -1;
    heap_ptr = 0;

func_exit:

    *fd   = heapFd;
    *addr = heapAddr;
    *heap = heap_ptr;

    return ret;
}

ExynosCameraGrallocAllocator::ExynosCameraGrallocAllocator()
{
    m_allocator = NULL;

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&m_grallocHal))
        ALOGE("ERR(%s):Loading gralloc HAL failed", __FUNCTION__);
}

ExynosCameraGrallocAllocator::~ExynosCameraGrallocAllocator()
{
}

status_t ExynosCameraGrallocAllocator::init(
        preview_stream_ops *allocator,
        int bufCount)
{
    status_t ret = NO_ERROR;

    m_allocator = allocator;

    if (setBufferCount(bufCount) != 0) {
        ALOGE("ERR(%s):setBufferCount failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_allocator->set_usage(m_allocator, GRALLOC_SET_USAGE_FOR_CAMERA) != 0) {
        ALOGE("ERR(%s):set_usage failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_grallocHal == NULL) {
        if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&m_grallocHal))
            ALOGE("ERR(%s):Loading gralloc HAL failed", __FUNCTION__);
    }

func_exit:

    return ret;
}

status_t ExynosCameraGrallocAllocator::alloc(
        buffer_handle_t **bufHandle,
        int fd[],
        char *addr[],
        int  *bufStride,
        bool *isLocked)
{
    status_t ret = NO_ERROR;
    int   width  = 0;
    int   height = 0;
    void  *grallocAddr[3] = {NULL};
    int   grallocFd[3] = {0};
    const private_handle_t *priv_handle = NULL;
    int   retryCount = 5;

    for (int retryCount = 5; retryCount > 0; retryCount--) {
#ifdef EXYNOS_CAMERA_MEMORY_TRACE
        ALOGI("INFO(%s[%d]):dequeue_buffer retryCount=%d",
            __FUNCTION__, __LINE__, retryCount);
#endif
        if (m_allocator->dequeue_buffer(m_allocator, bufHandle, bufStride) != 0) {
            ALOGE("ERR(%s):dequeue_buffer failed", __FUNCTION__);
            continue;
        }

        if (bufHandle == NULL) {
            ALOGE("ERR(%s):bufHandle == NULL failed, retry(%d)", __FUNCTION__, retryCount);
            continue;
        }

        if (m_allocator->lock_buffer(m_allocator, *bufHandle) != 0)
            ALOGE("ERR(%s):lock_buffer failed, but go on to the next step ...", __FUNCTION__);

        if (*isLocked == false) {
            if (m_grallocHal->lock(
                        m_grallocHal,
                        **bufHandle,
                        GRALLOC_LOCK_FOR_CAMERA,
                        0, 0, /* left, top */
                        width, height,
                        grallocAddr) != 0) {
                ALOGE("ERR(%s):grallocHal->lock failed.. retry", __FUNCTION__);

                if (m_allocator->cancel_buffer(m_allocator, *bufHandle) != 0)
                    ALOGE("ERR(%s):cancel_buffer failed", __FUNCTION__);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
            break;
        }
    }

    if (bufHandle == NULL) {
        ALOGE("ERR(%s):bufHandle == NULL failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (*bufHandle == NULL) {
        ALOGE("@@@@ERR(%s):*bufHandle == NULL failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    priv_handle = private_handle_t::dynamicCast(**bufHandle);

    grallocFd[0] = priv_handle->fd;
    grallocFd[1] = priv_handle->fd1;
    *isLocked    = true;

func_exit:

    fd[0] = grallocFd[0];
    fd[1] = grallocFd[1];
    addr[0] = (char *)grallocAddr[0];
    addr[1] = (char *)grallocAddr[1];

    return ret;
}

status_t ExynosCameraGrallocAllocator::free(buffer_handle_t *bufHandle, bool isLocked)
{
    status_t ret = NO_ERROR;

    if (bufHandle == NULL) {
        ALOGE("ERR(%s):bufHandle equals NULL", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (isLocked == true) {
        if (m_grallocHal->unlock(m_grallocHal, *bufHandle) != 0) {
            ALOGE("ERR(%s):grallocHal->unlock failed", __FUNCTION__);
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    if (m_allocator->cancel_buffer(m_allocator, bufHandle) != 0) {
        ALOGE("ERR(%s):cancel_buffer failed", __FUNCTION__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCameraGrallocAllocator::setBufferCount(int bufCount)
{
    status_t ret = NO_ERROR;

    if (m_allocator->set_buffer_count(m_allocator, bufCount) != 0) {
        ALOGE("ERR(%s):set_buffer_count failed [bufCount=%d]", __FUNCTION__, bufCount);
        ret = INVALID_OPERATION;
    }

    return ret;
}
status_t ExynosCameraGrallocAllocator::setBuffersGeometry(
        int width,
        int height,
        int halPixelFormat)
{
    status_t ret = NO_ERROR;

    if (m_allocator->set_buffers_geometry(
                    m_allocator,
                    width, height,
                    halPixelFormat) != 0) {
        ALOGE("ERR(%s):set_buffers_geometry failed", __FUNCTION__);
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraGrallocAllocator::getAllocator(preview_stream_ops **allocator)
{
    *allocator = m_allocator;

    return NO_ERROR;
}

int ExynosCameraGrallocAllocator::getMinUndequeueBuffer()
{
    int minUndeqBufCount = 0;
    if (m_allocator->get_min_undequeued_buffer_count(m_allocator, &minUndeqBufCount) != 0) {
        ALOGE("ERR(%s):enqueue_buffer failed", __FUNCTION__);
        return INVALID_OPERATION;
    }

    return minUndeqBufCount < 2 ? (minUndeqBufCount + NUM_PREVIEW_BUFFERS_MARGIN) : minUndeqBufCount;
}

status_t ExynosCameraGrallocAllocator::dequeueBuffer(
        buffer_handle_t **bufHandle,
        int fd[],
        char *addr[],
        bool *isLocked)
{
    int bufStride = 0;

    if (alloc(bufHandle, fd, addr, &bufStride, isLocked) != 0) {
        ALOGE("ERR(%s):alloc failed", __FUNCTION__);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraGrallocAllocator::enqueueBuffer(buffer_handle_t *handle)
{
    if (m_allocator->enqueue_buffer(m_allocator, handle) != 0) {
        ALOGE("ERR(%s):enqueue_buffer failed", __FUNCTION__);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraGrallocAllocator::cancelBuffer(buffer_handle_t *handle)
{
    if (m_grallocHal->unlock(m_grallocHal, *handle) != 0) {
        ALOGE("ERR(%s):grallocHal->unlock failed", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (m_allocator->cancel_buffer(m_allocator, handle) != 0) {
        ALOGE("ERR(%s):cancel_buffer failed", __FUNCTION__);
        return INVALID_OPERATION;
    }
    return NO_ERROR;
}
}
