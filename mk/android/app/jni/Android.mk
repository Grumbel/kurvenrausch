# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# libmain.so: the game, linked against the prebuilt SDL2 (see build-apk.sh,
# which puts src/*.cpp and include/ next to this file).
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := main
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(wildcard $(LOCAL_PATH)/*.cpp))
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/../SDL/include
LOCAL_CPPFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -O2 \
	-DKURVENRAUSCH_VERSION=\"$(KURVENRAUSCH_VERSION)\"
LOCAL_CPP_FEATURES := exceptions rtti
LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
