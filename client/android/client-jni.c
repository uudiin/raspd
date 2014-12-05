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

#define LOGE(msg) __android_log_print(ANDROID_LOG_ERROR, "client", msg)

extern void up(void);
extern void down(void);
extern void left(void);
extern void right(void);
extern void speed_up(void);
extern void speed_down(void);
extern int client_connect(const char *hostname, unsigned short portno);


void Java_com_client_nativef_ClientNative_up(JNIEnv *env, jobject thiz)
{
	up();
}

void Java_com_client_nativef_ClientNative_down(JNIEnv *env, jobject thiz)
{
	down();
}

void Java_com_client_nativef_ClientNative_left(JNIEnv *env, jobject thiz)
{
	left();
}

void Java_com_client_nativef_ClientNative_right(JNIEnv *env, jobject thiz)
{
	right();
}

void Java_com_client_nativef_ClientNative_brake(JNIEnv *env, jobject thiz)
{
	brake();
}

void Java_com_client_nativef_ClientNative_speedup(JNIEnv *env, jobject thiz)
{
	speed_up();
}

void Java_com_client_nativef_ClientNative_speeddown(JNIEnv *env, jobject thiz)
{
	speed_down();
}

void Java_com_client_nativef_ClientNative_tleft(JNIEnv *env, jobject thiz)
{
}

void Java_com_client_nativef_ClientNative_tright(JNIEnv *env, jobject thiz)
{
}

void Java_com_client_nativef_ClientNative_telev(JNIEnv *env, jobject thiz)
{
}

void Java_com_client_nativef_ClientNative_fire(JNIEnv *env, jobject thiz)
{
}

static JavaVM *gs_jvm = NULL;
static jobject gs_obj = NULL;

jstring string_to_jstring(JNIEnv *env, char *buf, int buflen)
{
	jstring msg;
	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	jmethodID ctorID = (*env)->GetMethodID(env, strClass, "<init>", "([BLjava/lang/String;)V");
	jbyteArray bytes = (*env)->NewByteArray(env, buflen);
	(*env)->SetByteArrayRegion(env, bytes, 0, buflen, (jbyte*)buf);
	jstring encoding = (*env)->NewStringUTF(env, "utf-8");
	return (jstring)(*env)->NewObject(env, strClass, ctorID, bytes, encoding);
}

jobject getInstance(JNIEnv *env, jclass obj_class)
{
	jmethodID construction_id = (*env)->GetMethodID(env, obj_class, "<init>", "()V");
	jobject obj = (*env)->NewObject(env, obj_class, construction_id);
	return obj;  
}

void cbrecv(char *buf, int buflen)
{
	JNIEnv *env = NULL;
	int err;

	if (gs_jvm == NULL || gs_obj == NULL)
		return;

	do {

		err = (*gs_jvm)->AttachCurrentThread(gs_jvm, &env, NULL);
		if (err < 0)
			break;

		jstring msg = string_to_jstring(env, buf, buflen);

		jclass java_class = (*env)->GetObjectClass(env, gs_obj);
		if (java_class == 0) {
			break;
		}

		jmethodID java_method = (*env)->GetMethodID(
						env, java_class, "DisplayOutput", "(Ljava/lang/String;)V");
		if (java_method == 0) {
			break;
		}
		(*env)->CallVoidMethod(env, java_class, java_method, 0);
	} while (0);

	if (env != NULL) {
		(*gs_jvm)->DetachCurrentThread(gs_jvm);
	}
}

int Java_com_client_nativef_ClientNative_connect(JNIEnv *env, jobject thiz, jstring host, int port)
{
	char *hostname = (char*)(*env)->GetStringUTFChars(env, host, 0);
	int err = 0;
	if (hostname == NULL) {
		return -2;
	}
	err = client_connect(hostname, (unsigned short)port);
	client_msg_dispatch(cbrecv);
	(*env)->ReleaseStringUTFChars(env, host, hostname);
	(*env)->GetJavaVM(env, &gs_jvm);
	gs_obj = (*env)->NewGlobalRef(env, thiz);
	return err;
}
