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

#ifndef EXYNOS_CAMERA_THREAD_H
#define EXYNOS_CAMERA_THREAD_H

#include <utils/threads.h>

using namespace android;

template<typename T>
class ExynosCameraThread : public Thread {

typedef bool (T::*thread_loop)(void);
public:
    ExynosCameraThread(
        T *hw,
        thread_loop loop,
        const char *name,
        int32_t priority = PRIORITY_DEFAULT)
    {
        m_hardware   = hw;
        m_threadLoop = loop;
        m_name       = name;
        m_priority   = priority;
    }

    virtual status_t run(int32_t priority = PRIORITY_DEFAULT, size_t stack = 0) {

        ALOGV("DEBUG(%s):Thread(%s) start running", __FUNCTION__, m_name);
        if (m_priority != priority)
            m_priority = priority;

        return Thread::run(m_name, m_priority, stack);
    }

private:
    virtual bool threadLoop() {
        return (m_hardware->*m_threadLoop)();
    }

private:
    T           *m_hardware;
    const char  *m_name;
    thread_loop m_threadLoop;
    int32_t     m_priority;
};

#endif
