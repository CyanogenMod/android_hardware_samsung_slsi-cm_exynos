#
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
#

ifeq ($(TARGET_SLSI_VARIANT),cm)

common_exynos_dirs := \
	libstagefrighthw \
	libcsc \
	libv4l2 \
	libexynosutils \
	libfimg4x \
	libcec \
	libvideocodec \
	libmpp \
	libhwcutils \
	libdisplay

ifneq ($(BOARD_TV_PRIMARY), true)
common_exynos_dirs += \
	libhwc
endif

ifeq ($(BOARD_USES_VIRTUAL_DISPLAY), true)
common_exynos_dirs += \
	libvirtualdisplay
endif

ifeq ($(BOARD_USE_ALP_AUDIO), true)
ifeq ($(BOARD_USE_SEIREN_AUDIO), true)
common_exynos_dirs += \
	libseiren
else
common_exynos_dirs += \
	libsrp
endif
endif

ifeq ($(BOARD_HDMI_INCAPABLE), true)
common_exynos_dirs += libhdmi_dummy
else
ifeq ($(BOARD_USES_NEW_HDMI), true)
common_exynos_dirs += libhdmi
else
common_exynos_dirs += libhdmi_legacy
endif
endif

ifeq ($(BOARD_USES_FIMGAPI_V4L2), true)
common_exynos_dirs += \
	libg2d
endif

include $(call all-named-subdir-makefiles,$(common_exynos_dirs))

endif
