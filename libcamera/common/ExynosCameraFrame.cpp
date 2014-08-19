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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraFrame"
#include <cutils/log.h>

#include "ExynosCameraFrame.h"

namespace android {

ExynosCameraFrame::ExynosCameraFrame(
        ExynosCameraParameters *obj_param,
        uint32_t frameCount)
{
    m_parameters = obj_param;
    m_frameCount = frameCount;
    m_numRequestPipe = 0;
    m_numCompletePipe = 0;
    m_frameState = FRAME_STATE_READY;
    m_frameLocked = false;
    m_metaDataEnable = false;
    m_zoom = 0;
    memset(&m_metaData, 0x0, sizeof(struct camera2_shot_ext));
    m_jpegSize = 0;
    m_request3AP = false;
    m_request3AC = false;
    m_requestISP = false;
    m_requestSCC = false;
    m_requestDIS = false;
    m_requestSCP = false;
    for (int i = 0; i < PERFRAME_NODE_GROUP_MAX; i++)
        memset(&m_node_gorup[i], 0x0, sizeof(struct camera2_node_group));
}

ExynosCameraFrame::~ExynosCameraFrame()
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    ExynosCameraFrameEntity *tmpEntity = NULL;

    while (!m_linkageList.empty()) {
        r = m_linkageList.begin()++;
        if (*r) {
            curEntity = *r;

            while (curEntity != NULL) {
                tmpEntity = curEntity->getNextEntity();
                ALOGV("DEBUG(%s[%d])", __FUNCTION__, curEntity->pipeId);

                delete curEntity;
                curEntity = tmpEntity;
            }

        }
        m_linkageList.erase(r);
    }
}

status_t ExynosCameraFrame::addSiblingEntity(
        ExynosCameraFrameEntity *curEntity,
        ExynosCameraFrameEntity *newEntity)
{
    m_linkageList.push_back(newEntity);

    return NO_ERROR;
}

status_t ExynosCameraFrame::addChildEntity(
        ExynosCameraFrameEntity *parentEntity,
        ExynosCameraFrameEntity *newEntity)
{
    int ret = 0;

    if (parentEntity == NULL) {
        ALOGE("ERR(%s):parentEntity is NULL", __FUNCTION__);
        return BAD_VALUE;
    }

    /* TODO: This is not suit in case of newEntity->next != NULL */
    ExynosCameraFrameEntity *tmpEntity;

    tmpEntity = parentEntity->getNextEntity();
    ret = parentEntity->setNextEntity(newEntity);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):setNextEntity fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    newEntity->setNextEntity(tmpEntity);

    return NO_ERROR;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFirstEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *firstEntity = NULL;

    if (m_linkageList.empty()) {
        ALOGE("ERR(%s):m_linkageList is empty", __FUNCTION__);
        firstEntity = NULL;
        return firstEntity;
    }

    r = m_linkageList.begin()++;
    m_currentEntity = r;
    firstEntity = *r;

    return firstEntity;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getNextEntity(void)
{
    ExynosCameraFrameEntity *nextEntity = NULL;

    if (m_currentEntity == m_linkageList.end()) {
        ALOGW("WARN(%s):last entity", __FUNCTION__);
        return nextEntity;
    }

    m_currentEntity++;
    nextEntity = *m_currentEntity;

    return nextEntity;
}

ExynosCameraFrameEntity *ExynosCameraFrame::getChildEntity(ExynosCameraFrameEntity *parentEntity)
{
    ExynosCameraFrameEntity *childEntity = NULL;

    if (parentEntity == NULL) {
        ALOGE("ERR(%s):parentEntity is NULL", __FUNCTION__);
        return childEntity;
    }

    childEntity = parentEntity->getNextEntity();

    return childEntity;
}


ExynosCameraFrameEntity *ExynosCameraFrame::searchEntityByPipeId(uint32_t pipeId)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    int listSize = 0;

    if (m_linkageList.empty()) {
        ALOGE("ERR(%s):m_linkageList is empty", __FUNCTION__);
        return NULL;
    }

    listSize = m_linkageList.size();
    r = m_linkageList.begin();

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) {
            ALOGE("ERR(%s):curEntity is NULL, index(%d), linkageList size(%d)",
                __FUNCTION__, i, listSize);
            return NULL;
        }

        while (curEntity != NULL) {
            if (curEntity->getPipeId() == pipeId)
                return curEntity;
            curEntity = curEntity->getNextEntity();
        }
        r++;
    }

    ALOGD("DEBUG(%s):Cannot find matched entity, frameCount(%d), pipeId(%d)", __FUNCTION__, getFrameCount(), pipeId);

    return NULL;
}

