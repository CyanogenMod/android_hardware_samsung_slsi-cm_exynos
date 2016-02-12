LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(BOARD_USE_VP8ENC_SUPPORT), true)
LOCAL_CFLAGS += -DUSE_VP8ENC_SUPPORT
endif

LOCAL_SRC_FILES := \
	ExynosVideoInterface.c \
	dec/ExynosVideoDecoder.c \
	enc/ExynosVideoEncoder.c

LOCAL_C_INCLUDES := \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(LOCAL_PATH)/include \
	$(TOP)/hardware/samsung_slsi-cm/exynos/include \
	$(TOP)/hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/include

LOCAL_ADDITIONAL_DEPENDENCIES += \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi-cm/openmax/include/khronos
else
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include/media/openmax
endif

ifeq ($(BOARD_USE_HEVCDEC_SUPPORT), true)
LOCAL_CFLAGS += -DUSE_HEVCDEC_SUPPORT
endif

ifeq ($(BOARD_USE_HEVC_HWIP), true)
LOCAL_CFLAGS += -DUSE_HEVC_HWIP
endif

ifeq ($(TARGET_SOC), exynos5430)
LOCAL_CFLAGS += -DSOC_EXYNOS5430
endif

ifeq ($(TARGET_SOC), exynos5433)
LOCAL_CFLAGS += -DSOC_EXYNOS5430
endif

ifeq ($(TARGET_SOC), exynos7420)
LOCAL_CFLAGS += -DSOC_EXYNOS5430
endif

LOCAL_MODULE := libExynosVideoApi
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm

include $(BUILD_STATIC_LIBRARY)
