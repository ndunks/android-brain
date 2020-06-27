LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/main.c

LOCAL_CFLAGS += -Wno-multichar

LOCAL_MODULE:= android-brain

include $(BUILD_EXECUTABLE)