status_t ExynosCameraFrame::setSrcBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer srcBuf)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->setSrcBuf(srcBuf);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not set src buffer, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::setDstBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer dstBuf)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstBuf(dstBuf);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not set dst buffer, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    /* TODO: set buffer to child node's source */
    entity = entity->getNextEntity();
    if (entity != NULL) {
        ret = entity->setSrcBuf(dstBuf);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):Could not set dst buffer, ret(%d)", __FUNCTION__, __LINE__, ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::getSrcBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer *srcBuf)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->getSrcBuf(srcBuf);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not get src buffer, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDstBuffer(uint32_t pipeId,
                                         ExynosCameraBuffer *dstBuf)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->getDstBuf(dstBuf);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not get dst buffer, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::setSrcRect(uint32_t pipeId, ExynosRect srcRect)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->setSrcRect(srcRect);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not set src rect, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::setDstRect(uint32_t pipeId, ExynosRect dstRect)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstRect(dstRect);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not set dst rect, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    /* TODO: set buffer to child node's source */
    entity = entity->getNextEntity();
    if (entity != NULL) {
        ret = entity->setSrcRect(dstRect);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):Could not set dst rect, ret(%d)", __FUNCTION__, __LINE__, ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::getSrcRect(uint32_t pipeId, ExynosRect *srcRect)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->getSrcRect(srcRect);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not get src rect, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDstRect(uint32_t pipeId, ExynosRect *dstRect)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->getDstRect(dstRect);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Could not get dst rect, ret(%d)", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::getSrcBufferState(uint32_t pipeId,
                                         entity_buffer_state_t *state)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    *state = entity->getSrcBufState();

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDstBufferState(uint32_t pipeId,
                                         entity_buffer_state_t *state)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    *state = entity->getDstBufState();

    return NO_ERROR;
}

status_t ExynosCameraFrame::setSrcBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->setSrcBufState(state);

    return ret;
}

status_t ExynosCameraFrame::setDstBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    ret = entity->setDstBufState(state);

    return ret;
}

status_t ExynosCameraFrame::ensureSrcBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state)
{
    int ret = 0;
    int retry = 0;
    entity_buffer_state_t curState;

    do {
        ret = getSrcBufferState(pipeId, &curState);
        if (ret < 0)
            continue;

        if (state == curState) {
            ret = OK;
            break;
        } else {
            ret = BAD_VALUE;
            usleep(100);
        }

        retry++;
        if (retry == 10)
            ret = TIMED_OUT;
    } while (ret != OK && retry < 100);

    ALOGV("DEBUG(%s[%d]): retry count %d", __FUNCTION__, __LINE__, retry);

    return ret;
}

status_t ExynosCameraFrame::ensureDstBufferState(uint32_t pipeId,
                                         entity_buffer_state_t state)
{
    int ret = 0;
    int retry = 0;
    entity_buffer_state_t curState;

    do {
        ret = getDstBufferState(pipeId, &curState);
        if (ret < 0)
            continue;

        if (state == curState) {
            ret = OK;
            break;
        } else {
            ret = BAD_VALUE;
            usleep(100);
        }

        retry++;
        if (retry == 10)
            ret = TIMED_OUT;
    } while (ret != OK && retry < 100);

    ALOGV("DEBUG(%s[%d]): retry count %d", __FUNCTION__, __LINE__, retry);

    return ret;
}

status_t ExynosCameraFrame::setEntityState(uint32_t pipeId,
                                           entity_state_t state)
{
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    if (entity->getEntityState() == ENTITY_STATE_COMPLETE &&
        state != ENTITY_STATE_REWORK) {
        return NO_ERROR;
    }

    if (state == ENTITY_STATE_COMPLETE) {
        m_numCompletePipe++;
        if (m_numCompletePipe >= m_numRequestPipe)
            setFrameState(FRAME_STATE_COMPLETE);
    }

    entity->setEntityState(state);

    return NO_ERROR;
}

