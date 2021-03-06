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

void Java_com_client_nativef_ClientNative_selecttank(JNIEnv *env, jobject thiz)
{
	select_tank();
}

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
	turret_left();
}

void Java_com_client_nativef_ClientNative_tright(JNIEnv *env, jobject thiz)
{
	turret_right();
}

void Java_com_client_nativef_ClientNative_telev(JNIEnv *env, jobject thiz)
{
	turret_elev();
}

void Java_com_client_nativef_ClientNative_fire(JNIEnv *env, jobject thiz)
{
	fire();
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

		jclass cls = (*env)->GetObjectClass(env, gs_obj);
		if (cls == 0) {
			break;
		}

		jmethodID med = (*env)->GetMethodID(env, cls,
					"handlePiMessage", "(Ljava/lang/String;)V");
		if (med == 0) {
			break;
		}
		(*env)->CallVoidMethod(env, gs_obj, med, msg);
		(*env)->DeleteLocalRef(env, msg);
	} while (0);

	if (env != NULL) {
		(*gs_jvm)->DetachCurrentThread(gs_jvm);
	}
}

int Java_com_client_nativef_ClientNative_sendcmd(JNIEnv *env, jobject thiz, jstring cmd)
{
    int err = 0;
	char *buf = (char*)(*env)->GetStringUTFChars(env, cmd, 0);
    err = client_send_cmd(buf, strlen(buf));
	(*env)->ReleaseStringUTFChars(env, cmd, buf);
    return err;
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
	return err;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv* env = NULL;
	jint result = -1;

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_4) != JNI_OK) {
		LOGE("GetEnv failed!");
		return result;
	}

	return JNI_VERSION_1_4;
}

JNIEXPORT void Java_com_client_nativef_ClientNative_setJNIEnv(JNIEnv *env, jobject obj, jobject handler)
{
	(*env)->GetJavaVM(env, &gs_jvm);
	gs_obj = (*env)->NewGlobalRef(env, handler);
}
