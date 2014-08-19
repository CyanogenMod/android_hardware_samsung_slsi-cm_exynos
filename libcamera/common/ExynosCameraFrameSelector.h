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

#ifndef EXYNOS_CAMERA_BAYER_SELECTOR_H
#define EXYNOS_CAMERA_BAYER_SELECTOR_H

#include "ExynosCameraParameters.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraBufferManager.h"
#include "ExynosCameraList.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraFrame.h"

namespace android{
class ExynosCameraFrameSelector {
public:
    ExynosCameraFrameSelector (ExynosCameraParameters *param,
                            ExynosCameraBufferManager *bufMgr);
    ~ExynosCameraFrameSelector();
    status_t release(void);
    status_t manageFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc);
    ExynosCameraFrame* selectFrames(int count, int pipeID, bool isSrc, int tryCount);
    status_t clearList(int pipeID = -1 , bool isSrc = false);
    int getHoldCount(void) { return m_frameHoldList.getSizeOfProcessQ(); };
    status_t setFrameHoldCount(int32_t count);
    status_t cancelPicture(void);
    void setWaitTime(uint64_t waitTime);

private:
    status_t m_manageNormalFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc);
    status_t m_manageHdrFrameHoldList(ExynosCameraFrame *frame, int pipeID, bool isSrc);

    ExynosCameraFrame* m_selectNormalFrame(int pipeID, bool isSrc, int tryCount);
    ExynosCameraFrame* m_selectFlashFrame(int pipeID, bool isSrc, int tryCount);
    ExynosCameraFrame* m_selectFocusedFrame(int pipeID, bool isSrc, int tryCount);
    ExynosCameraFrame* m_selectHdrFrame(int pipeID, bool isSrc, int tryCount);
    status_t m_getBufferFromFrame(ExynosCameraFrame *frame, int pipeID, bool isSrc, ExynosCameraBuffer **outBuffer);
    status_t m_pushQ(ExynosCameraList<ExynosCameraFrame *> *list, ExynosCameraFrame* inframe, bool lockflag);
    status_t m_popQ(ExynosCameraList<ExynosCameraFrame *> *list, ExynosCameraFrame** outframe, bool unlockflag, int tryCount);
    status_t m_waitAndpopQ(ExynosCameraList<ExynosCameraFrame *> *list, ExynosCameraFrame** outframe, bool unlockflag, int tryCount);
    status_t m_frameComplete(ExynosCameraFrame *frame);
    status_t m_LockedFrameComplete(ExynosCameraFrame *frame);
    status_t m_clearList(ExynosCameraList<ExynosCameraFrame *> *list, int pipeID, bool isSrc);
    status_t m_release(ExynosCameraList<ExynosCameraFrame *> *list);

    bool m_isFrameMetaTypeShotExt(void);

private:
    ExynosCameraList<ExynosCameraFrame *> m_frameHoldList;
    ExynosCameraList<ExynosCameraFrame *> m_hdrFrameHoldList;

    ExynosCameraParameters *m_parameters;
    ExynosCameraBufferManager *m_bufMgr;
    ExynosCameraActivityControl *m_activityControl;

    int m_reprocessingCount;
    ExynosCameraBuffer m_selectedBuffer;

    mutable Mutex m_listLock;
    int32_t m_frameHoldCount;
    bool isCanceled;
};
}

#endif