status_t ExynosCameraFrame::getEntityState(uint32_t pipeId,
                                           entity_state_t *state)
{
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    *state = entity->getEntityState();
    return NO_ERROR;
}

status_t ExynosCameraFrame::getEntityBufferType(uint32_t pipeId,
                                                entity_buffer_type_t *type)
{
    ExynosCameraFrameEntity *entity = searchEntityByPipeId(pipeId);
    if (entity == NULL) {
        ALOGE("ERR(%s[%d]):Could not find entity, pipeID(%d)", __FUNCTION__, __LINE__, pipeId);
        return BAD_VALUE;
    }

    *type = entity->getBufType();
    return NO_ERROR;
}

uint32_t ExynosCameraFrame::getFrameCount(void)
{
    return m_frameCount;
}

status_t ExynosCameraFrame::setNumRequestPipe(uint32_t num)
{
    m_numRequestPipe = num;
    return NO_ERROR;
}

uint32_t ExynosCameraFrame::getNumRequestPipe(void)
{
    return m_numRequestPipe;
}

bool ExynosCameraFrame::isComplete(void)
{
    return checkFrameState(FRAME_STATE_COMPLETE);
}

ExynosCameraFrameEntity *ExynosCameraFrame::getFrameDoneEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;

    if (m_linkageList.empty()) {
        ALOGE("ERR(%s):m_linkageList is empty", __FUNCTION__);
        return NULL;
    }

    r = m_linkageList.begin()++;
    curEntity = *r;

    while (r != m_linkageList.end()) {
        if (curEntity != NULL && curEntity->getEntityState() == ENTITY_STATE_FRAME_DONE) {
            if (curEntity->getNextEntity() != NULL) {
                curEntity = curEntity->getNextEntity();
                continue;
            }
            return curEntity;
        }
        r++;
        curEntity = *r;
    }

    return NULL;
}

status_t ExynosCameraFrame::skipFrame(void)
{
//    Mutex::Autolock lock(m_frameStateLock);
    m_frameState = FRAME_STATE_SKIPPED;

    return NO_ERROR;
}

void ExynosCameraFrame::setFrameState(frame_status_t state)
{
//    Mutex::Autolock lock(m_frameStateLock);

    /* TODO: We need state machine */
    if (state > FRAME_STATE_INVALID)
        m_frameState = FRAME_STATE_INVALID;
    else
        m_frameState = state;
}

frame_status_t ExynosCameraFrame::getFrameState(void)
{
//    Mutex::Autolock lock(m_frameStateLock);
    return m_frameState;
}

bool ExynosCameraFrame::checkFrameState(frame_status_t state)
{
//    Mutex::Autolock lock(m_frameStateLock);

    return (m_frameState == state) ? true : false;
}

void ExynosCameraFrame::printEntity(void)
{
    List<ExynosCameraFrameEntity *>::iterator r;
    ExynosCameraFrameEntity *curEntity = NULL;
    int listSize = 0;

    if (m_linkageList.empty()) {
        ALOGE("ERR(%s):m_linkageList is empty", __FUNCTION__);
        return;
    }

    listSize = m_linkageList.size();
    r = m_linkageList.begin();

    ALOGD("DEBUG(%s): FrameCount(%d), request(%d), complete(%d)", __FUNCTION__, getFrameCount(), m_numRequestPipe, m_numCompletePipe);

    for (int i = 0; i < listSize; i++) {
        curEntity = *r;
        if (curEntity == NULL) {
            ALOGE("ERR(%s):curEntity is NULL, index(%d)", __FUNCTION__, i);
            return;
        }

        ALOGD("DEBUG(%s):sibling id(%d), state(%d)",
            __FUNCTION__, curEntity->getPipeId(), curEntity->getEntityState());

        while (curEntity != NULL) {
            ALOGD("DEBUG(%s):----- Child id(%d), state(%d)",
                __FUNCTION__, curEntity->getPipeId(), curEntity->getEntityState());
            curEntity = curEntity->getNextEntity();
        }
        r++;
    }

    return;
}

void ExynosCameraFrame::dump(void)
{
}

void ExynosCameraFrame::frameLock(void)
{
    m_frameLocked = true;
}

void ExynosCameraFrame::frameUnlock(void)
{
    m_frameLocked = false;
}

