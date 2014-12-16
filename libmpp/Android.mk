# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libexynosutils libexynosv4l2

LOCAL_C_INCLUDES += \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(LOCAL_PATH)/../include \
	$(TOP)/hardware/samsung_slsi-cm/exynos/include \
	$(TOP)/hardware/samsung_slsi-cm/exynos/libexynosutils \
	$(TOP)/hardware/samsung_slsi-cm/exynos3/include \
	$(TOP)/hardware/samsung_slsi-cm/exynos4/include \
	$(TOP)/hardware/samsung_slsi-cm/exynos5/include

LOCAL_ADDITIONAL_DEPENDENCIES := \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SRC_FILES := MppFactory.cpp
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libmpp

ifeq ($(TARGET_BOARD_PLATFORM),exynos5)
LOCAL_SHARED_LIBRARIES += libexynosgscaler
LOCAL_CFLAGS += -DENABLE_GSCALER
else
LOCAL_SHARED_LIBRARIES += libexynosfimc
LOCAL_CFLAGS += -DENABLE_FIMC
endif
include $(BUILD_SHARED_LIBRARY)
