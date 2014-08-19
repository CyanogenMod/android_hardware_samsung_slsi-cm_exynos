LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := exyrngd
LOCAL_SRC_FILES := \
		exyrngd.c
LOCAL_SHARED_LIBRARIES := libc libcutils
#LOCAL_CFLAGS := -DANDROID_CHANGES
LOCAL_MODULE_TAGS := eng optional
include $(BUILD_EXECUTABLE)

