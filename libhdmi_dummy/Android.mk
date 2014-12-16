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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libexynosutils libexynosv4l2 libhwcutils libdisplay libhwcutilsmodule

ifeq ($(BOARD_USES_HWC_SERVICES),true)
	LOCAL_CFLAGS += -DHWC_SERVICES
ifeq ($(BOARD_USE_S3D_SUPPORT),true)
	LOCAL_CFLAGS += -DS3D_SUPPORT
endif
endif

ifeq ($(BOARD_USES_GSC_VIDEO),true)
	LOCAL_CFLAGS += -DGSC_VIDEO
endif

ifeq ($(BOARD_HDMI_INCAPABLE), true)
	LOCAL_CFLAGS += -DHDMI_INCAPABLE
endif

ifeq ($(BOARD_USES_FIMC), true)
        LOCAL_CFLAGS += -DUSES_FIMC
endif

LOCAL_C_INCLUDES := \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../libhwcutils \
	$(LOCAL_PATH)/../libdisplay \
	$(LOCAL_PATH)/../libhwc \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/include \
	$(TOP)/hardware/samsung_slsi-cm/exynos/libexynosutils \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/include \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libhwcmodule \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libhwcutilsmodule \
	$(TOP)/hardware/samsung_slsi-cm/exynos/libmpp \
	$(TOP)/system/core/libsync/include

LOCAL_ADDITIONAL_DEPENDENCIES := \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SRC_FILES := \
	ExynosExternalDisplay.cpp

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libhdmi
include $(BUILD_SHARED_LIBRARY)

