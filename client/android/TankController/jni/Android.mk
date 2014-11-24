# ndk-build NDK_PROJECT_PATH=~/workspace/AntiVirus-jni/encrypt-jni/

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/lib/

LOCAL_MODULE    := tank-jni
LOCAL_SRC_FILES := client.c l298n.c tank-jni.c \
					./lib/sock.c ./lib/unix.c ./lib/xmalloc.c

# for logging
LOCAL_LDLIBS    += -llog -ldl

include $(BUILD_SHARED_LIBRARY)