bool ExynosCameraFrame::getFrameLockState(void)
{
    return m_frameLocked;
}

status_t ExynosCameraFrame::initMetaData(struct camera2_shot_ext *shot)
{
    int ret = 0;

    if (shot != NULL) {
        ALOGV("DEBUG(%s[%d]): initialize shot_ext", __FUNCTION__, __LINE__);
        memcpy(&m_metaData, shot, sizeof(struct camera2_shot_ext));
    }

    ret = m_parameters->duplicateCtrlMetadata(&m_metaData);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):duplicate Ctrl metadata fail", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrame::getMetaData(struct camera2_shot_ext *shot)
{
    if (shot == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(shot, &m_metaData, sizeof(struct camera2_shot_ext));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeDynamicMeta(struct camera2_shot_ext *shot)
{
    if (shot == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(&m_metaData.shot.dm, &shot->shot.dm, sizeof(struct camera2_dm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeUserDynamicMeta(struct camera2_shot_ext *shot)
{
    if (shot == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(&m_metaData.shot.udm, &shot->shot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getDynamicMeta(struct camera2_shot_ext *shot)
{
    if (shot == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(&shot->shot.dm, &m_metaData.shot.dm, sizeof(struct camera2_dm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getUserDynamicMeta(struct camera2_shot_ext *shot)
{
    if (shot == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(&shot->shot.udm, &m_metaData.shot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraFrame::setMetaDataEnable(bool flag)
{
    m_metaDataEnable = flag;
    return NO_ERROR;
}

bool ExynosCameraFrame::getMetaDataEnable()
{
    long count = 0;
    while (count < DM_WAITING_COUNT) {
        if (m_metaDataEnable == true) {
            ALOGD("DEBUG(%s[%d]): metadata enable count(%ld) ", __FUNCTION__, __LINE__, count);
            break;
        }

        count++;
        usleep(WAITING_TIME);
    }

    return m_metaDataEnable;
}

status_t ExynosCameraFrame::getNodeGroupInfo(struct camera2_node_group *node_group, int index)
{
    if (node_group == NULL) {
        ALOGE("ERR(%s[%d]): node_group is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (index >= PERFRAME_NODE_GROUP_MAX) {
        ALOGE("ERR(%s[%d]): index is bigger than PERFRAME_NODE_GROUP_MAX", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(node_group, &m_node_gorup[index], sizeof(struct camera2_node_group));

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeNodeGroupInfo(struct camera2_node_group *node_group, int index)
{
    if (node_group == NULL) {
        ALOGE("ERR(%s[%d]): node_group is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (index >= PERFRAME_NODE_GROUP_MAX) {
        ALOGE("ERR(%s[%d]): index is bigger than PERFRAME_NODE_GROUP_MAX", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(&m_node_gorup[index], node_group, sizeof(struct camera2_node_group));

    return NO_ERROR;
}

status_t ExynosCameraFrame::getNodeGroupInfo(struct camera2_node_group *node_group, int index, int *zoom)
{
    getNodeGroupInfo(node_group, index);
    *zoom = m_zoom;

    return NO_ERROR;
}

status_t ExynosCameraFrame::storeNodeGroupInfo(struct camera2_node_group *node_group, int index, int zoom)
{
    storeNodeGroupInfo(node_group, index);
    m_zoom = zoom;

    return NO_ERROR;
}

void ExynosCameraFrame::dumpNodeGroupInfo(const char *name)
{
    for (int i = 0; i < PERFRAME_NODE_GROUP_MAX; i ++) {
        if (name != NULL)
            ALOGD("INFO(%s[%d]):[%d] (%s)++++++++++++++++++++ frameCount(%d)", __FUNCTION__, __LINE__, i, name, m_frameCount);

            ALOGI("Leader (%d, %d, %d, %d)(%d, %d, %d, %d)(%d %d)",
                m_node_gorup[i].leader.input.cropRegion[0],
                m_node_gorup[i].leader.input.cropRegion[1],
                m_node_gorup[i].leader.input.cropRegion[2],
                m_node_gorup[i].leader.input.cropRegion[3],
                m_node_gorup[i].leader.output.cropRegion[0],
                m_node_gorup[i].leader.output.cropRegion[1],
                m_node_gorup[i].leader.output.cropRegion[2],
                m_node_gorup[i].leader.output.cropRegion[3],
                m_node_gorup[i].leader.request,
                m_node_gorup[i].leader.vid);

        for (int j = 0; j < CAPTURE_NODE_MAX; j ++) {
            ALOGI("Capture[%d] (%d, %d, %d, %d)(%d, %d, %d, %d)(%d, %d)", j,
                m_node_gorup[i].capture[j].input.cropRegion[0],
                m_node_gorup[i].capture[j].input.cropRegion[1],
                m_node_gorup[i].capture[j].input.cropRegion[2],
                m_node_gorup[i].capture[j].input.cropRegion[3],
                m_node_gorup[i].capture[j].output.cropRegion[0],
                m_node_gorup[i].capture[j].output.cropRegion[1],
                m_node_gorup[i].capture[j].output.cropRegion[2],
                m_node_gorup[i].capture[j].output.cropRegion[3],
                m_node_gorup[i].capture[j].request,
                m_node_gorup[i].capture[j].vid);
        }

        if (name != NULL)
            ALOGD("INFO(%s[%d]):[%d] (%s)------------------------ ", __FUNCTION__, __LINE__, i, name);
    }

    return;
}

void ExynosCameraFrame::setJpegSize(int size)
{
    m_jpegSize = size;
}

int ExynosCameraFrame::getJpegSize(void)
{
    return m_jpegSize;
}

int64_t ExynosCameraFrame::getTimeStamp(void)
{
    return (int64_t)getMetaDmSensorTimeStamp(&m_metaData);
}

void ExynosCameraFrame::getFpsRange(uint32_t *min, uint32_t *max)
{
    getMetaCtlAeTargetFpsRange(&m_metaData, min, max);
}

void ExynosCameraFrame::setRequest(bool tap,
                                   bool tac,
                                   bool isp,
                                   bool scc,
                                   bool dis,
                                   bool scp)
{
    m_request3AP = tap;
    m_request3AC = tac;
    m_requestISP = isp;
    m_requestSCC = scc;
    m_requestDIS = dis;
    m_requestSCP = scp;
}

void ExynosCameraFrame::setRequest(uint32_t pipeId, bool val)
{
    switch (pipeId) {
    case PIPE_3AC:
    case PIPE_3AC_FRONT:
        m_request3AC = val;
        break;
    case PIPE_SCC:
    case PIPE_SCC_FRONT:
        m_requestSCC = val;
        break;
    case PIPE_SCP:
    case PIPE_SCP_FRONT:
        m_requestSCP = val;
        break;
    default:
        ALOGW("WRN(%s[%d]): unknown pipeId", __FUNCTION__, __LINE__);
        break;
    }
}

bool ExynosCameraFrame::getRequest(uint32_t pipeId)
{
    bool request = false;

    switch (pipeId) {
    case PIPE_3AC:
    case PIPE_3AC_FRONT:
        request = m_request3AC;
        break;
    case PIPE_SCC:
    case PIPE_SCC_FRONT:
    case PIPE_SCC_REPROCESSING:
        request = m_requestSCC;
        break;
    case PIPE_SCP:
    case PIPE_SCP_FRONT:
        request = m_requestSCP;
        break;
    default:
        ALOGW("WRN(%s[%d]): unknown pipeId", __FUNCTION__, __LINE__);
        break;
    }
    return request;
}

/*
 * ExynosCameraFrameEntity class
 */

ExynosCameraFrameEntity::ExynosCameraFrameEntity(
        uint32_t pipeId,
        entity_type_t type,
        entity_buffer_type_t bufType)
{
    m_pipeId = pipeId;

    if (m_setEntityType(type) != NO_ERROR)
        ALOGE("ERR(%s[%d]):setEntityType fail, pipeId(%d), type(%d)", __FUNCTION__, __LINE__, pipeId, type);

    m_bufferType = bufType;
    m_entityState = ENTITY_STATE_READY;

    m_prevEntity = NULL;
    m_nextEntity = NULL;
}

status_t ExynosCameraFrameEntity::m_setEntityType(entity_type_t type)
{
    status_t ret = NO_ERROR;

    m_EntityType = type;

    switch (type) {
    case ENTITY_TYPE_INPUT_ONLY:
        m_srcBufState = ENTITY_BUFFER_STATE_REQUESTED;
        m_dstBufState = ENTITY_BUFFER_STATE_NOREQ;
        break;
    case ENTITY_TYPE_OUTPUT_ONLY:
        m_srcBufState = ENTITY_BUFFER_STATE_NOREQ;
        m_dstBufState = ENTITY_BUFFER_STATE_REQUESTED;
        break;
    case ENTITY_TYPE_INPUT_OUTPUT:
        m_srcBufState = ENTITY_BUFFER_STATE_REQUESTED;
        m_dstBufState = ENTITY_BUFFER_STATE_REQUESTED;
        break;
    default:
        m_srcBufState = ENTITY_BUFFER_STATE_NOREQ;
        m_dstBufState = ENTITY_BUFFER_STATE_NOREQ;
        m_EntityType = ENTITY_TYPE_INVALID;
        ret = BAD_VALUE;
        break;
    }

    return ret;
}

uint32_t ExynosCameraFrameEntity::getPipeId(void)
{
    return m_pipeId;
}

status_t ExynosCameraFrameEntity::setSrcBuf(ExynosCameraBuffer buf)
{
    int ret = 0;

    if (m_bufferType != ENTITY_BUFFER_DELIVERY &&
        m_srcBufState != ENTITY_BUFFER_STATE_REQUESTED) {
        ALOGE("ERR(%s[%d]):Invalid buffer state(%d)", __FUNCTION__, __LINE__, m_srcBufState);
        return INVALID_OPERATION;
    }

    this->m_srcBuf = buf;

    ret = setSrcBufState(ENTITY_BUFFER_STATE_READY);

    return ret;
}

status_t ExynosCameraFrameEntity::setDstBuf(ExynosCameraBuffer buf)
{
    int ret = 0;

    if (m_bufferType != ENTITY_BUFFER_DELIVERY &&
        m_dstBufState != ENTITY_BUFFER_STATE_REQUESTED) {
        ALOGE("ERR(%s[%d]):Invalid buffer state(%d)", __FUNCTION__, __LINE__, m_dstBufState);
        return INVALID_OPERATION;
    }

    this->m_dstBuf = buf;

    ret = setDstBufState(ENTITY_BUFFER_STATE_READY);

    return ret;
}

status_t ExynosCameraFrameEntity::getSrcBuf(ExynosCameraBuffer *buf)
{
    *buf = this->m_srcBuf;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getDstBuf(ExynosCameraBuffer *buf)
{
    *buf = this->m_dstBuf;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setSrcRect(ExynosRect rect)
{
    this->m_srcRect = rect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setDstRect(ExynosRect rect)
{
    this->m_dstRect = rect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getSrcRect(ExynosRect *rect)
{
    *rect = this->m_srcRect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::getDstRect(ExynosRect *rect)
{
    *rect = this->m_dstRect;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setSrcBufState(entity_buffer_state_t state)
{
    m_srcBufState = state;
    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setDstBufState(entity_buffer_state_t state)
{
    m_dstBufState = state;
    return NO_ERROR;
}

entity_buffer_state_t ExynosCameraFrameEntity::getSrcBufState(void)
{
    return m_srcBufState;
}

entity_buffer_state_t ExynosCameraFrameEntity::getDstBufState(void)
{
    return m_dstBufState;
}

entity_buffer_type_t ExynosCameraFrameEntity::getBufType(void)
{
    return m_bufferType;
}

status_t ExynosCameraFrameEntity::setEntityState(entity_state_t state)
{
    this->m_entityState = state;

    return NO_ERROR;
}

entity_state_t ExynosCameraFrameEntity::getEntityState(void)
{
    return this->m_entityState;
}

ExynosCameraFrameEntity *ExynosCameraFrameEntity::getPrevEntity(void)
{
    return this->m_prevEntity;
}

ExynosCameraFrameEntity *ExynosCameraFrameEntity::getNextEntity(void)
{
    return this->m_nextEntity;
}

status_t ExynosCameraFrameEntity::setPrevEntity(ExynosCameraFrameEntity *entity)
{
    this->m_prevEntity = entity;

    return NO_ERROR;
}

status_t ExynosCameraFrameEntity::setNextEntity(ExynosCameraFrameEntity *entity)
{
    this->m_nextEntity = entity;

    return NO_ERROR;
}

}; /* namespace android */
