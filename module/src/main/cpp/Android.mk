LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := $(MODULE_NAME)
LOCAL_SRC_FILES := fucklos.cpp
LOCAL_STATIC_LIBRARIES := libcxx
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/external/libcxx/Android.mk
