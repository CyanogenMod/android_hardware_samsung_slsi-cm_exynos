# Copyright (C) 2012 The Android Open Source Project
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
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.product.board>.so

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libEGL libGLESv1_CM libhardware \
	libhardware_legacy libion_exynos libutils libsync \
	libexynosv4l2 libMcClient libexynosutils libhwcutils libdisplay libhdmi \
	libdisplaymodule libhdmimodule libmpp

ifeq ($(BOARD_DYNAMIC_RECOMPOSITION_DISABLED), true)
	LOCAL_CFLAGS += -DDYNAMIC_RECOMPOSITION_DISABLED
endif

ifeq ($(BOARD_USES_FIMC), true)
        LOCAL_SHARED_LIBRARIES += libexynosfimc
else
        LOCAL_SHARED_LIBRARIES += libexynosgscaler
endif

ifeq ($(BOARD_USES_FIMC), true)
	LOCAL_CFLAGS += -DUSES_FIMC
endif

ifeq ($(BOARD_USES_IP_SERVICE), true)
	LOCAL_CFLAGS += -DIP_SERVICE
	LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/libipService
	LOCAL_SHARED_LIBRARIES += libExynosIPService
endif

ifeq ($(BOARD_USES_HWC_SERVICES),true)
	LOCAL_SHARED_LIBRARIES += libExynosHWCService
	LOCAL_CFLAGS += -DHWC_SERVICES
	LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/libhwcService

ifeq ($(BOARD_USES_WFD),true)
	LOCAL_CFLAGS += -DUSES_WFD
	LOCAL_SHARED_LIBRARIES += libfimg
	LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/exynos/libfimg4x
endif
endif

ifeq ($(BOARD_USES_VIRTUAL_DISPLAY), true)
	LOCAL_CFLAGS += -DUSES_VIRTUAL_DISPLAY
	LOCAL_SHARED_LIBRARIES += libfimg libvirtualdisplay libvirtualdisplaymodule
	LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/exynos/libfimg4x
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libvirtualdisplay
	LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libvirtualdisplaymodule
endif

ifeq ($(BOARD_USES_WFD_SERVICE),true)
	LOCAL_CFLAGS += -DUSES_WFD_SERVICE
	LOCAL_SHARED_LIBRARIES += libmedia
	LOCAL_C_INCLUDES += $(TOP)/frameworks/av/include/media
endif

ifeq ($(BOARD_USES_FB_PHY_LINEAR),true)
	LOCAL_CFLAGS += -DUSE_FB_PHY_LINEAR
	LOCAL_SHARED_LIBRARIES += libfimg
	LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/exynos/libfimg4x
endif

ifeq ($(BOARD_HDMI_INCAPABLE), true)
	LOCAL_CFLAGS += -DHDMI_INCAPABLE
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libhdmi_dummy
else
ifeq ($(BOARD_USES_NEW_HDMI), true)
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libhdmi
else
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libhdmi_legacy
endif
ifeq ($(BOARD_USES_CEC),true)
	LOCAL_SHARED_LIBRARIES += libcec-exynos
	LOCAL_CFLAGS += -DUSES_CEC
endif
endif

# Exynos 5430 onwards use a decon frame buffer device, but still have the
# old kernel APIs for calling it (S3C_FB_*).
# Newer SoCs (Exynos 7420 onwards) make use of a new kernel API.
# WARNING: Support is highly experimental!
ifneq ($(filter exynos7420 exynos7580 exynos8890, $(TARGET_SOC)),)
	LOCAL_CFLAGS += -DDECON_FB
endif

LOCAL_CFLAGS += -DLOG_TAG=\"hwcomposer\"

LOCAL_C_INCLUDES += \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../libhwcutils \
	$(LOCAL_PATH)/../libdisplay \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/include \
	$(TOP)/hardware/samsung_slsi-cm/exynos/libexynosutils \
	$(TOP)/hardware/samsung_slsi-cm/exynos/libcec \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/include \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libhwcmodule \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libdisplaymodule \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libhdmimodule \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_SOC)/libhwcutilsmodule \
	$(TOP)/hardware/samsung_slsi-cm/exynos/libmpp \
	$(TOP)/system/core/libsync/include

LOCAL_ADDITIONAL_DEPENDENCIES := \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SRC_FILES := ExynosHWC.cpp

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

