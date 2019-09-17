LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)
LOCAL_SRC_FILES:= \
	drmdemo.c

LOCAL_MODULE:= drmdemo

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libdrm \
    libkms \
	libpng \
	libcutils \
	liblog

include $(BUILD_EXECUTABLE)