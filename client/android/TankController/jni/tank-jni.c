/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <jni.h>

#include <android/log.h>

#include "client.h"
#define LOGE(msg) __android_log_print(ANDROID_LOG_ERROR, "tank",msg)

extern void up(void);
extern void down(void);
extern void left(void);
extern void right(void);
extern void speed_up(void);
extern void speed_down(void);
extern int connect_stream(const char *hostname, unsigned short portno);


void Java_com_tank_nativef_TankNative_up(JNIEnv* env, jobject thiz) {
	up();
}

void Java_com_tank_nativef_TankNative_down(JNIEnv* env, jobject thiz) {
	down();
}

void Java_com_tank_nativef_TankNative_left(JNIEnv* env, jobject thiz) {
	left();
}

void Java_com_tank_nativef_TankNative_right(JNIEnv* env, jobject thiz) {
	right();
}

void Java_com_tank_nativef_TankNative_brake(JNIEnv* env, jobject thiz) {
	brake();
}

void Java_com_tank_nativef_TankNative_speedup(JNIEnv* env, jobject thiz) {
	speed_up();
}

void Java_com_tank_nativef_TankNative_speeddown(JNIEnv* env, jobject thiz) {
	speed_down();
}

int Java_com_tank_nativef_TankNative_connect(JNIEnv* env, jobject thiz, jstring host, int port) {
	char *hostname = (char*)(*env)->GetStringUTFChars(env, host, 0);
	int err = 0;
	if(hostname) {
		err = connect_stream(hostname, (unsigned short)port);
		(*env)->ReleaseStringUTFChars(env, host, hostname);
	}
	return err;
}
