ifndef ANDROID_SDK_ROOT
	$(error "ANDROID_SDK_ROOT not set")
endif
ifndef ANDROID_NDK_HOME
ANDROID_NDK_HOME=$(ANDROID_SDK_ROOT)/ndk
endif

PROJECT_PATH := $(PWD)
BUILD_PATH := $(PWD)/build
NDK_OUT := $(BUILD_PATH)/obj
NDK_LIBS_OUT := $(BUILD_PATH)/libs
APP_BUILD_SCRIPT  := $(PWD)/Android.mk
APP_ABI := armeabi-v7a

ANDROID_ARGS := \
	PROJECT_PATH=$(PROJECT_PATH) \
	APP_BUILD_SCRIPT=$(APP_BUILD_SCRIPT) \
	APP_ABI=$(APP_ABI) \
	NDK_OUT=$(NDK_OUT) \
	NDK_LIBS_OUT=$(NDK_LIBS_OUT) \
	PROJECT_PATH=$(PROJECT_PATH) \
	APP_PLATFORM=android-19 \
	NDK_PROJECT_PATH=null

ANDROID_BUILD := @make  --no-print-dir -f $(ANDROID_NDK_HOME)/build/core/build-local.mk $(ANDROID_ARGS)

build:
	$(ANDROID_BUILD)

clean:
	$(ANDROID_BUILD) clean
	@rm -rf $(BUILD_PATH)/* 2>/dev/null

.PHONY: build clean